// olympusix.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "olympusix.h"

/*	Selected microscope.
*/
__int32 olympusIX = -1;

/*	Number of available microscopes or -1 if driver not initialized.
*/
__int32 olympusIXs = -1;

/*	OlympusIX object. Unused handles are set to 0.
*/
HANDLE ixObj = NULL;
HMODULE	hMod = NULL;


/*	Check for and read a scalar value.
*/
double getScalar(const mxArray* array)
{
	if (!mxIsNumeric(array) || mxGetNumberOfElements(array) != 1) mexErrMsgTxt("Not a scalar.");
	return mxGetScalar(array);
}


/*	Check for and read a string
*/
char* getString(const mxArray* array, const int StrLength = MAX_RESPONSE_SIZE)
{
	if (!mxIsChar(array))
	{
		mexErrMsgIdAndTxt("MATLAB:mxcreatecharmatrixfromstr:invalidInputType",
			"Input must be of type char.");
	}

	/* Allocate enough memory to hold the converted string. */
	size_t ValueLen = mxGetNumberOfElements(array) + 1;
	if (static_cast<int>(ValueLen) >= StrLength)
	{
		mexErrMsgTxt("Supplied string is longer than the maximum supported string length.");
	}
	char* Value;
	Value = static_cast<char*>(mxCalloc(ValueLen, sizeof(char)));
	mxGetString(array, Value, ValueLen);
	return Value;
}


ixState::ixState(HMODULE	hMod_)
{
	hMod = hMod_;
	//-----------------------------------------------------
	// get function address
	pfn_InifInterface = (fn_MSL_PM_Initialize)GetProcAddress(hMod, "MSL_PM_Initialize");
	if (pfn_InifInterface == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_Initialize!");
	}

	pfn_EnumInterface = (fn_MSL_PM_EnumInterface)GetProcAddress(hMod, "MSL_PM_EnumInterface");
	if (pfn_EnumInterface == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_EnumInterface!");
	}

	pfn_GetInterfaceInfo = (fn_MSL_PM_GetInterfaceInfo)GetProcAddress(hMod, "MSL_PM_GetInterfaceInfo");
	if (pfn_GetInterfaceInfo == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_GetInterfaceInfo!");
	}

	pfn_OpenInterface = (fn_MSL_PM_OpenInterface)GetProcAddress(hMod, "MSL_PM_OpenInterface");
	if (pfn_OpenInterface == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_OpenInterface!");
	}

	pfn_CloseInterface = (fn_MSL_PM_CloseInterface)GetProcAddress(hMod, "MSL_PM_CloseInterface");
	if (pfn_CloseInterface == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_CloseInterface!");
	}

	pfn_RegisterCallback = (fn_MSL_PM_RegisterCallback)GetProcAddress(hMod, "MSL_PM_RegisterCallback");
	if (pfn_RegisterCallback == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_RegisterCallback!");
	}

	pfn_SendCommand = (fn_MSL_PM_SendCommand)GetProcAddress(hMod, "MSL_PM_SendCommand");
	if (pfn_SendCommand == NULL) {
		mexErrMsgTxt("Could not find MSL_PM_SendCommand!");
	}

	//-----------------------------------------------------
	//	call initialize function
	int	ret;
	if (pfn_InifInterface)
		ret = (*pfn_InifInterface)();

	int count = -1;
	int iter = 1;
	while (iter < 5)
	{
		if (pfn_EnumInterface)
			count = (*pfn_EnumInterface)();
		if (!count)
		{
			Sleep(200);
			iter++;
			//			mexErrMsgTxt("No OlympusIX interfaces found!");
		}
		else
		{
			olympusIXs = count;
#ifndef NDEBUG
			mexPrintf("%d OlympusIX interface found!\n", count);
#endif
			break;
		}
	}

	if (!count)
	{
		mexErrMsgTxt("No OlympusIX interfaces found!");
	}

	memset(&m_Cmd, 0, sizeof(m_Cmd));

	int		i = 0;
	ifData = NULL;
	if (pfn_GetInterfaceInfo)
		ret = (*pfn_GetInterfaceInfo)(i, &ifData);

	m_iMUid[0] = 266;
	m_iMUid[1] = 306;
	m_iMirrorCurr[0] = 1;
	m_iMirrorCurr[1] = 1;

	m_iShutterCurr[0] = 1;
	m_iShutterCurr[0] = 1;

	m_iObservationMethods = -1;

	m_bMUcurrentInitialized = false;
	m_bShuttercurrentInitialized = false;
	m_bObjectiveInitialized = false;
	m_bLightpathInitialized = false;
	m_bMainDeckInitialized = false;

	for (i = 0; i < MAX_NUM_OBSERVATION_METHODS; i++)
		m_bOMinitialized[i] = false;

	timeout = 10000;

	m_bOpened = false;
	m_bLoggedin = false;
	m_bResponded = false;
	m_bResponseRead = true;

	m_bFrameLights = true;
	m_bPanelLights = true;
}


