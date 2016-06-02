#ifndef _LAVID_MF_AUDIOENCODER_H
#define _LAVID_MF_AUDIOENCODER_H

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include <iostream>
#include <cstdio>
#include <thread>

#include "RingBuffer.h"


class MFAudioEncoder {

public:
	/*
	Constructor:
	in_buffer:      input buffer (raw audio samples)
	out_buffer:     (out) output buffer (encoded audio samples)
	sps:            audio samples per second
	bitrate:        desired average bitrate
	*/
	MFAudioEncoder(RingBuffer<IMFSample*> *in_buffer, RingBuffer<IMFSample*> *out_buffer, int sps, int bitrate);

	/*
	Initialize Media Foundation environment and variables
	*/
	int initialize();

	/*
	Start encoding
	*/
	int startEncoderThread();

	/*
	Wait for encoder thread to finish
	*/
	void joinThread();

private:

	/*
	Audio encoder thread main code
	*/
	void ProcessData();

	/*
	Searches for and configures an audio encoding MFT
	*/
	bool findEncoder();

	/*
	Media info
	*/
	int sps, bitrate;

	/*
	Input and Output buffers
	*/
	RingBuffer<IMFSample*> *in_buffer;
	RingBuffer<IMFSample*> *out_buffer;

	/*
	Quit
	*/
	bool quit;

	/*
	Microsoft Media Foundation environment and variables
	*/
	IMFTransform *pEncoder;
	IMFMediaType *pInType, *pOutType;
	IMFMediaEventGenerator *pEvGenerator;
	MFT_INPUT_STREAM_INFO inStreamInfo;
	MFT_OUTPUT_STREAM_INFO outStreamInfo;

	/*
	Thread reference
	*/
	std::thread *encodeThread;
};

#endif
