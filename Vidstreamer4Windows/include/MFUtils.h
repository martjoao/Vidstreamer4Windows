#ifndef _LAVID_MF_UTILS_H
#define _LAVID_MF_UTILS_H

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

class MFUtils {

public:
	//initialize COM library and Media Foundation
	static bool initializeMF();

private:
	MFUtils();
};

#endif
