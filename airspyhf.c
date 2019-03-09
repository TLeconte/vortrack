#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <complex.h>
#include <libairspyhf/airspyhf.h>
#include <assert.h>
#include "vortrack.h"

#define INRATE 768000
#define IFFREQ 50000
// FSINT is 50000
#define DOWNSC (INRATE/FSINT)

extern int verbose;
extern int gain;

#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

extern void vor(float V);

static struct airspyhf_device *device = NULL;

// fractional downsampling of 50/768 = 25/384 is required
// 384*25 = 9600
#define CYCLE_INPUT (25)
#define CYCLE_OUTPUT (384)
#define CYCLE_IO (CYCLE_INPUT * CYCLE_OUTPUT)

static complex float V = 0;
static complex float D = 0;
static complex float D_old = 0;
static complex float Values[CYCLE_IO];
static int idx_input = 0;
static int idx_ds = 0;

static int rx_callback(airspyhf_transfer_t * transfer)
{
	int i, j, k;
	float *p = (float *)(transfer->samples);

	for (i = 0; i < transfer->sample_count; i++) {
		float re, im;

		re = p[i * 2];
		im = p[(i * 2) + 1];

		// Fs/4 downsampler
		switch(idx_ds) {
			case 0:
			    D = CMPLXF(re, im);
			    idx_ds = 1;
			    break;
			case 1:
			    D = CMPLXF(im, -re);
			    idx_ds = 2;
			    break;
			case 2:
			    D = CMPLXF(-re, -im);
			    idx_ds = 3;
			    break;
			case 3:
			    D = CMPLXF(-im, re);
			    idx_ds = 0;
			    break;
			default:
			// unreachable, error here;
			    assert(idx_ds < 4);
			    break;
    		}	

		// linear interpolation
		for (j = 0; j < CYCLE_INPUT; j++) {
			complex float val =
				D + ((D - D_old) * j / CYCLE_INPUT);
			Values[idx_input + j] = val;
		}
		idx_input += CYCLE_INPUT;
		D_old = D;

		if (idx_input == CYCLE_IO) {
			for (k = 0; k < CYCLE_INPUT; k++) {
			  double input_val;
			  input_val = cabs(Values[k * CYCLE_OUTPUT]);
			  vor(input_val / (float)DOWNSC);
			}
			idx_input = 0;
		}

	}

	return 0;
}

int init_airspyhf(int freq)
{
	int result;
	uint32_t i, count;
	uint32_t *supported_samplerates;
	int Fc;

	result = airspyhf_open(&device);
	if (result != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "airspyhf_open() failed (%d)\n",
			result);
		return -1;
	}

	airspyhf_get_samplerates(device, &count, 0);
	supported_samplerates = (uint32_t *) malloc(count * sizeof(uint32_t));
	airspyhf_get_samplerates(device, supported_samplerates, count);
	for (i = 0; i < count; i++)
		if (supported_samplerates[i] == INRATE)
			break;
	if (i >= count) {
		fprintf(stderr, "did not find required sampling rate\n");
		airspyhf_close(device);
		return -1;
	}
	free(supported_samplerates);

	result = airspyhf_set_samplerate(device, i);
	if (result != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "airspyhf_set_samplerate() failed (%d)\n",
			result);
		airspyhf_close(device);
		return -1;
	}

	Fc = freq + IFFREQ - INRATE/4;
	result = airspyhf_set_freq(device, Fc);
	if (result != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "airspyhf_set_freq() failed (%d)\n",
			result);
		airspyhf_close(device);
		return -1;
	}

	return 0;
}

int runAirspyHF(void)
{
	int result;

	result = airspyhf_start(device, rx_callback, NULL);
	if (result != AIRSPYHF_SUCCESS) {
		fprintf(stderr, "airspyhf_start() failed (%d)\n",
			result);
		return -1;
	}

	do {
		sleep(1);
	} while (airspyhf_is_streaming(device) == true);

	return 0;
}
