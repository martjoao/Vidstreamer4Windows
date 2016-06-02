#include "MFCapture.h"
#include "MFUtils.h"

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

MFCapture::MFCapture(RingBuffer<IMFSample*> *out_buffer, bool isVideo)
{
	this->ppDevices     = NULL;
	this->pReader		= NULL;
	this->pSource		= NULL;
	this->captureThread = NULL;
	this->quit			= false;
	this->out_buffer    = out_buffer;
	this->isVideo       = isVideo;

	this->width         = -1;
	this->height        = -1;
	this->ratenum       = -1;
	this->rateden       = -1;

	this->initialize();
}

int MFCapture::initialize()
{
	HRESULT hr;
	MFUtils::initializeMF();

	hr = CreateVideoCaptureDevice();
	if (FAILED(hr)) {
		std::cout << "MFCapture: Unable to create IMFMediaSource from capture device" << std::endl;
		exit(1);
	}

	hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
	if (FAILED(hr)) {
		std::cout << "MFCapture: Unable to create IMFMediaSource from capture device" << std::endl;
		exit(1);
	}
	ConfigureDecoder(0);
	EnumerateTypesForStream(0);
	return 0;
}

int MFCapture::startCaptureThread()
{
	captureThread = new std::thread(&MFCapture::ProcessSamples, this);
	return 0;
}

void MFCapture::joinThread()
{
	if (captureThread) 
	{
		captureThread->join();
	}
}

int MFCapture::getWidth()
{
	return this->width;
}

int MFCapture::getHeight()
{
	return this->height;
}

int MFCapture::getRateNum()
{
	return this->ratenum;
}

int MFCapture::getRateDen()
{
	return this->rateden;
}

int MFCapture::getSamplesPerSecond()
{
	return this->sps;
}

int MFCapture::getBitsPerSample()
{
	return this->bps;
}

HRESULT MFCapture::ShowDeviceNames()
{
	for (DWORD i = 0; i < count; i++)
	{
		HRESULT hr = S_OK;
		WCHAR *szFriendlyName = NULL;

		// Try to get the display name.
		UINT32 cchName;
		hr = ppDevices[i]->GetAllocatedString(
			MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
			&szFriendlyName, &cchName);

		if (SUCCEEDED(hr))
		{
			wprintf(L"%d - %ls\n", i, szFriendlyName);
		}
		CoTaskMemFree(szFriendlyName);
	}
	return 0;
}

HRESULT MFCapture::CreateVideoCaptureDevice()
{
	IMFMediaSource **ppSource = &pSource;
	UINT32 count = 0;
	IMFAttributes *pConfig = NULL;

	// Create an attribute store to hold the search criteria.
	HRESULT hr = MFCreateAttributes(&pConfig, 1);

	// Request video capture devices.
	if (SUCCEEDED(hr))
	{
		if (isVideo)
		{
			hr = pConfig->SetGUID(
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
				);
		}
		else
		{
			hr = pConfig->SetGUID(
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID
				);
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = MFEnumDeviceSources(pConfig, &ppDevices, &count);
	}
	if (SUCCEEDED(hr))
	{
		ShowDeviceNames();
	}

	// Create a media source for the first device in the list.
	if (SUCCEEDED(hr))
	{
		if (count > 0)
		{
			hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(ppSource));
		}
		else
		{
			hr = MF_E_NOT_FOUND;
		}
	}

	for (DWORD i = 0; i < count; i++)
	{
		ppDevices[i]->Release();
	}
	CoTaskMemFree(ppDevices);
	return hr;
}