//-----------------------------------------------------------------------------
//	COMMAND: call back entry from SDK port manager
int	CALLBACK CommandCallback(
	ULONG		MsgId,			// Callback ID.
	ULONG		wParam,			// Callback parameter, it depends on callback event.
	ULONG		lParam,			// Callback parameter, it depends on callback event.
	PVOID		pv,				// Callback parameter, it depends on callback event.
	PVOID		pContext,		// This contains information on this call back function.
	PVOID		pCaller			// This is the pointer specified by a user in the requirement function.
)
{
	UNREFERENCED_PARAMETER(MsgId);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	UNREFERENCED_PARAMETER(pContext);
	UNREFERENCED_PARAMETER(pCaller);
	UNREFERENCED_PARAMETER(pv);

	ixState* ixStates = (ixState*)pContext;
	if (ixStates == NULL)	return	0;
	/////////////////////////////////////////////////////////
	//	show result.
	if (pContext != NULL) {
		ixStates->setResponse(true, reinterpret_cast<char*>(ixStates->m_Cmd.m_Rsp));
#ifndef NDEBUG
		const size_t len = strlen(reinterpret_cast<char*>(ixStates->m_Cmd.m_Cmd));
		char* orig_cmd;
		orig_cmd = static_cast<char*>(mxCalloc(len, sizeof(char)));
		strncpy_s(orig_cmd, len, reinterpret_cast<char*>(ixStates->m_Cmd.m_Cmd), len - 2);

		//mexPrintf("Olympus response to command \"%s\" - \"%s\"\n", orig_cmd, ixStates->m_Cmd.m_Rsp);
		//mexEvalString("drawnow"); // nice to have - but generates Matlab crash quite often!
		mxFree(orig_cmd);
#endif
	}
	else {
		mexPrintf("Olympus command error!\n");
	}
	return	0;
}


bool executeOMsequence(ixState* ixStates, char* cAction, size_t bSeqNumber)
{
	// Adjust shutter mode for OMSEQ command
	// i.e. close the shutter and then put it to open (4) or closed (2) state
	for (int kDeck = 0; kDeck < MAX_NUM_DECKS; kDeck++)
	{
		if ((bSeqNumber == 1) && (kDeck == 1))
		{
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SHM%d %d", kDeck + 1, 4); // special case for Seq1 == BF -> make shutter opened after sequence
			//ixStates->m_iShutterCurr[kDeck] = shutter_open;
		}
		else
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SHM%d %d", kDeck + 1, ixStates->m_iShutterCurr[kDeck] == shutter_closed ? 2 : 4);
		ixStates->send_and_wait(cAction, true);
	}

	if (ixStates->m_bOMinitialized[bSeqNumber])
	{
		sprintf_s(cAction, MAX_RESPONSE_SIZE, "OMSEQ %s", ixStates->m_cOMparameters[bSeqNumber]);
		ixStates->send_and_wait(cAction, true);
	}

	// return to 'normal mode' of operation for shutters
	for (int kDeck = 0; kDeck < MAX_NUM_DECKS; kDeck++)
	{
		sprintf_s(cAction, MAX_RESPONSE_SIZE, "SHM%d 1", kDeck + 1);
		ixStates->send_and_wait(cAction, true);
	}

	// display active observation mode on the TPC unit
	for (size_t idx = 0; idx < MAX_NUM_OBSERVATION_METHODS; ++idx)
	{
		sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,%d", 171 + static_cast<int>(idx), (bSeqNumber == idx) ? 2 : 1);
		ixStates->send_and_wait(cAction, true);
	}

	return true;
}

