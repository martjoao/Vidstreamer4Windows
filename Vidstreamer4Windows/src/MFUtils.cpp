#include "MFUtils.h"

#include <iostream>

MFUtils::MFUtils()
{

}

bool MFUtils::initializeMF()
{
	HRESULT hr;

	bool coInitialized = false;
	bool mfInitialized = false;

	hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (hr == S_OK || hr == S_FALSE)
	{
		coInitialized = true;
	}
	else
	{
		std::cout << "Failed to initialize COM" << std::endl;
		return false;
	}

	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
	{
		std::cout << "Failed to initialize Media Foundation" << std::endl;
		return false;
	}
	return true;
}
