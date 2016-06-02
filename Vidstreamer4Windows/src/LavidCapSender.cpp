#include "DefaultCapSender.h"


DefaultCapSender::DefaultCapSender(std::string udp_add, int vidbitrate, int audbitrate)
{
	this->udp_add         = udp_add;
	this->vidbitrate      = vidbitrate;
	this->audbitrate      = audbitrate;

	this->thread_ref      = NULL;

	this->videoCapBuffer  = NULL;
	this->audioCapBuffer  = NULL;
	this->videoEncBuffer  = NULL;
	this->audioEncBuffer  = NULL;

	this->vidcap          = NULL;
	this->videnc          = NULL;
	this->audcap          = NULL;
	this->audenc          = NULL;
	this->mux             = NULL;

}

DefaultCapSender::~DefaultCapSender()
{

}

void DefaultCapSender::start()
{
	this->thread_ref = new std::thread(&DefaultCapSender::run, this);
}

void DefaultCapSender::run()
{
	MFUtils::initializeMF();

	//buffers
	this->videoCapBuffer = new RingBuffer<IMFSample*>(5);
	this->audioCapBuffer = new RingBuffer<IMFSample*>(5);
	this->videoEncBuffer = new RingBuffer<IMFSample*>(5);
	this->audioEncBuffer = new RingBuffer<IMFSample*>(5);

	this->vidcap = new MFCapture(videoCapBuffer, true);
	this->videnc = new MFH264Encoder(videoCapBuffer, videoEncBuffer,
		vidcap->getWidth(), vidcap->getHeight(), vidcap->getRateNum() / vidcap->getRateDen(), 1, vidbitrate);
	this->audcap = new MFCapture(audioCapBuffer, false);
	this->audenc = new MFAudioEncoder(audioCapBuffer, audioEncBuffer, audcap->getSamplesPerSecond(), audbitrate);
	this->mux = new FFMPEGMuxStreamer(udp_add, vidcap->getWidth(),
		vidcap->getHeight(), vidcap->getRateNum(), vidcap->getRateDen(), audcap->getSamplesPerSecond(), vidbitrate, audbitrate);

	
	AVCodec* c;
	mux->add_stream(audioEncBuffer, &c, AV_CODEC_ID_AC3);
	mux->add_stream(videoEncBuffer, &c, AV_CODEC_ID_H264);

	audenc->startEncoderThread();
	audcap->startCaptureThread();
	vidcap->startCaptureThread();
	videnc->startEncoder();
	mux->start();

	vidcap->joinThread();
	audcap->joinThread();
	audenc->joinThread();
	mux->join();

}