//-----------------------------------------------------------------------------
//	NOTIFICATION: call back entry from SDK port manager
int	CALLBACK NotifyCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller)
{
	UNREFERENCED_PARAMETER(MsgId);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	UNREFERENCED_PARAMETER(pv);
	UNREFERENCED_PARAMETER(pContext);
	UNREFERENCED_PARAMETER(pCaller);

	ixState* ixStates = static_cast<ixState*>(pContext);
	if (ixStates == NULL)	return	0;

	if (pContext != NULL)
	{
		char msg[MAX_RESPONSE_SIZE];
		strcpy_s(msg, MAX_RESPONSE_SIZE, static_cast<char*>(pv));
#ifndef NDEBUG
		// nice info - but generates crashes of Matlab
//		mexPrintf("Olympus notify call: \"%s\"\n", msg);
#endif
		int val, state;
		int deck;

		char cAction[MAX_RESPONSE_SIZE];

		// Handle host key actions etc
		if (2 == sscanf_s(msg, "SK %d,%d", &val, &state) && state == 1)
		{
			if (val == 51) // MCZ button - Observation Method Fluorescence
			{
				executeOMsequence(ixStates, cAction, 0);
			}
			else if (val == 54) // MCZ button - Observation Method Bright Field
			{
				executeOMsequence(ixStates, cAction, 1);
			}
			else if (val == 55) // MCZ shutter button - open/close shutter on the main deck
			{
				if (!ixStates->m_bMainDeckInitialized)
				{
					return 0; // Main Deck needs to initialized
				}

				// process shutter state change
				int iNewState = -1;
				if (ixStates->m_iShutterCurr[ixStates->m_iMainDeck - 1] == 0)
				{
					// shutter on the main deck was open, needs to close
					iNewState = 1;
				}
				else if (ixStates->m_iShutterCurr[ixStates->m_iMainDeck - 1] == 1)
				{
					// shutter on the main deck was closed, needs to open
					iNewState = 0;
				}
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "ESH%d %d", ixStates->m_iMainDeck, iNewState);
				ixStates->send_and_wait(cAction, true);
				ixStates->m_iShutterCurr[ixStates->m_iMainDeck - 1] = iNewState;

				// process display of the shutter state change
				int iCommandNumber = -1;
				if (ixStates->m_iMainDeck == 1)
				{
					iCommandNumber = 261;
				}
				else if (ixStates->m_iMainDeck == 2)
				{
					iCommandNumber = 301;
				}
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,%d", iCommandNumber, iNewState + 1);
				ixStates->send_and_wait(cAction, true);
			}
			else if (val == 58) // MCZ button - move to the next mirror on the main deck
			{
				if (!ixStates->m_bMainDeckInitialized)
				{
					return 0; // Main Deck needs to initialized
				}

				// first deactivate display status of the current mirror
				deck = ixStates->m_iMainDeck - 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
				ixStates->send_and_wait(cAction, true);
				Sleep(100);

				// process change of the mirror on the main deck
				if (ixStates->m_iMirrorCurr[deck] < 8)
					ixStates->m_iMirrorCurr[deck]++;
				else
					ixStates->m_iMirrorCurr[deck] = 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "MUSEQ2 %d", ixStates->m_iMirrorCurr[deck]);
				ixStates->send_and_wait(cAction, true);

				// then activate display status of the new mirror
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
				ixStates->send_and_wait(cAction, true);
				ixStates->m_iMirrorCurr[deck] = val;
			}
			else if (val == 59) // MCZ button - move to the previous mirror on the main deck
			{
				if (!ixStates->m_bMainDeckInitialized)
				{
					return 0; // Main Deck needs to initialized
				}

				// first deactivate display status of the current mirror
				deck = ixStates->m_iMainDeck - 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
				ixStates->send_and_wait(cAction, true);
				Sleep(100);

				// process change of the mirror on the main deck
				if (ixStates->m_iMirrorCurr[deck] > 1)
					ixStates->m_iMirrorCurr[deck]--;
				else
					ixStates->m_iMirrorCurr[deck] = 8;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "MUSEQ2 %d", ixStates->m_iMirrorCurr[deck]);
				ixStates->send_and_wait(cAction, true);

				// then activate display status of the new mirror
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
				ixStates->send_and_wait(cAction, true);
				ixStates->m_iMirrorCurr[deck] = val;
			}
			else if (val >= 151 && val <= 156) // set objective 
			{
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "OBSEQ %d", val - 150);
				ixStates->send_and_wait(cAction, true);
			}
			else if (val >= 171 && val <= 177) // set observation method sequence
			{
				int seqNumber = val - 171;
				if (ixStates->m_bOMinitialized[seqNumber])
				{
					executeOMsequence(ixStates, cAction, seqNumber);
				}
			}
			else if (val == 261 || val == 301) // TPC unit - shuter on deck1
			{
				if (val == 261) deck = 1; else deck = 2;
				if (ixStates->m_iShutterCurr[deck - 1] == 1)
				{
					// shutter was closed, need to open
					ixStates->m_iShutterCurr[deck - 1] = 0;
				}
				else if (ixStates->m_iShutterCurr[deck - 1] == 0)
				{
					// shutter was opened, need to close
					ixStates->m_iShutterCurr[deck - 1] = 1;
				}
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "ESH%d %d", deck, ixStates->m_iShutterCurr[deck - 1]);
				ixStates->send_and_wait(cAction, true);
			}
			else if (val >= 266 && val <= 273) // set mirror on deck1
			{
				deck = 1;
				deck = deck - 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
				ixStates->send_and_wait(cAction, true);
				ixStates->m_iMirrorCurr[deck] = val - 266 + 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "MUSEQ1 %d", ixStates->m_iMirrorCurr[deck]);
				ixStates->send_and_wait(cAction, true);
			}
			else if (val >= 306 && val <= 313) // set mirror on deck2
			{
				deck = 2;
				deck = deck - 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
				ixStates->send_and_wait(cAction, true);
				ixStates->m_iMirrorCurr[deck] = val - 306 + 1;
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "MUSEQ2 %d", ixStates->m_iMirrorCurr[deck]);
				ixStates->send_and_wait(cAction, true);
			}
			else if (val == 354 || val == 67) // escape button pressed on TPC unit or MCZ unit
				ixStates->send_and_wait("FG 0", true);
		}

		// mirror unit change
		if ((2 == sscanf_s(msg, "NMU%d %d", &deck, &val)) && ixStates->m_bMUcurrentInitialized)
		{
			deck = deck - 1;
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", ixStates->m_iMUid[deck] + ixStates->m_iMirrorCurr[deck] - 1);
			ixStates->send_and_wait(cAction, true);
			Sleep(100);
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", ixStates->m_iMUid[deck] + val - 1);
			ixStates->send_and_wait(cAction, true);
			ixStates->m_iMirrorCurr[deck] = val;
		}

		// shutter in deck change
		if ((2 == sscanf_s(msg, "NESH%d %d", &deck, &val)) && ixStates->m_bShuttercurrentInitialized)
		{
			int iCommandNumber;
			if (deck == 1)
				iCommandNumber = 261;
			else if (deck == 2)
				iCommandNumber = 301;
			else
				return 0;
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,%d", iCommandNumber, val + 1);
			ixStates->send_and_wait(cAction, true);
		}

		// objective change
		if ((1 == sscanf_s(msg, "NOB %d", &val)) && ixStates->m_bObjectiveInitialized)
		{
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", ixStates->m_iObjective + 150);
			ixStates->send_and_wait(cAction, true);
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", val + 150);
			ixStates->send_and_wait(cAction, true);
			ixStates->m_iObjective = val;
		}
	}

	return 0;
}