HRESULT MFCapture::ProcessSamples()
{
	HRESULT hr          = S_OK;
	IMFSample *pSample  = NULL;
	size_t  cSamples    = 0;

	while (!quit)
	{
		DWORD streamIndex, flags;
		LONGLONG llTimeStamp;

		hr = pReader->ReadSample(
			MF_SOURCE_READER_ANY_STREAM,    // Stream index.
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llTimeStamp,                   // Receives the time stamp.
			&pSample                        // Receives the sample or NULL.
			);

		if (FAILED(hr))
		{
			break;
		}

		if (pSample != NULL)
		{
			hr = pSample->SetSampleTime(llTimeStamp);			
			if (FAILED(hr))
			{
				break;
			}
		}
		
		DWORD totalLength = 0;
		if (pSample) pSample->GetTotalLength(&totalLength);

		//wprintf(L"Stream %d (%I64d) \t %d\n", streamIndex, llTimeStamp, totalLength);
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			wprintf(L"\tEnd of stream\n");
			quit = true;
		}
		if (flags & MF_SOURCE_READERF_NEWSTREAM)
		{
			wprintf(L"\tNew stream\n");
		}
		if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
		{
			wprintf(L"\tNative type changed\n");
		}
		if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
		{
			wprintf(L"\tCurrent type changed\n");
		}
		if (flags & MF_SOURCE_READERF_STREAMTICK)
		{
			//wprintf(L"\tStream tick\n");
		}

		if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
		{
			// The format changed. Reconfigure the decoder.
			//hr = ConfigureDecoder(streamIndex);
			if (FAILED(hr))
			{
				break;
			}
		}

		if (pSample)
		{
			++cSamples;
		}

		out_buffer->writeNext(pSample);
	}

	if (FAILED(hr))
	{
		wprintf(L"ProcessSamples FAILED, hr = 0x%x\n", hr);
	}
	else
	{
		wprintf(L"Processed %d samples\n", cSamples);
	}
	SafeRelease(&pSample);
	return hr;
}

HRESULT MFCapture::ConfigureDecoder(DWORD dwStreamIndex)
{
	IMFMediaType *pNativeType = NULL;
	IMFMediaType *pType = NULL;

	HRESULT hr = pReader->GetNativeMediaType(dwStreamIndex, 0, &pNativeType);
	if (FAILED(hr))
	{
		return hr;
	}

	GUID majorType, subtype;
	hr = pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
	if (FAILED(hr))
	{
		goto done;
	}

	// Define the output type.
	hr = MFCreateMediaType(&pType);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pType->SetGUID(MF_MT_MAJOR_TYPE, majorType);
	if (FAILED(hr))
	{
		goto done;
	}

	// Select a subtype.
	if (majorType == MFMediaType_Video)
	{
		subtype = MFVideoFormat_NV12;
	}
	else if (majorType == MFMediaType_Audio)
	{
		subtype = MFAudioFormat_PCM;
	}
	else
	{
		goto done;
	}

	hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pReader->SetCurrentMediaType(dwStreamIndex, NULL, pType);
	if (FAILED(hr))
	{
		goto done;
	}

done:
	SafeRelease(&pNativeType);
	SafeRelease(&pType);
	return hr;
}

HRESULT MFCapture::EnumerateTypesForStream(DWORD dwStreamIndex)
{
	HRESULT hr = S_OK;
	DWORD dwMediaTypeIndex = 0;

	while (SUCCEEDED(hr))
	{
		IMFMediaType *pType = NULL;
		hr = pReader->GetCurrentMediaType(dwStreamIndex, &pType);

		if (hr == MF_E_NO_MORE_TYPES)
		{
			hr = S_OK;
			break;
		}
		else if (SUCCEEDED(hr))
		{
			GUID format;
			pType->GetGUID(MF_MT_SUBTYPE, &format);

			UINT32 num, den;
			MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den);

			if (isVideo)
			{
				UINT32 w, h, fps, den;
				MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &w, &h);
				MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &fps, &den);

				this->width = w;
				this->height = h;
				this->ratenum = fps;
				this->rateden = den;

				if (format == MFVideoFormat_NV12)
					break;
				if (format == MFVideoFormat_UYVY)
					break;
				if (format == MFVideoFormat_IYUV)
					break;
				if (format == MFVideoFormat_RGB32)
					break;
			}
			else
			{
				this->bps = MFGetAttributeUINT32(pType, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);
				this->sps = MFGetAttributeUINT32(pType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);

				if (format == MFAudioFormat_PCM)
					break;
			}
			
			pType->Release();
		}
		++dwMediaTypeIndex;
	}
	return hr;
}