#include "MFH264Encoder.h"
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

MFH264Encoder::MFH264Encoder(RingBuffer<IMFSample*> *in_buffer, RingBuffer<IMFSample*> *out_buffer,
	int frameWidth, int frameHeight, int frameRate, int frameAspect, int bitrate)
{
	this->pEncoder			= NULL;
	this->pInType           = NULL;
	this->pOutType          = NULL;
	this->pEvGenerator      = NULL;

	this->quit              = false;

	this->in_buffer         = in_buffer;
	this->out_buffer        = out_buffer;
	this->frameAspect       = frameAspect;
	this->frameHeight       = frameHeight;
	this->frameWidth        = frameWidth;
	this->frameRate         = frameRate;
	this->bitrate           = bitrate;

	this->initialize();
}

int MFH264Encoder::initialize()
{
	MFUtils::initializeMF();

	//Finds, creates and configures a H264 HW encoder
	findEncoder();
	return 0;
}

int MFH264Encoder::startEncoder()
{
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
	return 0;
}

bool MFH264Encoder::findEncoder()
{
	HRESULT hr;
	
	MFT_REGISTER_TYPE_INFO out_type = { 0 };
	out_type.guidMajorType = MFMediaType_Video;
	out_type.guidSubtype = MFVideoFormat_H264;

	IMFActivate **ppActivate = NULL;
	UINT32 count = 0;

	hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
		MFT_ENUM_FLAG_HARDWARE,
		NULL, &out_type,
		&ppActivate,
		&count
	);

	if (FAILED(hr) || count == 0) 
	{
		std::cout << "Video Encoder: Specified encoder not found!" << std::endl;
		exit(1);
	}

	hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(&pEncoder));
	
	if (FAILED(hr))
	{
		std::cout << "Video Encoder: Unable to activate encoder" << std::endl;
		exit(1);
	}

	for (UINT32 i = 0; i < count; i++) 
	{
		ppActivate[i]->Release();
	}
	CoTaskMemFree(ppActivate);

	//Enable async mode
	IMFAttributes *pAttributes = NULL;
	hr = pEncoder->GetAttributes(&pAttributes);
	hr = pAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
	SafeRelease(&pAttributes);

	//Get Stream info
	DWORD inMin = 0, inMax = 0, outMin = 0, outMax = 0;
	pEncoder->GetStreamLimits(&inMin, &inMax, &outMin, &outMax);
	
	DWORD inStreamsCount = 0, outStreamsCount = 0;
	pEncoder->GetStreamCount(&inStreamsCount, &outStreamsCount);

	DWORD * inStreams = new DWORD[ inStreamsCount];
	DWORD *outStreams = new DWORD[outStreamsCount];

	hr = pEncoder->GetStreamIDs(inStreamsCount, inStreams, outStreamsCount, outStreams);

	if (hr != S_OK) 
	{
		if (hr == E_NOTIMPL)
		{
			inStreams[0]  = 0;
			outStreams[0] = 0;
		}
		else 
		{
			std::cout << "Video Encoder: Unable to get MFT encoder stream IDs" << std::endl;
			exit(1);
		}
	}

	//Set types

	MFCreateMediaType(&pOutType);

	pOutType->SetGUID(MF_MT_MAJOR_TYPE, out_type.guidMajorType);
	pOutType->SetGUID(MF_MT_SUBTYPE, out_type.guidSubtype);
	MFSetAttributeSize(pOutType, MF_MT_FRAME_SIZE, frameWidth, frameHeight);
	MFSetAttributeRatio(pOutType, MF_MT_FRAME_RATE, frameRate, 1);
	MFSetAttributeRatio(pOutType, MF_MT_PIXEL_ASPECT_RATIO, frameAspect, 1);
	pOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	//pOutType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
	//pOutType->SetUINT32(MF_MT_MPEG2_LEVEL, eAVEncH264VLevel5_1);
	pOutType->SetUINT32(MF_MT_AVG_BITRATE, this->bitrate);
	pOutType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, 10); 

	hr = pEncoder->SetOutputType(outStreams[0], pOutType, 0);

	if (FAILED(hr))
	{
		std::cout << "Video Encoder: Failed to set encoder output type" << std::endl;
		exit(1);
	}

	GUID format;
	for (int i = 0; ; i++)
	{
		hr = pEncoder->GetInputAvailableType(inStreams[0], i, &pInType);
		if (hr != S_OK) break;
		
		pInType->GetGUID(MF_MT_SUBTYPE, &format);
		if (format == MFVideoFormat_NV12) break;
		//if (format == MFVideoFormat_UYVY) break;
		//if (format == MFVideoFormat_IYUV) break;
		SafeRelease(&pInType);
	}

	if (pInType == NULL) 
	{
		std::cout << "Video Encoder: Failed to get input type" << std::endl;
		exit(1);
	}

	UINT32 w, h, fps, den;
	MFGetAttributeSize(pInType, MF_MT_FRAME_SIZE, &w, &h);
	MFGetAttributeRatio(pInType, MF_MT_FRAME_RATE, &fps, &den);

	hr = pEncoder->SetInputType(inStreams[0], pInType, 0);
	if(FAILED(hr))
	{
		std::cout << "Video Encoder: Failed to set input type" << std::endl;
		exit(1);
	}

	hr = pEncoder->QueryInterface(IID_PPV_ARGS(&pEvGenerator));
	if (FAILED(hr))
	{
		std::cout << "Video Encoder: Failed to expose interface" << std::endl;
		exit(1);
	}

	encoderCb = new EncoderEventCallback(this);
	pEvGenerator->BeginGetEvent(encoderCb, NULL);
	return SUCCEEDED(hr);
}

