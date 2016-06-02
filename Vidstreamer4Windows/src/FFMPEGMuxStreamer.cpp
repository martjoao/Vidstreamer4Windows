#include "FFMPEGMuxStreamer.h"

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

FFMPEGMuxStreamer::FFMPEGMuxStreamer(std::string udp_address, int vidw, int vidh, int vidrnum, int vidrden,
	int sps, long long vidbitrate, long long audbitrate)
{
	p_ofmt            = NULL;
	p_ofmt_ctx        = NULL;
	p_codec           = NULL;
	video_ibuffer     = NULL;
	audio_ibuffer     = NULL;
	muxThread         = NULL;

	video_stream      = -1;
	audio_stream      = -1;
	ret               = 0;

	hasVideo          = false;
	hasAudio          = false;
	quit              = false;

	this->udp_add     = udp_address;

	this->vidw        = vidw;
	this->vidh        = vidh;
	this->vidrnum     = vidrnum;
	this->vidrden     = vidrden;
	this->sps         = sps;
	this->vidbitrate  = vidbitrate;
	this->audbitrate  = audbitrate;

	this->initialize();
}

int FFMPEGMuxStreamer::initialize()
{
	av_register_all();
	avformat_network_init();

	p_ofmt = av_guess_format(NULL, "placeholder.ts", NULL);
	if (p_ofmt == NULL)
	{
		std::cout << "FFMPEGMuxStreamer: Error guessing output format" << std::endl;
		exit(1);
	}

	p_ofmt->video_codec = AV_CODEC_ID_H264;
	p_ofmt->audio_codec = AV_CODEC_ID_AC3;
	
	ret = avformat_alloc_output_context2(&p_ofmt_ctx, p_ofmt, NULL, udp_add.c_str());
	if (ret < 0)
	{
		std::cout << "FFMPEGMuxStreamer: Unable to allocate output format context" << std::endl;
	}
}

int FFMPEGMuxStreamer::start()
{
	muxThread = new std::thread(&FFMPEGMuxStreamer::run, this);
	return 0;
}

int FFMPEGMuxStreamer::join()
{
	if (this->muxThread)
		this->muxThread->join();
	return 0;
}

