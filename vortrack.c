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
int interval=2;
int freq ;

#if (WITH_RTL)
int initRtl(int dev_index, int fr);
int runRtlSample(void);
int devid = 0;
int ppm = 0;
int gain = 1000;
#endif
#ifdef WITH_AIRSPY
int init_airspy(int freq);
int runAirspy(void);
int gain = 18;
#endif
#ifdef WITH_AIRSPYHF
int init_airspyhf(int freq);
int runAirspyHF(void);
#endif


static void sighandler(int signum);

static void usage(void)
{
	fprintf(stderr,
		"vor receiver Copyright (c) 2018 Thierry Leconte \n\n");
#ifdef WITH_AIRSPYHF
	fprintf(stderr, "Usage: vortrack [-l interval ] frequency in MHz\n");
#endif
#ifdef WITH_AIRSPY
	fprintf(stderr, "Usage: vortrack [-g gain] [-l interval ] frequency in MHz\n");
	fprintf(stderr, " -g gain :\t\t\tlinearity gain [0-21] default 18\n");
#endif
#if WITH_RTL
	fprintf(stderr, "Usage: vortrack [-g gain] [-l interval ] [-p ppm] [-r device] frequency in MHz\n\n");
	fprintf(stderr, " -g gain :\t\t\tgain in tenth of db (ie : 500 = 50 db)\n");
	fprintf(stderr, " -p ppm :\t\t\tppm freq shift\n");
	fprintf(stderr, " -r n :\t\t\trtl device number\n");
#endif
	fprintf(stderr, " -l interval :\t\t\ttime between two measurements\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int i, c;
	struct sigaction sigact;

	while ((c = getopt(argc, argv, "vg:l:p:r:h")) != EOF) {
		switch ((char)c) {
		case 'v':
			verbose = 1;
			break;
		case 'l':
			interval = atoi(optarg);
			break;
#ifdef WITH_AIRSPY
		case 'g':
			gain = atoi(optarg);
			break;
#endif
#ifdef WITH_RTL
		case 'p':
			ppm = atoi(optarg);
			break;
		case 'r':
			devid = atoi(optarg);
			break;
#endif
		case 'h':
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

#ifdef WITH_AIRSPYHF
	if (init_airspyhf(freq))
		exit(-1);
	runAirspyHF();
#endif
	sighandler(0);
	exit(0);

}


static void sighandler(int signum)
{
	exit(0);
}
