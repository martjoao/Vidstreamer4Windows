#ifndef _LAVID_MF_CAPTURE_H
#define _LAVID_MF_CAPTURE_H

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include <iostream>
#include <cstdio>
#include <thread>

#include "RingBuffer.h"

class MFCapture
{
public:
	/*
	Constructor
	out_buffer:		output raw frames/samples
	isVideo:		if true, capture video, else capture audio
	*/
	MFCapture(RingBuffer<IMFSample*> *out_buffer, bool isVideo);

	/*
	Initializes media foundation environment and variables
	*/
	int initialize();

	/*
	Start capturing and filling buffer
	*/
	int startCaptureThread();

	/*
	Wait for capture thread to finish
	*/
	void joinThread();

	/*
	Get captured media information
	*/
	int getWidth();
	int getHeight();
	int getRateNum();
	int getRateDen();
	int getBitsPerSample();
	int getSamplesPerSecond();

private:

	/*
	Auxiliary Media Foundation related methods
	*/
	HRESULT ShowDeviceNames();
	HRESULT CreateVideoCaptureDevice();
	HRESULT ProcessSamples();
	HRESULT ConfigureDecoder(DWORD dwStreamIndex);
	HRESULT EnumerateTypesForStream(DWORD dwStreamIndex);


	/*
	Output Buffer
	*/
	RingBuffer<IMFSample*> *out_buffer;

	/*
	Media Foundation environment and variables
	*/
	IMFActivate **ppDevices;
	IMFMediaSource *pSource;
	IMFSourceReader *pReader;
	UINT count;

	/*
	Run while !quit
	*/
	bool quit;

	/*
	Sets capture mode from audio to video
	*/
	bool isVideo;

	/*
	Video information (set if isVideo==true, 0 otherwise)
	*/
	int width, height, ratenum, rateden;


	/*
	Audio information (set if isVideo==false, 0 otherwise)
	bps: bits per sample
	sps: samples per second
	*/
	int bps, sps;

	/*
	Capture thread reference
	*/
	std::thread* captureThread;
};

#endif