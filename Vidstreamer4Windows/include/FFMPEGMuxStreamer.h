#ifndef _LAVID_FFMPEG_MUX_STREAMER_H
#define _LAVID_FFMPEG_MUX_STREAMER_H

//prevent av_free_packet deprecated error
#pragma warning(disable: 4996)

#include <mfapi.h>

#include <iostream>
#include <cstdio>
#include <thread>
#include <string>

extern "C"
{
#include <libavformat\avformat.h>
#include <libavcodec\avcodec.h>
}

#include "RingBuffer.h"

class FFMPEGMuxStreamer {

public:
	/*
	Constructor:
	udp_address:  IP:PORT to send stream to
	vidw:         video width
	vidh:         video height
	vidrnum:      video frame rate numerator
	vidrden:      video frame rate denominator
	sps:          audio samples per second
	vidbitrate:   video bitrate
	audbitrate:   audio bitrate
	*/
	FFMPEGMuxStreamer(std::string udp_address, int vidw, int vidh, int vidrnum,
		int vidrden, int sps, long long vidbitrate, long long audbitrate);

	/*
	Initialize FFMPEG context and structs
	*/
	int initialize();

	/*
	Start muxing and sending streams
	*/
	int start();

	/*
	Wait for muxer thread to finish
	*/
	int join();

	/*
	Adds a stream to be muxed:
	in_buffer:	Video frames or audio samples buffer to be muxed
	codec:		(out) codec struct with information
	codec_id:   Video/Audio format
	*/
	AVStream *add_stream(RingBuffer<IMFSample*> *in_buffer, AVCodec **codec, AVCodecID codec_id);

private:

	/*
	Main thread code
	*/
	void run();

	/*
	Streams information
	*/
	int vidw, vidh, vidrnum, vidrden;
	int sps, audbitrate;
	long long vidbitrate;


	/*
	Audio and Video input buffers
	*/
	RingBuffer<IMFSample*> *video_ibuffer;
	RingBuffer<IMFSample*> *audio_ibuffer;

	/*
	Audio and video stream codes
	*/
	int video_stream;
	int audio_stream;

	/*
	Check if there is an audio or video stream
	*/
	bool hasVideo;
	bool hasAudio;

	/*
	Output UDP address
	*/
	std::string udp_add;
	bool quit;

	/*
	FFMPEG variables and environment
	*/
	AVOutputFormat *p_ofmt;
	AVFormatContext *p_ofmt_ctx;
	AVCodec * p_codec;
	AVStream *audiostream, *videostream;

	/*
	Thread reference
	*/
	std::thread* muxThread;

	/*
	Return value, used in all methods
	*/
	int ret;


};

#endif