void ixState::setResponse(bool bResponse, const char* cResponse)
{
	UNREFERENCED_PARAMETER(cResponse);
	m_bResponded = bResponse;
	memcpy_s(m_cResponse, MAX_RESPONSE_SIZE, m_Cmd.m_Rsp, MAX_RESPONSE_SIZE);
	return;
}


bool ixState::getResponse(char* cResponse)
{
	memcpy_s(cResponse, MAX_RESPONSE_SIZE, m_cResponse, MAX_RESPONSE_SIZE);
	m_bResponseRead = true;
	return m_bResponded;
}


bool ixState::getResponded(void)
{
	return m_bResponded;
}


bool ixState::getResponseRead(void)
{
	return m_bResponseRead;
}


bool ixState::open(void)
{
	bool	bRes;
	bRes = (*pfn_OpenInterface)(ifData);
	if (!bRes)
	{
		mexErrMsgTxt("Could not open OlympusIX interface!");
	}
	else
	{
		m_bOpened = TRUE;
#ifndef NDEBUG
		mexPrintf("OlympusIX interface succesfuly opened!\n");
#endif
	}

	bRes = (*pfn_RegisterCallback)(ifData, CommandCallback, NotifyCallback, ErrorCallback, this);
	if (!bRes)
	{
		mexErrMsgTxt("Could not register Callback function!");
	}
	else
	{
#ifndef NDEBUG
		mexPrintf("Callback function succesfuly registered!\n");
#endif
	}

	return bRes;
}


