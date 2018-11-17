/*
 *  Copyright (c) 2017 Thierry Leconte (f4dwv)
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <signal.h>
#include <getopt.h>
#include <math.h>
#include <complex.h>
#include "vortrack.h"

int verbose = 0;
int gain = 1000;
int freq ;

#if (WITH_RTL)
int initRtl(int dev_index, int fr);
int runRtlSample(void);
int devid = 0;
int ppm = 0;
#endif
#ifdef WITH_AIRSPY
int init_airspy(int freq);
int runAirspy(void);
#endif

static int interval=1;

static void sighandler(int signum);

static void usage(void)
{
	fprintf(stderr,
		"vor receiver Copyright (c) 2018 Thierry Leconte \n\n");
	fprintf(stderr,
		"Usage: vortrack [-g gain] [-l interval ] [-p ppm] [-r device] frequency (in Mhz\n");
	fprintf(stderr, "\n\n");
	fprintf(stderr, " -g gain :\t\t\tgain in tenth of db (ie : 500 = 50 db)\n");
	fprintf(stderr, " -l interval :\t\t\ttime between two measurements\n");
#if WITH_RTL
	fprintf(stderr, " -p ppm :\t\t\tppm freq shift\n");
	fprintf(stderr, " -r n :\t\t\trtl device number\n");
#endif
	exit(1);
}

int main(int argc, char **argv)
{
	int i, c;
	struct sigaction sigact;

	while ((c = getopt(argc, argv, "vg:l:p:r:")) != EOF) {
		switch ((char)c) {
		case 'v':
			verbose = 1;
			break;
		case 'l':
			interval = atoi(optarg);
			break;
		case 'g':
			gain = atoi(optarg);
			break;
#ifdef WITH_RTL
		case 'p':
			ppm = atoi(optarg);
			break;
		case 'r':
			devid = atoi(optarg);
			break;
#endif
		default:
			usage();
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "need frequency\n");
		exit(-2);
	}
	freq = (int)(atof(argv[optind]) * 1000000.0);

	if(freq<108000000 || freq > 118000000) {
		fprintf(stderr, "invalid frequency\n");
		exit(-2);
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

#if (WITH_RTL)
	if (initRtl(devid, freq))
		exit(-1);
	runRtlSample();
#endif

#ifdef WITH_AIRSPY
	if (init_airspy(freq))
		exit(-1);
	runAirspy();
#endif
	sighandler(0);
	exit(0);

}


static void sighandler(int signum)
{
	exit(0);
}

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

void demod(complex float V)
{
	static filterstate_t flt_r;
	static filterstate_t flt_s;
	static filterstate_t flt_f;
	static double phase=0,sum=0;
	static complex double fpr=1;
	static int n=0;

	const double W30=2.0*M_PI*30/FSINT;

	double S,fp,F,A;
	complex double ref30,fmcar,sig30;

	S=cabsf(V);

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

	A=carg(sig30*conj(ref30));

	sum+=A;
	n++;
	if(n>interval*FSINT) {
		double avg=180.0/M_PI*sum/n;
		if(avg<0) avg+=360.0;
		printf("%3.0f\n",avg);
		n=0;sum=0;
	}
}	


