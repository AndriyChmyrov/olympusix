/****************************************************************************\
*																			*
*		    Olympus IX micrsocope interface									*
*			Andriy Chmyrov © 20.05.2016										*
*																			*
\****************************************************************************/


#ifndef NDEBUG
#define DEBUG(text) mexPrintf("%s:%d - %s\n",__FILE__,__LINE__,text);	// driver actions

#define MEXMESSAGE(error) mexMessage(__FILE__,__LINE__,error)
#define MEXENTER mexEnter(__FILE__,__LINE__)
#else
#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG(text)

#define MEXMESSAGE(error) mexMessage(error)
#define MEXENTER mexEnter()
#endif
#define MEXLEAVE mexLeave()


#ifndef NDEBUG
void mexEnter(const char* file, const int line);
void mexMessage(const char* file, const int line, unsigned error);
#else
void mexEnter(void);
void mexMessage(unsigned error);
#endif
void mexLeave(void);
void mexCleanup(void);

#define	GT_MDK_PORT_MANAGER	"msl_pm.dll"
#define BUFSIZE MAX_PATH
#define MAX_NUM_DECKS 2
#define MAX_NUM_OBSERVATION_METHODS 7

#define MATLAB_DEFAULT_RELEASE R2018a

const WORD StringLength = 256;


int	CALLBACK CommandCallback(
	ULONG		MsgId,			// Callback ID.
	ULONG		wParam,			// Callback parameter, it depends on callback event.
	ULONG		lParam,			// Callback parameter, it depends on callback event.
	PVOID		pv,				// Callback parameter, it depends on callback event.
	PVOID		pContext,		// This contains information on this call back function.
	PVOID		pCaller			// This is the pointer specified by a user in the requirement function.
	);

int	CALLBACK NotifyCallback(
	ULONG		MsgId,			// Callback ID.
	ULONG		wParam,			// Callback parameter, it depends on callback event.
	ULONG		lParam,			// Callback parameter, it depends on callback event.
	PVOID		pv,				// Callback parameter, it depends on callback event.
	PVOID		pContext,		// This contains information on this call back function.
	PVOID		pCaller			// This is the pointer specified by a user in the requirement function.
	);

int	CALLBACK ErrorCallback(
	ULONG		MsgId,			// Callback ID.
	ULONG		wParam,			// Callback parameter, it depends on callback event.
	ULONG		lParam,			// Callback parameter, it depends on callback event.
	PVOID		pv,				// Callback parameter, it depends on callback event.
	PVOID		pContext,		// This contains information on this call back function.
	PVOID		pCaller			// This is the pointer specified by a user in the requirement function.
	);


enum shutter_pos
{
	// No change / undefined
	shutter_undefined = -1,
	// Shutter closed, i.e. no light passes
	shutter_closed = 1,
	// Shutter open
	shutter_open = 0
};


class ixState
{
public:
	ixState(HMODULE);
	~ixState();
	bool open(void);
	bool close(void);
	bool send(const char* c_cmdIn, bool bCallbackCall);
	bool send_and_wait(const char* c_cmdIn, bool bCallbackCall);
	void setResponse(bool, const char*);
	bool getResponse(char*);
	bool getResponseRead(void);
	bool getResponded(void);
	bool waitForResponse(void);
	bool logout(void);
	bool isOpened(void);
	bool isLoggedin(void);
	mxArray* getParameter(const char* name);
	void setParameter(const char* name, const mxArray* field);
	MDK_MSL_CMD					m_Cmd;
	int							m_iDecks[MAX_NUM_DECKS];
	int							m_iMirrorCurr[MAX_NUM_DECKS];
	int							m_iMUid[MAX_NUM_DECKS];
	bool						m_bMUcurrentInitialized;
	int							m_iShutterCurr[MAX_NUM_DECKS];
	bool						m_bShuttercurrentInitialized;
	int							m_iObjective;
	bool						m_bObjectiveInitialized;
	int							m_iLightpath;
	bool						m_bLightpathInitialized;
	int							m_iMainDeck;
	bool						m_bMainDeckInitialized;
	int							m_iObservationMethods;
	char						m_cOMparameters[MAX_NUM_OBSERVATION_METHODS][MAX_RESPONSE_SIZE]; // lazy way - no need to control creation/memory release
	bool						m_bOMinitialized[MAX_NUM_OBSERVATION_METHODS];
	bool						m_bFrameLights;
	bool						m_bPanelLights;

private:
	fn_MSL_PM_Initialize		pfn_InifInterface;
	fn_MSL_PM_EnumInterface		pfn_EnumInterface;
	fn_MSL_PM_GetInterfaceInfo	pfn_GetInterfaceInfo;
	fn_MSL_PM_OpenInterface		pfn_OpenInterface;
	fn_MSL_PM_CloseInterface	pfn_CloseInterface;
	fn_MSL_PM_RegisterCallback	pfn_RegisterCallback;
	fn_MSL_PM_SendCommand		pfn_SendCommand;
	void						*ifData;
	DWORD						timeout;	// default timeout in ms
	bool						m_bOpened;
	bool						m_bLoggedin;
	bool						m_bResponded;
	char						m_cResponse[MAX_RESPONSE_SIZE];
	bool						m_bResponseRead;
};


extern __int32 olympusIX, olympusIXs;		// Microscopes
extern HANDLE	ixObj;
extern HMODULE	hMod;


int	CALLBACK CommandCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller);
int	CALLBACK NotifyCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller);
//int	CALLBACK _NotifyCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller);
//int	CALLBACK _CommandCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller);
int	CALLBACK _ErrorCallback(ULONG MsgId, ULONG	wParam, ULONG lParam, PVOID	pv, PVOID pContext, PVOID pCaller);


// mxArray* getParameter(ixState* ixStates, const char* name);
// void setParameter(ixState* ixStates, const char* name, const mxArray* field);