AVStream* FFMPEGMuxStreamer::add_stream(RingBuffer<IMFSample*> *in_buffer, AVCodec **codec, AVCodecID codec_id)
{
	AVCodecContext *c;
	AVStream *st;
	
	*codec = avcodec_find_encoder(codec_id);

	if (!(*codec)) {
		fprintf(stderr, "FFMPEGMuxStreamer: Could not find encoder for '%s'\n",
			avcodec_get_name(codec_id));
		exit(1);
	}

	st = avformat_new_stream(this->p_ofmt_ctx, *codec);

	if (!st) {
		fprintf(stderr, "FFMPEGMuxStreamer: Could not allocate new stream\n");
		exit(1);
	}

	st->id = this->p_ofmt_ctx->nb_streams - 1;
	c = st->codec;
	
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt        = AV_SAMPLE_FMT_FLTP;
		c->bit_rate          = audbitrate;
		c->sample_rate       = sps;
		c->channels          = 2; //TODO: parametize this
		c->time_base.den     = 10000000; 
		c->time_base.num     = 1;

		st->time_base.num    = 1;
		st->time_base.den    = 90000; //MPEGTS TIMEBASE

		p_ofmt->audio_codec  = codec_id;

		this->audio_stream   = st->id;
		this->audiostream    = st;
		this->audio_ibuffer  = in_buffer;
		this->hasAudio       = true;

		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id          = codec_id;
		c->bit_rate          = this->vidbitrate;//unknown
		c->width             = vidw;
		c->height            = vidh;
		c->time_base.den     = 10000000; //time base is inverted
		c->time_base.num     = 1;
		c->pix_fmt           = AV_PIX_FMT_YUV420P;
		
		st->time_base.num    = 1;
		st->time_base.den    = 90000; //MPEGTS TIMEBASE
		
		p_ofmt->video_codec  = codec_id;

		this->videostream    = st;
		this->video_stream   = st->id;
		this->video_ibuffer  = in_buffer;
		this->hasVideo       = true;
		break;

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if (this->p_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	return st;
}

void FFMPEGMuxStreamer::run()
{
	AVPacket p_pkt;

	long long cur_pts    = 0;	
	long long aud_pts    = 0;
	long long vid_pts    = 0;
	long long audt       = 0;
	long long vidt       = 0;
	long long vidd       = 0;
	long long audd       = 0;
	bool header_written  = false;

	IMFSample *auds      = NULL;
	IMFSample *vids      = NULL; 

	ret = avio_open(&(p_ofmt_ctx->pb), udp_add.c_str(), AVIO_FLAG_WRITE);
	if (ret < 0)
	{
		std::cout << "FFMPEGMuxStreamer: Unable to open output" << std::endl;
		exit(1);
	}
		
	/*
	while (hasAudio && hasVideo)
	{
		if (auds == NULL)
		{
			auds = audio_ibuffer->readNext();
			continue;
		}
		if (vids == NULL)
		{
			vids = video_ibuffer->readNext();
			continue;
		}

		auds->GetSampleTime(&audt);
		vids->GetSampleTime(&vidt);

		auds->GetSampleDuration(&audd);
		vids->GetSampleDuration(&vidd);

		if (abs(audt - vidt) > audd)
		{
			if (audt < vidt)
			{
				SafeRelease(&auds);
				auds = audio_ibuffer->readNext();
			}
			else if (vidt < audt)
			{
				SafeRelease(&vids);
				vids = video_ibuffer->readNext();
			}
		}
		else
		{
			break;
		}
	}
	*/

	while (!quit)
	{
		IMFSample *vidsamp = NULL, *audsamp = NULL;

		if (hasVideo )
		{
			vidsamp = video_ibuffer->readNext();
			if (vidsamp != NULL)
			{
				IMFMediaBuffer* buf;
				DWORD capacity, length;

				memset(&p_pkt, 0, sizeof(AVPacket));

				p_pkt.stream_index = video_stream;
				vidsamp->GetSampleTime(&(vidt));
				vidsamp->GetSampleDuration(&(p_pkt.duration));

				vidsamp->GetBufferByIndex(0, &buf);
				buf->GetCurrentLength(&length);
				buf->Lock(&p_pkt.data, &capacity, &length);
				p_pkt.size = length;
				buf->Unlock();

				p_pkt.flags = AV_PKT_FLAG_KEY;

				p_pkt.pts = vidt;
				av_packet_rescale_ts(&p_pkt, videostream->codec->time_base, videostream->time_base);
				p_pkt.dts = p_pkt.pts;

				if (!header_written)
				{
					avformat_write_header(p_ofmt_ctx, NULL);
					header_written = true;
				}
				
				ret = av_write_frame(p_ofmt_ctx, &p_pkt);

				SafeRelease(&buf);
				//SafeRelease(&vidsamp);
				vidt += p_pkt.duration;
				av_free_packet(&p_pkt);
			}
		}
		
		if (hasAudio )
		{
			audsamp = audio_ibuffer->readNext();
			if (audsamp != NULL)
			{
				IMFMediaBuffer* buf;
				DWORD capacity, length;

				memset(&p_pkt, 0, sizeof(AVPacket));

				p_pkt.stream_index = audio_stream;
				audsamp->GetSampleTime(&(audt));
				audsamp->GetSampleDuration(&(p_pkt.duration));
												
				audsamp->GetBufferByIndex(0, &buf);
				buf->GetCurrentLength(&length);
				buf->Lock(&p_pkt.data, &capacity, &length);
				p_pkt.size = length;
				buf->Unlock();

				p_pkt.flags = AV_PKT_FLAG_KEY;
				
				p_pkt.pts = audt;
				av_packet_rescale_ts(&p_pkt, audiostream->codec->time_base, audiostream->time_base);
				p_pkt.dts = p_pkt.pts;

				if (!header_written)
				{
					avformat_write_header(p_ofmt_ctx, NULL);
					header_written = true;
				}
				int ret = av_write_frame(p_ofmt_ctx, &p_pkt);

				//SafeRelease(&audsamp);
				SafeRelease(&buf);
				av_free_packet(&p_pkt);
			}
		}

		if (vidsamp == NULL && audsamp == NULL)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		SafeRelease(&vidsamp);
		SafeRelease(&audsamp);

	}
}