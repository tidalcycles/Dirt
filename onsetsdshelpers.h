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

/** \file */

/**
 * \defgroup HelperFuncs	Convenience functions to apply OnsetsDS to a chunk of audio data, or to an audio file.
 * 
 * These functions are NOT required in order to use the core OnsetsDS functionality, but provide wrappers to
 * make it easy to apply OnsetsDS to time-domain data (audio chunks, or audio files) without having to write the
 * FFT processing yourself.
 */
 //@{ 
 
#ifndef _OnsetsDSHelpers_
#define _OnsetsDSHelpers_

#ifdef __cplusplus
extern "C" {
#endif

#include <fftw3.h>
#include "../onsetsds.h"

////////////////////////////////////////////////////////////////////////////////

/**
When using onsetsds_process_audiofile(), this specifies that your callback function should
take an #OnsetsDS and a double as arguments, and return void. The double will be a time
offset, from the beginning of the recording, at which the detected onset occurred.
*/
typedef void (*ODSFileCallback)(OnsetsDS*, double);

/**
Holds all the state data required by onsetsds_process_audiodata(), including a pointer to an #OnsetsDS
as well as the time-domain/freq-domain buffers.
Also remembers the FFT frame size, hop size.
*/
typedef struct OnsetsDSAudioBuf{
	OnsetsDS* ods;
	
	size_t buflen;
	size_t hopsize;
	size_t writepos;
	float *data;         // size will be buflen
	float *window;       // size will be buflen
	float *windoweddata; // size will be buflen
	float *fftbuf; // size will be buflen
	fftwf_plan fftplan;
	
	// Whole-file-only things (i.e. unused when you're pushing audio blocks yourself):
	long sampsElapsed;
	double samplerate;
	ODSFileCallback filecallback;
} OnsetsDSAudioBuf;

/**
When using onsetsds_process_audiodata(), this specifies that your callback function should
take an #OnsetsDSAudioBuf and a size_t as arguments, and return void. The size_t will be a sample
offset at which the detected onset occurred, within the audio frame that was just passed in. (More than 
one onset per audio frame is possible, depending on how much data you're passing in at a time.)
*/
typedef void (*ODSDataCallback)(OnsetsDSAudioBuf*, size_t);

/**
This data structure stores statistics derived from using onsetsds_evaluate_audiofile(), describing how well
the onset detector matched the "ground truth" annotations.
*/
typedef struct OnsetsDSEvalData{
	long numGT; ///< How many ground truth annotations were provided
	long numAnnot; ///< How many onsets it found
	long numTP; ///< How many correct detections occurred
	long numFP; ///< How many false positives occurred
	long numFN; ///< How many false negatives occurred
	
	float precision; ///< 0 to 1: a measure of resistance against false positives
	float recall; ///< 0 to 1: a measure of resistance against false negatives
	float f; ///< 0 to 1: the "F-measure", a combination of the precision and recall statistics
	
	float devimean; ///< Mean of each onset's deviation from the annotated onset, a rough indicator of reacting "too quickly"/"too slowly"
	float deviabsmean; ///< Absolute mean of each onset's deviation from the annotated onset, a rough indicator of temporal accuracy
	float devisd; ///< Standard deviation re devimean, useful when combining stats
	float deviabssd; ///< Standard deviation re deviabsmean, useful when combining stats
} OnsetsDSEvalData;

////////////////////////////////////////////////////////////////////////////////

/**
Set up the data structures for use by onsetsds_process_audiodata().

@param odsbuf	Will be set up nicely by this function.
@param ods		Must have been initialised properly before calling this function.
@param hopsize	Hop size in samples (256 is recommended)
*/
void onsetsds_init_audiodata(OnsetsDSAudioBuf* odsbuf, OnsetsDS* ods, size_t hopsize);
/**
Correctly deallocate and destroy the #OnsetsDSAudioBuf. Use this after onsetsds_process_audiofile(), or after you've finished
running a series of onsetsds_process_audiodata() calls.

@param odsbuf	
*/
void onsetsds_destroy_audiodata(OnsetsDSAudioBuf* odsbuf);

/**
Process a new chunk of audio data. 

@param odsbuf	Must have been initialised properly before calling this function, using onsetsds_init_audiodata()
@param data		The *new* chunk of mono, time-domain audio data. Internal buffers will handle frame overlap etc. The size 
			of the input data does *not* need to have a relation to the frame size or hop size.
@param datalen	Size of the data buffer.
@param callback	Name of your callback function, which will be called whenever an onset is detected. 
			It will be passed the #OnsetsDSAudioBuf object and (more importantly) the sample offset at which the onset was detected 
			(i.e. a value between 0 and datalen).
*/
void onsetsds_process_audiodata(OnsetsDSAudioBuf* odsbuf, float* data, size_t datalen, 
			ODSDataCallback callback);

////////////////////////////////////////////////////////////////////////////////

/**
Process an entire file of audio data. Returns 0 if successful (may fail if it can't find/open the audio file, for example).

@param odsbuf		Must have been initialised properly before calling this function, using onsetsds_init_audiodata()
@param infilename	The file to be loaded.
@param callback		Name of your callback function, which will be called whenever an onset is detected. 
			It will be passed the #OnsetsDS object and (more importantly) the time offset in seconds at which the onset was detected 
			(a double-precision-floating-point value between 0 and the duration of the audio file).
*/
int onsetsds_process_audiofile(OnsetsDSAudioBuf* odsbuf, const char *infilename,
			ODSFileCallback callback);

/**
Process an entire file of audio data and compare the onset detector's output against a single "ground truth" annotation of where the 
onsets really are. 

@param odsbuf		Must have been initialised properly before calling this function, using onsetsds_init_audiodata()
@param infilename	The file to be loaded.
@param gtfilename	The file containing a text list of ground-truth annotations, one number per line, each being an onset's
						position in seconds from the beginning of the file. The numbers must be in ascending order.
						This format can be easily exported from programs 
						such as <a href="http://www.sonicvisualiser.org/">Sonic Visualiser</a>.
@param results		Pointer to the #OnsetsDSEvalData where the results should be written.

*/
int onsetsds_evaluate_audiofile(OnsetsDSAudioBuf* odsbuf, const char *infilename, const char *gtfilename, OnsetsDSEvalData* results);

////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

//@}
