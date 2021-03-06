/*
 * Copyright (C) 2015, 2018 John Greb
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licence version 3.
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <alsa/asoundlib.h>

static snd_pcm_t *fcd_handle;
int OpenSource()
{
	int fail;
	snd_pcm_hw_params_t* params;
	snd_pcm_stream_t fcd_stream = SND_PCM_STREAM_CAPTURE;

	fail = snd_pcm_open( &fcd_handle, "hw:CARD=V20", fcd_stream, 0 );
	if (fail < 0 ) {
		fprintf(stderr, "Failed to open Funcube Dongle with error: %d\n", fail);
		return fail;
	}

	snd_pcm_hw_params_alloca(&params);
	if ( snd_pcm_hw_params_any(fcd_handle, params) < 0 )
		fail++;
	else
		if ( snd_pcm_hw_params(fcd_handle, params) < 0 )
			fail++;
		else
			if ( snd_pcm_start(fcd_handle) < 0 )
				fail++;

	if (fail) {
		fprintf(stderr, "Funcube Dongle stream start failed\n");
		snd_pcm_close( fcd_handle );
		return -1;
	} else {
		fprintf(stderr, "Funcube stream started\n");
	}
	return 0;
}

void CloseSource()
{
	snd_pcm_close( fcd_handle );
}

void downsample(int16_t* buf, float *I, float *Q, int len)
{
	// Offset tuning to remove dc spike: tune 48kHz high
	// 4x downsample and rotate by one quarter.
	// rotate:  (0, 1), (-3, 2), (-4, -5), (7, -6)
	// downsample: (0 -3 -4 +7), (1 +2 -5 -6])
	int i, pos;

	for (i = pos = 0; pos < len - 7; pos += 8) {
		I[i] = (float)(buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]);
		Q[i] = (float)(buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]);
		i++;
	}
}

#define RESCALE (24000.0f / 3.14159265f)
static float dc = 0.0f;
static float lastI = 0.1f;
static float lastQ = 0.1f;
void demodulate(int16_t audio[], float I[], float Q[], int len)
{
	float cr, cj, angle, sum;

	sum = 0.0f;
	for (int i = 0; i < len; i++) {
		cr = I[i] * lastI + Q[i] * lastQ;
		cj = Q[i] * lastI - I[i] * lastQ;
		angle = atan2f(cj, cr);
		sum += angle;
		audio[i] = (int16_t)((angle - dc ) * RESCALE);
		lastI = I[i];
		lastQ = Q[i];
	}
	dc = 0.9f * dc + 0.1 * sum / len;
}

#define BLOCKSIZE 4000
int work()
{
	int l;
	int16_t audio192[BLOCKSIZE * 4 * 2];
	void *out;
	float i48khz[BLOCKSIZE];
	float q48khz[BLOCKSIZE];
	int16_t *audio48 = audio192;

	out = (void *)audio192;
	l = snd_pcm_mmap_readi(fcd_handle, out, (snd_pcm_uframes_t)BLOCKSIZE * 4);
	if (l <= 0)
		return 0;
	downsample(audio192, i48khz, q48khz, l << 1); // double length for IQ stereo input
	// TODO: low pass filter - only need 12 kHz for nfm
	demodulate(audio48, i48khz, q48khz, l >> 2); // quarter rate for 192k in / 48k out
	fwrite(audio48, 2, l >> 2, stdout);

	return l; // until signal caught
}

void writewavheader(FILE *outfile)
{
	char wavhead[] = {
	0x52,0x49, 0x46,0x46, 0x64,0x19, 0xff,0x7f, 0x57,0x41, 0x56,0x45, 0x66,0x6d, 0x74,0x20,
	0x10,0x00, 0x00,0x00, 0x01,0x00, 0x01,0x00, 0x80,0xbb, 0x00,0x00, 0x00,0x77, 0x01,0x00,
	0x02,0x00, 0x10,0x00, 0x64,0x61, 0x74,0x61, 0x40,0x19, 0xff,0x7f, 0x00,0x00, 0x00,0x00
	};
	fwrite(wavhead, 2, sizeof(wavhead), outfile);
}

volatile int stopped = 0;
void sighandler(int signum)
{
	if (!stopped)
		fprintf(stderr, "Signal %d caught, exiting!\n", signum);
	stopped = 1;
}

int main()
{
	struct sigaction sigact;

	if ( OpenSource() )
		return -ENODEV;

	writewavheader(stdout);

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);

	while ( work() && !stopped )
		;
	CloseSource();
	return 0;
}