bool ixState::close(void)
{
	bool	bRes = false;
	if (m_bOpened)
	{
		logout();

		if (ifData)
		{
			bRes = (*pfn_CloseInterface)(ifData);
			if (!bRes)
			{
				mexPrintf("OlympusIX interface did not close correctly!\n");
			}
			else
			{
				m_bOpened = false;
#ifndef NDEBUG
				mexPrintf("OlympusIX interface closed correctly!\n");
#endif
			}
		}
		else
		{
#ifndef NDEBUG
			mexPrintf("OlympusIX ifData parameter is not set!\nUnable to close OlympusIX interface!\n");
#endif
		}
	}

	return bRes;
}


bool ixState::send(const char* c_cmdIn, bool bCallbackCall = false)
{
	UNREFERENCED_PARAMETER(bCallbackCall);
	if (!m_bOpened)
		mexErrMsgTxt("OlympusIX interface is not yet opened!");

	if (_stricmp("L 0,0", c_cmdIn) == 0)
		MEXLEAVE;

	MEXENTER;

	memset(&m_Cmd, 0x00, sizeof(MDK_MSL_CMD));

	const size_t len = strlen(c_cmdIn);
	char	cmda[MAX_RESPONSE_SIZE] = { '\0' };

	memcpy(cmda, c_cmdIn, len);
	cmda[len] = '\r';
	cmda[len + 1] = '\n';

	strcpy_s((char*)m_Cmd.m_Cmd, sizeof(m_Cmd.m_Cmd), cmda);

	////////////////////////////////////////////////////////////
	// set parameters for command
	m_Cmd.m_CmdSize = static_cast<DWORD>(len + 2);
	m_Cmd.m_Callback = CommandCallback;
	m_Cmd.m_Context = NULL;		// this pointer passed by pv
	m_Cmd.m_Timeout = timeout;	// (ms)
	m_Cmd.m_Sync = FALSE;
	m_Cmd.m_Command = TRUE;		// TRUE: Command , FALSE: it means QUERY form ('?').

	bool bRes;
	bRes = (*pfn_SendCommand)(ifData, &m_Cmd);
	if (!bRes)
	{
		mexPrintf("Olympus command \"%s\" did not send succesfully!\n", c_cmdIn);
		return false;
	}
	else
	{
		m_bResponded = false;
		m_cResponse[0] = '\0';
		m_bResponseRead = false;
#ifndef NDEBUG
		if (!bCallbackCall)
		{
			mexPrintf("Olympus command \"%s\" sent succesfully!\n", c_cmdIn);
		}
#endif
		if (_stricmp("L 1,0", c_cmdIn) == 0 || _stricmp("L 1,1", c_cmdIn) == 0)
		{
			m_bLoggedin = true;
		}
	}
#ifndef NDEBUG
	if (!bCallbackCall)
		mexEvalString("drawnow");
#endif
	Sleep(50);

	MEXLEAVE;
	return true;
}


bool ixState::send_and_wait(const char* c_cmdIn, bool bCallbackCall = false)
{
	if (!m_bOpened)
		mexErrMsgTxt("OlympusIX interface is not yet opened!");
	bool bRes;
	bRes = send(c_cmdIn, bCallbackCall);
	if (!bCallbackCall)
		return waitForResponse();
	else
		return bRes;
}


bool ixState::waitForResponse(void)
{
	byte ni = 0;
	while (ni < 100)
	{
		if (getResponded())
		{
#ifndef NDEBUG
			if (ni)
				mexPrintf("Waiting for %.1f seconds helped to get the response!\n", (double)(ni) * 0.1);
#endif
			return true;
		}
		else
			Sleep(100);
		ni++;
	}
#ifndef NDEBUG
	mexPrintf("Waiting for %.1f seconds didn't help to get the response\n", (double)ni * 0.1);
#endif
	return false;
}


bool ixState::logout(void)
{
	bool bRes = false;
	if (m_bOpened && m_bLoggedin)
	{
#ifndef NDEBUG
		mexPrintf("Login status was true, trying to log out!\n");
#endif
		if (!m_bFrameLights)
		{
#ifndef NDEBUG
			mexPrintf("Frame lights were switched off, trying to switch them on!\n");
#endif
			setParameter("Frame_lights", mxCreateDoubleScalar(1));
		}

		if (!m_bPanelLights)
		{
#ifndef NDEBUG
			mexPrintf("Touch Panel lights were switched off, trying to switch them on!\n");
#endif
			setParameter("Panel_lights", mxCreateDoubleScalar(1));
		}

		bRes = send_and_wait("L 0,0");
		if (bRes)
		{
#ifndef NDEBUG
			mexPrintf("Log out succeeded!\n");
#endif
			m_bLoggedin = false;
		}
	}
	return bRes;
}


