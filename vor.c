/*
 *  Copyright (c) 2017 Thierry Leconte (f4dwv)
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <getopt.h>
#include <math.h>
#include <complex.h>
#include "vortrack.h"

extern int interval;

typedef struct {
 complex double xv[8], yv[8];
} filterstate_t;

static complex double filter510(complex double V , filterstate_t *st) {
	st->xv[0] = st->xv[1]; st->xv[1] = st->xv[2]; st->xv[2] = st->xv[3]; st->xv[3] = st->xv[4]; 
        st->xv[4] = V ;
        st->yv[0] = st->yv[1]; st->yv[1] = st->yv[2]; st->yv[2] = st->yv[3]; st->yv[3] = st->yv[4]; 
        st->yv[4] =   (st->xv[0] + st->xv[4]) +   1.3724127962 * (st->xv[1] + st->xv[3]) +   0.7448255925 * st->xv[2]
                     + ( -0.9133512299 * st->yv[2]) + (  1.9094231878 * st->yv[3]);
        return(st->yv[4]);
}

static complex double filterlow(complex double V, filterstate_t *st) {

	st->xv[0] = st->xv[1]; st->xv[1] = st->xv[2]; st->xv[2] = st->xv[3]; st->xv[3] = st->xv[4];
        st->xv[4] = V ;
        st->yv[0] = st->yv[1]; st->yv[1] = st->yv[2]; st->yv[2] = st->yv[3]; st->yv[3] = st->yv[4];
        st->yv[4] =   (st->xv[0] + st->xv[4]) +   0.0000142122 * st->xv[1] +   0.0000142122 * st->xv[3]
                     -   1.9999715756 * st->xv[2]
                     + ( -0.9972352026 * st->yv[2]) + (  1.9972326511 * st->yv[3]);
        return(st->yv[4]);
}

/* Morse code table for letters A-Z and digits 0-9 */
static const char *morse_table[36] = {
	/* A */ ".-",   /* B */ "-...", /* C */ "-.-.", /* D */ "-..",
	/* E */ ".",    /* F */ "..-.", /* G */ "--.",  /* H */ "....",
	/* I */ "..",   /* J */ ".---", /* K */ "-.-",  /* L */ ".-..",
	/* M */ "--",   /* N */ "-.",   /* O */ "---",  /* P */ ".--.",
	/* Q */ "--.-", /* R */ ".-.",  /* S */ "...",  /* T */ "-",
	/* U */ "..-",  /* V */ "...-", /* W */ ".--",  /* X */ "-..-",
	/* Y */ "-.--", /* Z */ "--..",
	/* 0 */ "-----",/* 1 */ ".----",/* 2 */ "..---",/* 3 */ "...--",
	/* 4 */ "....-",/* 5 */ ".....",/* 6 */ "-....",/* 7 */ "--...",
	/* 8 */ "---..",/* 9 */ "----."
};

static char morse_lookup(const char *sym)
{
	for (int i = 0; i < 36; i++) {
		if (strcmp(sym, morse_table[i]) == 0)
			return (i < 26) ? ('A' + i) : ('0' + i - 26);
	}
	return '?';
}

static char last_ident[32] = "";

extern int verbose;

