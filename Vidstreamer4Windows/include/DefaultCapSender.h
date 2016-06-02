#ifndef _LAVID_CAP_SENDER_H
#define _LAVID_CAP_SENDER_H

#include <string>
#include <thread>

#include "RingBuffer.h"

#include "MFCapture.h"
#include "MFAudioEncoder.h"
#include "MFH264Encoder.h"
#include "FFMPEGMuxStreamer.h"
#include "MFUtils.h"


class DefaultCapSender
{
public:
	DefaultCapSender(std::string udp_add, int vidbitrate, int audbitrate);
	~DefaultCapSender();

	void start();
private:
	void run();

	std::string udp_add;
	int vidbitrate;
	int audbitrate;

	std::thread* thread_ref;

	RingBuffer<IMFSample*> *videoCapBuffer;
	RingBuffer<IMFSample*> *audioCapBuffer;
	RingBuffer<IMFSample*> *videoEncBuffer;
	RingBuffer<IMFSample*> *audioEncBuffer;

	MFCapture *vidcap;
	MFH264Encoder *videnc;
	MFCapture *audcap;
	MFAudioEncoder *audenc;
	FFMPEGMuxStreamer *mux;


};

#endif