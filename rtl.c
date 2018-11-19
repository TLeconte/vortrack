/*
 *  Copyright (c) 2014 Thierry Leconte (f4dwv)
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/resource.h>
#include <math.h>
#include <complex.h>

#include <rtl-sdr.h>
#include "vortrack.h"

#define INRATE 2000000
#define IFFREQ 50000
#define DOWNSC (INRATE/FSINT)

#define INBUFSZ (DOWNSC*2048)

extern int ppm;
extern int verbose;
extern int gain;

extern void vor(float V);

static rtlsdr_dev_t *dev = NULL;

complex float Osc[DOWNSC];

static int nearest_gain(int target_gain)
{
	int i, err1, err2, count, close_gain;
	int *gains;
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = malloc(sizeof(int) * count);
	count = rtlsdr_get_tuner_gains(dev, gains);
	close_gain = gains[0];
	for (i = 0; i < count; i++) {
		err1 = abs(target_gain - close_gain);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			close_gain = gains[i];
		}
	}
	free(gains);
	if (verbose)
		fprintf(stderr, "Tuner gain : %f\n", (float)close_gain / 10.0);
	return close_gain;
}

int initRtl(int dev_index, int fr)
{
	int i, r, n;

	n = rtlsdr_get_device_count();
	if (!n) {
		fprintf(stderr, "No supported devices found.\n");
		return -1;
	}

	if (verbose)
		fprintf(stderr, "Using device %d: %s\n",
			dev_index, rtlsdr_get_device_name(dev_index));

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device\n");
		return r;
	}

	rtlsdr_set_tuner_gain_mode(dev, 1);	/* no agc */
	r = rtlsdr_set_tuner_gain(dev, nearest_gain(gain));
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to set gain.\n");

	if (ppm != 0) {
		r = rtlsdr_set_freq_correction(dev, ppm);
		if (r < 0)
			fprintf(stderr,
				"WARNING: Failed to set freq. correction\n");
	}

	r = rtlsdr_set_center_freq(dev, fr - IFFREQ);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
	}

	r = rtlsdr_set_sample_rate(dev, INRATE);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	}

	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");
	}

	for (i = 0; i < DOWNSC; i++) {
		Osc[i] =
		    cexpf(-I * i * 2 * M_PI * (float)IFFREQ / (float)INRATE);
	}

	return 0;
}

static void in_callback(unsigned char *rtlinbuff, unsigned int nread, void *ctx)
{
	static int idx = 0;
	static complex float D = 0;

	unsigned int i;

	if (nread == 0) {
		return;
	}

	for (i = 0; i < nread;) {
		float Is, Qs;

		Is = (float)rtlinbuff[i++] - 127.5;
		Qs = (float)rtlinbuff[i++] - 127.5;

		D += (Is + Qs * I) * Osc[idx];
		idx++;

		if (idx == DOWNSC) {
			vor(cabs(D) / (float)DOWNSC / 128.0);
			idx = 0;
			D = 0;
		}
	}
}

int runRtlSample(void)
{
	int r;

	r = rtlsdr_read_async(dev, in_callback, NULL, 8, INBUFSZ);
	return r;
}