static void morse_decode(float S)
{
	static double mphase = 0;
	static complex double iq_lp = 0;
	static double env = 0;
	static double peak = 0;
	static double floor_lvl = 0;
	static int tone_state = 0;
	static int raw_state = 0;
	static int stable_count = 0;
	static int duration = 0;
	static int init_count = 0;
	static char symbols[8];
	static int sym_idx = 0;
	static char ident[32];
	static int ident_idx = 0;

	const int DEBOUNCE = FSINT * 30 / 1000;   /* 30 ms */
	const int END_OF_IDENT = FSINT * 3 / 2;   /* 1.5 s */

	const double W1020 = 2.0 * M_PI * 1020 / FSINT;

	mphase += W1020;
	if (mphase > M_PI) mphase -= 2.0 * M_PI;

	/* Heterodyne 1020 Hz tone to DC, 1-pole IIR lowpass (~25 Hz cutoff) */
	complex double mixed = S * cexp(-I * mphase);
	iq_lp = 0.997 * iq_lp + 0.003 * mixed;
	double mag = cabs(iq_lp);

	/* Smooth envelope a bit (~50 Hz) */
	env = 0.995 * env + 0.005 * mag;

	/* Track slow noise floor and decaying peak */
	if (init_count < FSINT / 2) {
		/* Prime trackers for first 0.5 s */
		floor_lvl = env;
		peak = env;
		init_count++;
		return;
	}
	if (env < floor_lvl) floor_lvl = 0.5 * floor_lvl + 0.5 * env;
	else floor_lvl = 0.99999 * floor_lvl + 0.00001 * env;

	if (env > peak) peak = env;
	else peak = 0.99998 * peak + 0.00002 * env;

	/* Require some SNR before trusting state detection */
	if (peak < floor_lvl * 1.8) {
		tone_state = 0;
		duration++;
		return;
	}

	double threshold = floor_lvl + 0.35 * (peak - floor_lvl);
	double hyst = 0.1 * (peak - floor_lvl);
	int new_raw = raw_state
	    ? (env > threshold - hyst)
	    : (env > threshold + hyst);

	if (new_raw != raw_state) {
		raw_state = new_raw;
		stable_count = 0;
	}
	stable_count++;
	duration++;

	/* Confirm a state change only after raw has been stable for DEBOUNCE samples.
	   This rejects short noise spikes during silences and short dropouts during tones. */
	if (raw_state != tone_state && stable_count >= DEBOUNCE) {
		double ms = 1000.0 * (duration - DEBOUNCE) / FSINT;
		if (tone_state) {
			/* tone just ended -> dit or dah */
			if (sym_idx < 7)
				symbols[sym_idx++] = (ms < 200.0) ? '.' : '-';
		} else {
			/* silence just ended -> flush accumulated letter on letter gap */
			if (ms > 200.0 && sym_idx > 0) {
				symbols[sym_idx] = 0;
				char c = morse_lookup(symbols);
				if (ident_idx < 31) ident[ident_idx++] = c;
				sym_idx = 0;
			}
		}
		if (verbose)
			fprintf(stderr, "morse: %s %.0f ms  (env=%.4f peak=%.4f flr=%.4f)\n",
			        tone_state ? "TONE" : "GAP ", ms, env, peak, floor_lvl);
		tone_state = raw_state;
		duration = DEBOUNCE;   /* already been in new state this long */
	}

	/* long silence => end of ident transmission (1.5 s delimits ident repeats) */
	if (!tone_state && duration > END_OF_IDENT && (ident_idx > 0 || sym_idx > 0)) {
		if (sym_idx > 0) {
			symbols[sym_idx] = 0;
			char c = morse_lookup(symbols);
			if (ident_idx < 31) ident[ident_idx++] = c;
			sym_idx = 0;
		}
		ident[ident_idx] = 0;
		/* require at least 2 letters to reject single-dit noise idents */
		if (ident_idx >= 2)
			strncpy(last_ident, ident, sizeof(last_ident) - 1);
		ident_idx = 0;
	}
}

void vor(float S)
{
	static filterstate_t flt_r;
	static filterstate_t flt_s;
	static filterstate_t flt_f;
	static double phase=0,sum=0,pA,uw=0;
	static complex double fpr;
	static int n=-FSINT/10;

	double A,fp,F;
	complex double ref30,fmcar,sig30;

	morse_decode(S);

	const double W30=2.0*M_PI*30/FSINT;

	phase+=W30;
	if(phase>M_PI) phase-=2.0*M_PI;

	ref30=cexp(phase*-I)*S;	
	ref30=filterlow(ref30,&flt_r);

	fmcar=filter510(cexp(9960/30*phase*-I)*S,&flt_f);
	F=carg(fmcar*conj(fpr));
	fpr=fmcar;
	if(F>2.0*M_PI*510/FSINT) F=2.0*M_PI*510/FSINT;
	if(F<-2.0*M_PI*510/FSINT) F=-2.0*M_PI*510/FSINT;

	sig30=cexp(phase*-I)*F;
	sig30=filterlow(sig30,&flt_s);

	A=carg(sig30*conj(ref30))+26*2.0*M_PI*30/FSINT;
	if(n>0) {
		if((A-pA)>M_PI) uw-=2.0*M_PI;
		if((A-pA)<-M_PI) uw+=2.0*M_PI;
		sum+=A+uw;
	}
	pA=A;

	n++;
	if(n>interval*FSINT) {
		double avg=fmod(180.0/M_PI*sum/n,360.0);
		if(avg<0) avg+=360;
		printf("%5.1f  ID:%s\n",avg,last_ident);
		n=0;sum=0;
	}
}	