bool ixState::isOpened(void)
{
#ifndef NDEBUG
	if (m_bOpened)
		mexPrintf("OlympusIX interface is currently opened!\n");
#endif
	return m_bOpened;
}


bool ixState::isLoggedin(void)
{
#ifndef NDEBUG
	if (m_bLoggedin)
		mexPrintf("Login status is currently true!\n");
#endif
	return m_bLoggedin;
}


ixState::~ixState(void)
{
	close();
}


/*	Get a parameter or status value.
*/
//mxArray* getParameter(ixState* ixStates, const char* name)
mxArray* ixState::getParameter(const char* name)
{
	bool bRes = false;
	char cResponse[MAX_RESPONSE_SIZE];

	if (_stricmp("reset", name) == 0)
	{
		return mxCreateDoubleScalar(1);
	}

	if (_stricmp("open", name) == 0)
	{
		bRes = open();
		return mxCreateDoubleScalar(static_cast<double>(bRes));;
	}

	if (_stricmp("close", name) == 0)
	{
		bRes = close();
		return mxCreateDoubleScalar(static_cast<double>(bRes));;
	}

	if (_stricmp("frame_lights", name) == 0)
	{
		return mxCreateDoubleScalar(static_cast<double>(m_bFrameLights));;
	}

	if (_stricmp("panel_lights", name) == 0)
	{
		return mxCreateDoubleScalar(static_cast<double>(m_bPanelLights));;
	}

	if (_stricmp("responded", name) == 0)
	{
		return mxCreateDoubleScalar(static_cast<double>(getResponded()));;
	}

	if (_stricmp("response", name) == 0)
	{
		bRes = getResponse(cResponse);
		if (bRes)
		{
			return mxCreateString(cResponse);
		}
		else
		{
			return mxCreateString('\0');
		}
	}

	if (_stricmp("responseread", name) == 0)
	{
		return mxCreateDoubleScalar(static_cast<double>(getResponseRead()));;
	}

	if (_stricmp("focusposition", name) == 0)
	{
		bRes = send_and_wait("FP?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			sscanf_s(cResponse, "%*s %d", &i);
			return mxCreateDoubleScalar(static_cast<double>(i));
		}
		else
		{
			return mxCreateDoubleScalar(static_cast<double>(-1));
		}
	}

	if (_stricmp("focusposition_um", name) == 0)
	{
		bRes = send_and_wait("FP?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			sscanf_s(cResponse, "%*s %d", &i);
			return mxCreateDoubleScalar(static_cast<double>(i) / 100);
		}
		else
		{
			return mxCreateDoubleScalar(static_cast<double>(-1));
		}
	}

	if (_stricmp("lightpath", name) == 0)
	{
		bRes = send_and_wait("BIL?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			if (sscanf_s(cResponse, "%*s %d", &i) == 1)
			{
				m_iLightpath = i;
				m_bLightpathInitialized = true;
				//char cAction[MAX_RESPONSE_SIZE];
				//sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", i + 150);
				//send_and_wait(cAction, true);
				return mxCreateDoubleScalar(static_cast<double>(i));
			}
		}
		return mxCreateDoubleScalar(-1);
	}

	if (_stricmp("maindeck", name) == 0)
	{
		if (m_bMainDeckInitialized)
		{
			return mxCreateDoubleScalar(static_cast<double>(m_iMainDeck));
		}
		else
		{
			bRes = send_and_wait("MDC?");
			bRes = getResponse(cResponse);
			if (bRes)
			{
				int i;
				if (sscanf_s(cResponse, "%*s %d", &i) == 1)
				{
					m_iMainDeck = i;
					m_bMainDeckInitialized = true;
					return mxCreateDoubleScalar(static_cast<double>(i));
				}
			}
		}
		return mxCreateDoubleScalar(-1);
	}

	if (_stricmp("mirrordeck1", name) == 0)
	{
		bRes = send_and_wait("MU1?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			sscanf_s(cResponse, "%*s %d", &i);
			int deck = 1 - 1;
			m_iMirrorCurr[deck] = i;
			char cAction[MAX_RESPONSE_SIZE];
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", m_iMUid[deck] + m_iMirrorCurr[deck] - 1);
			send_and_wait(cAction);
			return mxCreateDoubleScalar(static_cast<double>(i));
		}
		else
		{
			mxFree(cResponse);
			return mxCreateDoubleScalar(static_cast<double>(-1));
		}
	}

	if (_stricmp("mirrordeck2", name) == 0)
	{
		bRes = send_and_wait("MU2?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			sscanf_s(cResponse, "%*s %d", &i);
			int deck = 2 - 1;
			m_iMirrorCurr[deck] = i;
			char cAction[MAX_RESPONSE_SIZE];
			sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", m_iMUid[deck] + m_iMirrorCurr[deck] - 1);
			send_and_wait(cAction);
			m_bMUcurrentInitialized = true;
			return mxCreateDoubleScalar(static_cast<double>(i));
		}
		else
		{
			mxFree(cResponse);
			return mxCreateDoubleScalar(static_cast<double>(-1));
		}
	}

	if (_stricmp("objective", name) == 0)
	{
		bRes = send_and_wait("OB?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			if (sscanf_s(cResponse, "%*s %d", &i) == 1)
			{
				m_iObjective = i;
				m_bObjectiveInitialized = true;
				char cAction[MAX_RESPONSE_SIZE];
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", i + 150);
				send_and_wait(cAction);
				return mxCreateDoubleScalar(static_cast<double>(i));
			}
		}
		return mxCreateDoubleScalar(-1);
	}

	if (_stricmp("shutterdeck1", name) == 0)
	{
		bRes = send_and_wait("ESH1?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			sscanf_s(cResponse, "%*s %d", &i);
			int deck = 1;
			m_iShutterCurr[deck - 1] = i;
			char cAction[MAX_RESPONSE_SIZE];
			if (m_iShutterCurr[deck - 1] == 0)
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", 261);
			else if (m_iShutterCurr[deck - 1] == 1)
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", 261);
			send_and_wait(cAction);
			m_bShuttercurrentInitialized = true;
			return mxCreateDoubleScalar(static_cast<double>(i));
		}
		else
		{
			return mxCreateDoubleScalar(static_cast<double>(-1));
		}
	}

	if (_stricmp("shutterdeck2", name) == 0)
	{
		bRes = send_and_wait("ESH2?");
		bRes = getResponse(cResponse);
		if (bRes)
		{
			int i;
			sscanf_s(cResponse, "%*s %d", &i);
			int deck = 2;
			m_iShutterCurr[deck - 1] = i;
			char cAction[MAX_RESPONSE_SIZE];
			if (m_iShutterCurr[deck - 1] == 0)
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,1", 301);
			else if (m_iShutterCurr[deck - 1] == 1)
				sprintf_s(cAction, MAX_RESPONSE_SIZE, "SKD %d,2", 301);
			send_and_wait(cAction);
			m_bShuttercurrentInitialized = true;
			return mxCreateDoubleScalar(static_cast<double>(i));
		}
		else
		{
			return mxCreateDoubleScalar(static_cast<double>(-1));
		}
	}

	if (_stricmp("waitforresponse", name) == 0)
	{
		return mxCreateDoubleScalar(static_cast<double>(waitForResponse()));
	}

	if (_stricmp("windowstatus", name) == 0)
	{
		bRes = send_and_wait("WS?");
		bRes = getResponse(cResponse);
		int i;
		sscanf_s(cResponse, "%*s %d", &i);
		return mxCreateDoubleScalar(static_cast<double>(i));
	}


	bRes = send_and_wait(name);

	/*
#ifndef NDEBUG
	mexPrintf("%s:%d - ", __FILE__, __LINE__);
#endif
	mexPrintf("\"%s\" unknown.\n", name);
	*/
	return mxCreateDoubleScalar(static_cast<double>(bRes));;
}