MFH264Encoder::EncoderEventCallback::EncoderEventCallback(MFH264Encoder *pEncoder)
{
	this->pEncodeH264 = pEncoder;
}

MFH264Encoder::EncoderEventCallback::~EncoderEventCallback() 
{

}

STDMETHODIMP MFH264Encoder::EncoderEventCallback::QueryInterface(REFIID _riid, void** pp_v)
{
	static const QITAB _qit[] = 
	{
		QITABENT(EncoderEventCallback, IMFAsyncCallback), { 0 }
	};
	return QISearch(this, _qit, _riid, pp_v);
}

STDMETHODIMP_(ULONG) MFH264Encoder::EncoderEventCallback::AddRef()
{
	return InterlockedIncrement(&ref_count);
}

STDMETHODIMP_(ULONG) MFH264Encoder::EncoderEventCallback::Release()
{
	long result = InterlockedDecrement(&ref_count);

	if (result == 0) 
	{
		delete this;
	}

	return result;
}

STDMETHODIMP MFH264Encoder::EncoderEventCallback::GetParameters(DWORD *p_flags, DWORD *p_queue)
{
	return E_NOTIMPL;
}

STDMETHODIMP MFH264Encoder::EncoderEventCallback::Invoke(IMFAsyncResult *pAsyncResult)
{
	IMFMediaEvent *pMediaEvent  = NULL;
	MediaEventType evType       = MEUnknown;
	HRESULT hr                  = S_OK;
	IMFSample *pSample          = NULL;
	int res                     = -1;
	MFT_OUTPUT_DATA_BUFFER outDataBuffer;
	DWORD status;

	pEncodeH264->pEvGenerator->EndGetEvent(pAsyncResult, &pMediaEvent);

	pMediaEvent->GetType(&evType);
	pMediaEvent->GetStatus(&hr);
	
	if (evType == METransformNeedInput)
	{
		pSample = pEncodeH264->in_buffer->readNext();
		if (pSample == NULL)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		hr = pEncodeH264->pEncoder->ProcessInput(0, pSample, 0);
		if (FAILED(hr))
		{
			std::cout << "Video Encoder: Process Input Failed" << std::endl;
		}
		if (pSample)
		{
			IMFMediaBuffer* buf;
			pSample->GetBufferByIndex(0, &buf);
			SafeRelease(&buf);
		}
		SafeRelease(&pSample);
	
	}
	else if (evType == METransformHaveOutput)
	{
		outDataBuffer.dwStatus    = 0;
		outDataBuffer.dwStreamID  = 0;
		outDataBuffer.pEvents     = 0;
		outDataBuffer.pSample     = NULL;

		hr = pEncodeH264->pEncoder->ProcessOutput(0, 1, &outDataBuffer, &status);
		if (SUCCEEDED(hr))
		{
			
			IMFMediaBuffer* pOutBuffer = NULL;
			outDataBuffer.pSample->GetBufferByIndex(0, &pOutBuffer);
			 
			DWORD totalLength = 0;
			outDataBuffer.pSample->GetTotalLength(&totalLength);
			//std::cout << "Output Processed: l: " << totalLength << std::endl;

			pEncodeH264->out_buffer->writeNext(outDataBuffer.pSample);
			SafeRelease(&pOutBuffer);
			//SafeRelease(&outDataBuffer.pSample);
			SafeRelease(&outDataBuffer.pEvents);
		}
		
		if (FAILED(hr))
		{
			if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
				//std::cout << "Video Encoder: TRANSFORM STREAM CHANGE" << std::endl;

				hr = pEncodeH264->pEncoder->SetOutputType(0, pEncodeH264->pOutType, 0);
				if (FAILED(hr))
				{
					std::cout << "Video Encoder: Failed to set output type" << std::endl;
				}
			}
			else {
				std::cout << "Video Encoder: Process output failed" << std::endl;
			}
		}
		
	}
	else if (evType == MF_E_TRANSFORM_STREAM_CHANGE)
	{
		//std::cout << "TRANSFORM STREAM CHANGE" << std::endl;
	}

	pMediaEvent->Release();
	pEncodeH264->pEvGenerator->BeginGetEvent(this, NULL);
	return S_OK;
}


