/*
 *  (C)2015, 2018 John Greb
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licence version 3.
 */

#include <stdio.h>
#include <stdint.h>
#include <alsa/asoundlib.h>

snd_pcm_t *OpenSource()
{
	int fail = 0;
	snd_pcm_hw_params_t* params;
	snd_pcm_t* fcd_handle = NULL;
	snd_pcm_stream_t fcd_stream = SND_PCM_STREAM_CAPTURE;

	if ( snd_pcm_open( &fcd_handle, "hw:CARD=V20", fcd_stream, 0 ) < 0 )
		return NULL;

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
		printf("Funcube Dongle stream start failed");
		snd_pcm_close( fcd_handle );
		return NULL;
	} else {
		printf("Funcube stream started");
	}
	return fcd_handle;
}

void CloseSource(snd_pcm_t *fcd_handle)
{
	if (fcd_handle)
		snd_pcm_close( fcd_handle );
}

#define BLOCKSIZE 8000
int work(snd_pcm_t *fcd_handle)
{
	int l;
	int16_t it[BLOCKSIZE * 2];
	void *out;

	out = (void *)&it[0];
	l = snd_pcm_mmap_readi(fcd_handle, out, (snd_pcm_uframes_t)BLOCKSIZE);
	if (l <= 0)
		return 0;


	return l; // until signal caught
}

int main()
{
	snd_pcm_t *fcd_handle;

	fcd_handle = OpenSource();
	if (!fcd_handle)
		return -22;
	while ( work( fcd_handle ) )
		;
	CloseSource( fcd_handle );
	return 0;
}
