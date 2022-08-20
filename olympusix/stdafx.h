// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#define DLL_EXPORT_SYM __declspec(dllexport)

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <mat.h>
#include <math.h>
#include <atlstr.h>



// TODO: reference additional headers your program requires here
#include "mex.h"
#include "gt.h"
#include "mdk_if.h"
#include "mdk_type.h"


#pragma comment(lib, "libmx.lib")
#pragma comment(lib, "libmex.lib")
#pragma comment(lib, "libmat.lib")


//#pragma comment(lib, "msl_pm.lib")
