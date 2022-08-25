// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#define DLL_EXPORT_SYM __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define MATLAB_MEX_FILE

#include "targetver.h"

// Windows and other standard Header Files:
#include <windows.h>
#include <math.h>
#include <atlstr.h>

// MATLAB Header Files:
#include <mat.h>
#include "mex.h"

// Olympus PortManager Header Files:
#include "gt.h"
#include "mdk_if.h"
#include "mdk_type.h"


// MATLAB libraries:
#pragma comment(lib, "libmx.lib")
#pragma comment(lib, "libmex.lib")
#pragma comment(lib, "libmat.lib")


// Olympus PortManager libraries:
//#pragma comment(lib, "msl_pm.lib")