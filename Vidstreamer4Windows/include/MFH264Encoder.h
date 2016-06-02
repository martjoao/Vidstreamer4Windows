#ifndef _LAVID_MF_ENCODEH264_H
#define _LAVID_MF_ENCODEH264_H

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include <iostream>
#include <cstdio>
#include <thread>

#include "RingBuffer.h"


class MFH264Encoder {

public:
	/*
	Constructor
	in_buffer:     raw video input samples
	out_buffer:    encoded video output samples
	frameWidth:    video width
	frameHeight:   video height
	frameRate:     video frame rate
	frameAspect:   video frame aspect
	bitrate:       desired output bitrate
	*/
	MFH264Encoder(RingBuffer<IMFSample*> *in_buffer, RingBuffer<IMFSample*> *out_buffer,
		int frameWidth, int frameHeight, int frameRate, int frameAspect, int bitrate);

	/*
	Initialize MF environment and variables
	*/
	int initialize();

	/*
	Start encoding
	*/
	int startEncoder();

private:

	/*
	Inner class for encoding media (Async MFT)
	*/
	class EncoderEventCallback : public IMFAsyncCallback {
	public:
		EncoderEventCallback(MFH264Encoder *pEncoder);
		virtual ~EncoderEventCallback();

		STDMETHODIMP QueryInterface(REFIID _riid, void** pp_v);

		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		STDMETHODIMP GetParameters(DWORD* p_flags, DWORD* p_queue);
		STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

	private:
		MFH264Encoder *pEncodeH264;
		long ref_count;
	};

	/*
	Finds and configures an MFT encoder
	*/
	bool findEncoder();

	/*
	Video Properties
	*/
	int frameWidth, frameHeight, frameRate, frameAspect, bitrate;

	/*
	I/O Buffers
	*/
	RingBuffer<IMFSample*> *in_buffer;
	RingBuffer<IMFSample*> *out_buffer;

	/*
	Quit Flag
	*/
	bool quit;

	/*
	Media Foundation Environment and variables
	*/
	IMFTransform *pEncoder;
	IMFMediaType *pInType, *pOutType;
	IMFMediaEventGenerator *pEvGenerator;
	EncoderEventCallback *encoderCb;
	MFT_INPUT_STREAM_INFO inStreamInfo;
	MFT_OUTPUT_STREAM_INFO outStreamInfo;
};

#endif