/*	Set a device parameter.
*/
void ixState::setParameter(const char* name, const mxArray* field)
{
	if (mxGetNumberOfElements(field) < 1) return;
	if (_stricmp("olympusIX", name) == 0) return;
	if (_stricmp("olympusIXs", name) == 0) return;
	if (olympusIX < 1 || olympusIX > olympusIXs) mexErrMsgTxt("Invalid olympusIX handle.");

	bool bRes = false;
	char cCommand[MAX_RESPONSE_SIZE];

	if (_stricmp("frame_lights", name) == 0)
	{
		int status = static_cast<int>(getScalar(field));
		if (status != 0 && status != 1)
			mexErrMsgTxt("Only 0 and 1 are allowed as the Frame Lights states!");
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "IXIL %d", status);
		bRes = send_and_wait(cCommand);
		if (bRes)
		{
			m_bFrameLights = static_cast<bool>(status);
		}
		return;
	}
	else if (_stricmp("focusposition", name) == 0)
	{
		int zpos = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "FG %d", zpos);
		bRes = send_and_wait(cCommand);
		return;
	}
	else if (_stricmp("focusposition_um", name) == 0)
	{
		int zpos = static_cast<int>(getScalar(field) * 100);
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "FG %d", zpos);
		bRes = send_and_wait(cCommand);
		return;
	}
	else if (_stricmp("lightpath", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "BIL %d", val);
		bRes = send_and_wait(cCommand);
		m_iLightpath = val;
		m_bLightpathInitialized = true;
		return;
	}
	else if (_stricmp("maindeck", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "S_MDC %d", val);
		bRes = send_and_wait(cCommand);
		m_iMainDeck = val;
		m_bMainDeckInitialized = true;
		return;
	}
	else if (_stricmp("mirrordeck1", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "MUSEQ1 %d", val);
		bRes = send_and_wait(cCommand);
		return;
	}
	else if (_stricmp("mirrordeck2", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "MUSEQ2 %d", val);
		bRes = send_and_wait(cCommand);
		return;
	}
	else if (_stricmp("objective", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "OBSEQ %d", val);
		bRes = send_and_wait(cCommand);
		return;
	}
	else if (_stricmp("observationmethod", name) == 0)
	{
		size_t val = static_cast<size_t>(getScalar(field));
		char cAction[MAX_RESPONSE_SIZE];
		executeOMsequence(this, cAction, val - 1);
		return;
	}
	else if (_stricmp("observationmethods", name) == 0)
	{
		char* cMethods;
		cMethods = getString(field);
		char* cTmp = cMethods;

		int iCount;
		for (iCount = 0; cTmp[iCount]; cTmp[iCount] == ',' ? iCount++ : *cTmp++);
		m_iObservationMethods = iCount + 1;
#ifndef NDEBUG
		mexPrintf("%d observation methods indentified!\n", m_iObservationMethods);
#endif

		bRes = send_and_wait("OPE 1");
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "S_OMCP %s", cMethods);
		bRes = send_and_wait(cCommand);
		bRes = send_and_wait("OPE 0");

		// enable observation methods buttons on TPC unit
		for (iCount = 0; iCount < m_iObservationMethods; iCount++)
		{
			sprintf_s(cCommand, MAX_RESPONSE_SIZE, "SKD %d,1", 171 + iCount);
			bRes = send_and_wait(cCommand);
		}

		return;
	}
	else if (_stricmp("omparameters", name) == 0)
	{
		mwSize total_num_of_cells;
		mwIndex index;
		const mxArray* cell_element_ptr;

		total_num_of_cells = mxGetNumberOfElements(field);
#ifndef NDEBUG
		mexPrintf("%d parameters of observation methods indentified!\n", total_num_of_cells);
#endif
		for (index = 0; index < total_num_of_cells; index++)
		{
			cell_element_ptr = mxGetCell(field, index);
			if (cell_element_ptr == NULL)
			{
				mexPrintf("Empty Cell\n");
			}
			else
			{
				char* cParameters;
				cParameters = getString(cell_element_ptr);
				memcpy_s(m_cOMparameters[index], MAX_RESPONSE_SIZE, cParameters, strlen(cParameters));
				m_bOMinitialized[index] = true;
#ifndef NDEBUG
				mexPrintf("parameters of the observation method %d are set to: %s\n", index + 1, cParameters);
#endif
			}
		}
		return;
	}
	else if (_stricmp("panel_lights", name) == 0)
	{
		int status = static_cast<int>(getScalar(field));
		if (status != 0 && status != 1)
			mexErrMsgTxt("Only 0 and 1 are allowed as the Panel Lights states!");
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "TPIL %d", status);
		bRes = send_and_wait(cCommand);
		if (bRes)
		{
			m_bPanelLights = static_cast<bool>(status);
		}
		return;
	}
	else if (_stricmp("shutterdeck1", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "ESH1 %d", val);
		bRes = send_and_wait(cCommand);
		return;
	}
	else if (_stricmp("shutterdeck2", name) == 0)
	{
		int val = static_cast<int>(getScalar(field));
		sprintf_s(cCommand, MAX_RESPONSE_SIZE, "ESH2 %d", val);
		bRes = send_and_wait(cCommand);
		return;
	}


#ifndef NDEBUG
	mexPrintf("%s:%d - ", __FILE__, __LINE__);
#endif
	mexPrintf("\"%s\" unknown.\n", name);
	return;
}
