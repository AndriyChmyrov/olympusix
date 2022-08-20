// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "olympusix.h"


#define VALUE	plhs[0]


/*	Driver mutually exclusive access lock (SEMAPHORE).

The semaphore is granted by the mexEnter() function. The owner of the
semaphore must release it with mexLeave().
*/
static HANDLE driver;


/*	Handle of the DLL module itself.
*/
static HANDLE self;


/*	Library initialization and termination.

This function creates a semaphore when a process is attaching to the
library.
*/
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	UNREFERENCED_PARAMETER(ul_reason_for_call);
	UNREFERENCED_PARAMETER(lpReserved);
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		self = hModule;
		driver = CreateSemaphore(NULL, 1, 1, NULL);
		return driver != NULL;
	case DLL_THREAD_ATTACH:
		self = hModule;
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		// next line is actually excessive, because mexCleanup is registered in mexStartup
		//mexCleanup();
		break;
	}
	return TRUE;
}


/*	Request exclusive access.

This function uses a semaphore for granting mutually exclusive access to a
code section between mexEnter()/mexLeave(). If the semaphore is locked, the
function waits up to 500ms before giving up.

The semaphore is created once upon initialization of the library and persists
until the library is unloaded. Code that may raise an exception or lead to a
MEX abort on error must not execute within a mexEnter()/mexLeave section,
otherwise the access may lock permanently.
*/
#ifndef NDEBUG
void mexEnter(const char* file, const int line)
#else
void mexEnter(void)
#endif
{
	switch (WaitForSingleObject(driver, 500))
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:					// access granted
		return;
	default:
#ifndef NDEBUG
		mexPrintf("%s:%d - ", file, line);
#endif
		mexPrintf("Locked.");
		mexErrMsgTxt("Locked.");
	}
}


/*	Release exclusive access.
*/
void mexLeave(void)
{
	ReleaseSemaphore(driver, 1, NULL);
}


/*	Check for an error and print the message.

This function is save within a mexEnter()/mexLeave section.
*/
#ifndef NDEBUG
void mexMessage(const char* file, const int line, unsigned error)
#else
void mexMessage(unsigned error)
#endif
{
	if (error != 0 )
	{
		MEXLEAVE;
#ifndef NDEBUG
		mexPrintf("%s:%d ", file, line);
#endif

		if (!error)
		{
			mexErrMsgTxt("Error!");
		}
	}
}


int	CALLBACK _ErrorCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller)
{
	UNREFERENCED_PARAMETER(pCaller);
	UNREFERENCED_PARAMETER(pContext);
	UNREFERENCED_PARAMETER(pv);
	UNREFERENCED_PARAMETER(lParam);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(MsgId);
#ifndef NDEBUG
	mexPrintf("Olympus error call!\n");
#endif
	return 0;
}


int	CALLBACK _NotifyCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller)
{
	UNREFERENCED_PARAMETER(pCaller);
	UNREFERENCED_PARAMETER(pContext);
	UNREFERENCED_PARAMETER(pv);
	UNREFERENCED_PARAMETER(lParam);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(MsgId);
#ifndef NDEBUG
	mexPrintf("Olympus notify call!\n");
#endif
	return 0;
}


/*	Free the microscope and the driver.
*/
void mexCleanup(void)
{
	if (olympusIXs >= 0)
	{
		while (olympusIXs)
		{
			olympusIX = olympusIXs;

			if (ixObj)
			{
				delete static_cast<ixState*>(ixObj);
				ixObj = NULL;
			}

			olympusIXs--;
		}

		if (hMod)
		{
			BOOL fFreeResult;
			fFreeResult = FreeLibrary(hMod);
			if (!fFreeResult)
			{
				mexPrintf("Olympus PortManager library did not unload correctly!\n");
			}
			else
			{
				hMod = NULL;
#ifndef NDEBUG
				mexPrintf("Olympus PortManager library unloaded correctly!\n");
#endif
			}
		}

#ifndef NDEBUG
		mexPrintf("OlympusIX connection shutted down!\n");
#endif
	}

}


/*	Initialize OlympusIX driver and get microscopes.
*/
int mexStartup(void)
{
	mexAtExit(mexCleanup);
	if (olympusIXs < 0)
	{
		// Load SDK library
		char path[_MAX_FNAME];
		HMODULE hm = NULL;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&self,
			&hm))
		{
			int ret = GetLastError();
			char errmsg[256] = {'\0'};
			sprintf_s(errmsg, sizeof(errmsg), "GetModuleHandle returned error %d!\n", ret);
			mexErrMsgTxt(errmsg);
		}
		GetModuleFileNameA(hm, path, sizeof(path));
		
		CString	szPort = path;
		szPort.Replace(_T("olympusix.mexw64"), _T(""));
		szPort += GT_MDK_PORT_MANAGER;

		hMod = LoadLibrary(szPort);
		if (hMod == NULL) {
			mexErrMsgTxt("Could not load Olympus library!");
		}
		else
		{
#ifndef NDEBUG
			mexPrintf("Olympus PortManager library loaded sucessfully!\n");
#endif
		}

		olympusIX = 1;
	}

	return olympusIX;
}

MEXFUNCTION_LINKAGE
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	static ixState *ixStates;
	if (nrhs == 0 && nlhs == 0)
	{
		mexPrintf("\nOlympusIX microscope interface.\n\n\tAndriy Chmyrov © 20.05.2016\n\n");
		return;
	}

	if (driver == NULL) mexErrMsgTxt("Semaphore not initialized.");

	if (olympusIX == -1)
	{
		char cCom[StringLength];
		mxGetString(prhs[0], cCom, StringLength);
		if (_stricmp("close", cCom) == 0) return;

		if (mexStartup() == 0) return;
		olympusIX = 1;
		ixStates = new ixState(hMod);
		ixObj = static_cast<HANDLE>(ixStates);
	}

	int n = 0;
	while (n < nrhs)
	{
		SHORT index;
		int field;
		switch (mxGetClassID(prhs[n]))
		{
		default:
			mexErrMsgTxt("Parameter name expected as string.");
		case mxCHAR_CLASS:
		{
			char read[StringLength];
			if (mxGetString(prhs[n], read, StringLength)) mexErrMsgTxt("Unknown parameter.");
			if (++n < nrhs)
			{
				ixStates->setParameter(read, prhs[n]);
				break;
			}
			if (nlhs > 1) mexErrMsgTxt("Too many output arguments.");
			VALUE = ixStates->getParameter(read);
			return;
		}
		case mxSTRUCT_CLASS:
			for (index = 0; index < static_cast<int>(mxGetNumberOfElements(prhs[n])); index++)
				for (field = 0; field < mxGetNumberOfFields(prhs[n]); field++)
					ixStates->setParameter(mxGetFieldNameByNumber(prhs[n], field), mxGetFieldByNumber(prhs[n], index, field));
			;
		}
		n++;
	}
	switch (nlhs)
	{
	default:
		mexErrMsgTxt("Too many output arguments.");
	case 1:
		VALUE = mxCreateDoubleScalar(static_cast<double>(olympusIX));
	case 0:;
	}

}