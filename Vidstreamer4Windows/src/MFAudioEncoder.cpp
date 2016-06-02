#include "MFAudioEncoder.h"
#include "MFUtils.h"

#include <codecapi.h>
#include <Shlwapi.h>

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

MFAudioEncoder::MFAudioEncoder(RingBuffer<IMFSample*> *in_buffer, RingBuffer<IMFSample*> *out_buffer, int sps, int bitrate)
{

	this->encodeThread   = NULL;
	this->pEncoder       = NULL;
	this->pInType        = NULL;
	this->pOutType       = NULL;	
	this->pEvGenerator   = NULL;

	this->in_buffer      = in_buffer;
	this->out_buffer     = out_buffer;
 	this->sps            = sps;
	this->bitrate        = bitrate;

	this->quit           = false;

	this->initialize();
}

int MFAudioEncoder::initialize()
{
	MFUtils::initializeMF();
	findEncoder();
	return 0;
}


int MFAudioEncoder::startEncoderThread()
{
	encodeThread = new std::thread(&MFAudioEncoder::ProcessData, this);
	return 0;
}
void MFAudioEncoder::joinThread()
{
	if (encodeThread)
	{
		encodeThread->join();
	}
}

void MFAudioEncoder::ProcessData()
{
	HRESULT hr = 0;
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);

	while (!quit)
	{
		IMFSample* pSample = in_buffer->readNext();
		if (pSample == NULL)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		hr = pEncoder->ProcessInput(0, pSample, 0);
		if (FAILED(hr))
		{
			std::cout << "Audio Encoder: Input Failed" << std::endl;
		}
		SafeRelease(&pSample);

		while (true)
		{
			DWORD flags = 0;
			hr = pEncoder->GetOutputStatus(&flags);
			if (flags != MFT_OUTPUT_STATUS_SAMPLE_READY && hr != E_NOTIMPL) {
				break;
			}

			MFT_OUTPUT_DATA_BUFFER outDataBuffer;
			MFT_OUTPUT_STREAM_INFO info;
			IMFMediaBuffer *b;
			DWORD status;

			outDataBuffer.dwStatus = 0;
			outDataBuffer.dwStreamID = 0;
			outDataBuffer.pEvents = NULL;
			outDataBuffer.pSample = NULL;

			pEncoder->GetOutputStreamInfo(0, &info);

			MFCreateSample(&outDataBuffer.pSample);
			MFCreateMemoryBuffer(info.cbSize, &b);
			outDataBuffer.pSample->AddBuffer(b);

			hr = pEncoder->ProcessOutput(0, 1, &outDataBuffer, &status);
			if (SUCCEEDED(hr))
			{
				DWORD totalLength = 0;
				outDataBuffer.pSample->GetTotalLength(&totalLength);
				//std::cout << "Audio Encoder: Output Processed: l: " << totalLength << std::endl;

				this->out_buffer->writeNext(outDataBuffer.pSample);
				outDataBuffer.pSample->AddRef();
				//b->AddRef();
			}
			else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
			{
				SafeRelease(&outDataBuffer.pSample);
				SafeRelease(&outDataBuffer.pEvents);
				SafeRelease(&b);

				continue;
			}
			else if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
			{
				//std::cout << "Audio Encoder: TRANSFORM STREAM CHANGE" << std::endl;
				hr = pEncoder->SetOutputType(0, pOutType, 0);
				if (FAILED(hr))
				{
					std::cout << "Audio Encoder: Failed to set output type" << std::endl;
				}
			}
			else
			{
				std::cout << "Audio Encoder: Process output failed" << std::endl;
			}

			SafeRelease(&outDataBuffer.pSample);
			SafeRelease(&outDataBuffer.pEvents);
			SafeRelease(&b);
		}		
	}
}

bool MFAudioEncoder::findEncoder()
{
	HRESULT hr;
	MFT_REGISTER_TYPE_INFO out_type = { 0 };

	out_type.guidMajorType = MFMediaType_Audio;
	out_type.guidSubtype   = MFAudioFormat_Dolby_AC3;

	IMFActivate **ppActivate = NULL;
	UINT32 count = 0;

	hr = MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER,
		MFT_ENUM_FLAG_ALL,
		NULL, &out_type,
		&ppActivate,
		&count
		);

	if (FAILED(hr) || count == 0)
	{
		std::cout << "Audio Encoder: Specified encoder not found" << std::endl;
		exit(1);
	}

	hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(&pEncoder));

	if (FAILED(hr))
	{
		std::cout << "Audio Encoder: Unable to activate encoder" << std::endl;
		exit(1);
	}

	for (UINT32 i = 0; i < count; i++)
	{
		ppActivate[i]->Release();
	}
	CoTaskMemFree(ppActivate);

	DWORD inMin = 0, inMax = 0, outMin = 0, outMax = 0;
	pEncoder->GetStreamLimits(&inMin, &inMax, &outMin, &outMax);

	DWORD inStreamsCount = 0, outStreamsCount = 0;
	pEncoder->GetStreamCount(&inStreamsCount, &outStreamsCount);

	DWORD * inStreams = new DWORD[inStreamsCount];
	DWORD *outStreams = new DWORD[outStreamsCount];

	hr = pEncoder->GetStreamIDs(inStreamsCount, inStreams, outStreamsCount, outStreams);

	if (hr != S_OK)
	{
		if (hr == E_NOTIMPL)
		{
			inStreams[0] = 0;
			outStreams[0] = 0;
		}
		else
		{
			std::cout << "Unable to get MFT encoder stream IDs" << std::endl;
			exit(1);
		}
	}

	MFCreateMediaType(&pOutType);
	pOutType->SetGUID(MF_MT_MAJOR_TYPE, out_type.guidMajorType);
	pOutType->SetGUID(MF_MT_SUBTYPE, out_type.guidSubtype);
	pOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
	pOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, this->sps);
	pOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
	//pOutType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000);
	if (out_type.guidSubtype == MFAudioFormat_AAC)
		pOutType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1);

	hr = pEncoder->SetOutputType(outStreams[0], pOutType, 0);

	if (FAILED(hr))
	{
		std::cout << "Audio Encoder: Failed to set encoder output type" << std::endl;
		exit(1);
	}

	GUID format;
	for (int i = 0;; i++)
	{
		hr = pEncoder->GetInputAvailableType(inStreams[0], i, &pInType);
		if (hr != S_OK) break;

		pInType->GetGUID(MF_MT_SUBTYPE, &format);
		if (format == MFAudioFormat_PCM) break;
		SafeRelease(&pInType);
	}

	if (pInType == NULL)
	{
		std::cout << "Audio Encoder: Failed to get input type" << std::endl;
		exit(1);
	}

	pInType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
	pInType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, this->sps);
	pInType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
	pInType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2*2);
	pInType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2*2 * this->sps);

	hr = pEncoder->SetInputType(inStreams[0], pInType, 0);
	if (FAILED(hr))
	{
		std::cout << "Audio Encoder: Failed to set input type" << std::endl;
		exit(1);
	}
	
	return SUCCEEDED(hr);
}
