/*
	OnsetsDS - real time musical onset detection library.
    Copyright (c) 2007 Dan Stowell. All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "onsetsdshelpers.h"

#include <stdlib.h>
#include <sndfile.h>
#include <fftw3.h>

void onsetsds_init_audiodata(OnsetsDSAudioBuf *odsbuf, OnsetsDS *ods, /* size_t framesize, */ size_t hopsize){
	
	odsbuf->ods    = ods;
	odsbuf->buflen = ods->fftsize;
	odsbuf->hopsize = hopsize;
	
	size_t framesizebytes = ods->fftsize * sizeof(float);
	
	// malloc odsbuf.data, odsbuf.window, odsbuf.windoweddata
	odsbuf->data         = (float*) malloc(framesizebytes);
	odsbuf->window       = (float*) malloc(framesizebytes);
	odsbuf->windoweddata = (float*) fftwf_malloc(framesizebytes);
	odsbuf->fftbuf       = (float*) fftwf_malloc(framesizebytes);
	
	// Create the FFTW plan
	odsbuf->fftplan = fftwf_plan_r2r_1d(ods->fftsize, odsbuf->windoweddata, odsbuf->fftbuf, FFTW_R2HC, FFTW_ESTIMATE);
	
	// zero odsbuf.data
	memset(odsbuf->data, 0, framesizebytes);
	
	// Create the FFT window
	double pi     = acos(-1.);
	double winc = pi / ods->fftsize;
	int i;
	for (i=0; i<ods->fftsize; ++i) {
		double w = i * winc;
		odsbuf->window[i] = sin(w);
	}
	
	odsbuf->sampsElapsed = 0L;
	odsbuf->writepos = 0;
}
void onsetsds_destroy_audiodata(OnsetsDSAudioBuf *odsbuf){
	// take down the FFTW stuff
	fftwf_destroy_plan(odsbuf->fftplan);
	// free mem
	free(odsbuf->data);
	free(odsbuf->window);
	fftwf_free(odsbuf->windoweddata);
	fftwf_free(odsbuf->fftbuf);
}

void onsetsds_process_audiodata(OnsetsDSAudioBuf* odsbuf, float* data, size_t datalen,
			ODSDataCallback callback){
	
	if(datalen==0){
		printf("onsetsds_process_audiodata GRRRRRR: no audio data sent (datalen==0)\n");
		return;
	}else{
	}
	
	size_t datareadpos = 0;
	size_t dataleft = datalen;
	size_t numtocopy;
	int i;
	while(dataleft > 0){
		// Read the smaller of how-much-available and how-much-to-fill-the-buffer
		numtocopy = ods_min(dataleft, odsbuf->buflen - odsbuf->writepos);
//		printf("onsetsds_process_audiodata: datalen = %i, dataleft = %i, buflen = %i, about to copy %i values to position %i\n", 
//					datalen, dataleft, odsbuf->buflen, numtocopy, odsbuf->writepos);
		memcpy(&odsbuf->data[odsbuf->writepos], &data[datareadpos], numtocopy * sizeof(float));
		
		odsbuf->writepos += numtocopy;
		
		// If the buffer is full, do all the FFT and stuff
		if(odsbuf->writepos >= odsbuf->buflen){
			
			// Copy the data into the buffer where windowing and FFT takes place
			memcpy(odsbuf->windoweddata, odsbuf->data, odsbuf->buflen * sizeof(float));
			
			// Shunt the audio data (and the writepos) down to make room for the next lot
			memcpy(odsbuf->data, &odsbuf->data[odsbuf->hopsize], (odsbuf->buflen - odsbuf->hopsize) * sizeof(float));
			//printf("onsetsds_process_audiodata: moving writepos from %i to %i(==hopsize)\n", odsbuf->writepos, odsbuf->hopsize);
			odsbuf->writepos = odsbuf->hopsize;
			
			// Windowing
			for(i=0; i<odsbuf->buflen; i++){
				odsbuf->windoweddata[i] *= odsbuf->window[i];
			}
			
			// FFT
			fftwf_execute(odsbuf->fftplan);
		
			// Onset detection
			if(onsetsds_process(odsbuf->ods, odsbuf->fftbuf)){
				// Call the callback!
				callback(odsbuf, datareadpos);
			}
			
		} // End buffer-is-filled
		
		datareadpos += numtocopy;
		dataleft -= numtocopy;
	} // End of still-some-data-to-push
	
}


void onsetsds_process_audiofile_CALLBACK(OnsetsDSAudioBuf* odsbuf, size_t onsetsamplepos);
void onsetsds_process_audiofile_CALLBACK(OnsetsDSAudioBuf* odsbuf, size_t onsetsamplepos){
	// Convert the sample pos into a seconds position through the whole file
	double secs = (odsbuf->sampsElapsed + onsetsamplepos) / odsbuf->samplerate;
	
	// Now call the file-level callback
	(odsbuf->filecallback)(odsbuf->ods, secs);
}

int onsetsds_process_audiofile(OnsetsDSAudioBuf* odsbuf, const char *infilename,
			ODSFileCallback callback){

	SNDFILE	 	*insndfile ;
	SF_INFO	 	sfinfo ;
	memset (&sfinfo, 0, sizeof (sfinfo));
	
	// Attempt to get the input file
	if ((insndfile = sf_open (infilename, SFM_READ, &sfinfo)) == NULL){
		printf ("onsetsds_process_audiofile ERROR: Not able to open input file %s.\n", infilename) ;
		fflush (stdout) ;
		return 100;
	}
	if(sfinfo.channels != 1){
		printf("onsetsds_process_audiofile ERROR: Only mono audio files can be processed. Num channels = %i. Exiting.\n", sfinfo.channels);
		sf_close(insndfile);
		return 200;
	}else{
		printf("onsetsds_process_audiofile: mono audio file, sample rate %i Hz.\n", sfinfo.samplerate);
	}
	
	odsbuf->sampsElapsed = 0L;
	odsbuf->samplerate   = (double) sfinfo.samplerate;
	odsbuf->filecallback = callback;
	
	// Create a buffer for reading the raw data into
	float* data = malloc(odsbuf->buflen * sizeof(float));
	
	sf_count_t numread;
	//printf("onsetsds_process_audiofile: Processing audio data\n", numread);
	while((numread = sf_read_float(insndfile, data, odsbuf->buflen)) > 0){
		//printf("Read %i audio frames (requested %i)\n", numread, odsbuf->buflen);
		
		//printf("Calling onsetsds_process_audiodata\n");
		onsetsds_process_audiodata(odsbuf, data, numread, onsetsds_process_audiofile_CALLBACK);
		//printf("Called onsetsds_process_audiodata\n");
		odsbuf->sampsElapsed += numread;
	}
	
	sf_close(insndfile);
	free(data);

	// Indicate success
	return 0;
}
