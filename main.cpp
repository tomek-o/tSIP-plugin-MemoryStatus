#define _WIN32_WINNT 0x0500

#define _EXPORTING
#include "..\tSIP\tSIP\phone\Phone.h"
#include "..\tSIP\tSIP\phone\PhoneSettings.h"
#include "..\tSIP\tSIP\phone\PhoneCapabilities.h"

#include "main.h"
#include <psapi.h>
#include <assert.h>

#define NETWORK_ERROR -1
#define NETWORK_OK     0

HANDLE Thread;
HANDLE SessionMutex;

static const struct S_PHONE_DLL_INTERFACE dll_interface =
{DLL_INTERFACE_MAJOR_VERSION, DLL_INTERFACE_MINOR_VERSION};

// callback ptrs
CALLBACK_LOG lpLogFn = NULL;
CALLBACK_CONNECT lpConnectFn = NULL;
CALLBACK_KEY lpKeyFn = NULL;
CALLBACK_ADD_TRAY_MENU_ITEM lpAddTrayMenuItemFn = NULL;

void *callbackCookie;	///< used by upper class to distinguish library instances when receiving callbacks


CALLBACK_SET_APP_STATUS lpSetAppStatusFn = NULL;

static BOOL bPSAPIInitialized					= FALSE;

//WinNT Functions, via PSAPI DLL
typedef BOOL (WINAPI *GETPERFORMANCEINFO)      ( PPERFORMANCE_INFORMATION, DWORD);

static GETPERFORMANCEINFO      p_GetPerformanceInfo   = NULL;

HINSTANCE hPSAPI                                = NULL;

DWORD WINAPI ThreadProc(LPVOID data)
{
    while (1)
    {
        int pause = 60000;

        PERFORMANCE_INFORMATION pi;
        pi.cb = sizeof(pi);
        if ( p_GetPerformanceInfo(&pi, sizeof(pi)) )
        {
            int pct = 100 * pi.CommitTotal / pi.CommitLimit;    // very crude
#if 0
            {
                char buf[128];
                sprintf(buf, "Commit Total = %u\n"
                        "System Cache = %u\nLimit = %u\nPage = %u\nLoad = %d\n",
                        pi.CommitTotal, pi.SystemCache, pi.CommitLimit, pi.PageSize,
                        (int)pct);
                MessageBox(NULL, buf, "Load", MB_OK);
            }
#endif
            if (pct > 93)
            {
                for (int i=0; i<3; i++)
                {
                    Beep( 750, 100 );
                    Sleep(150);
                }
                pause = 60000;
            }

            char buf[128];
            snprintf(buf, sizeof(buf), "Free memory: %u MB", static_cast<unsigned int>((pi.CommitLimit - pi.CommitTotal)/(1024*1024/pi.PageSize)));
            if (lpSetAppStatusFn)
            {
                lpSetAppStatusFn(callbackCookie, "MemoryStatusPlugin", 10, buf);
            }
        }

        Sleep(pause);
    }
    return 0;
}

/** get handle to dll without knowing its name
*/
HMODULE GetCurrentModule()
{
    MEMORY_BASIC_INFORMATION mbi;
    static int dummy;
    VirtualQuery( &dummy, &mbi, sizeof(mbi) );
    return reinterpret_cast<HMODULE>(mbi.AllocationBase);
}

/* extern "C" __declspec(dllexport) */ void GetPhoneInterfaceDescription(struct S_PHONE_DLL_INTERFACE* interf) {
    interf->majorVersion = dll_interface.majorVersion;
    interf->minorVersion = dll_interface.minorVersion;
}

void Log(const char* txt) {
    if (lpLogFn)
        lpLogFn(callbackCookie, const_cast<char*>(txt));
}

void SetCallbacks(void *cookie, CALLBACK_LOG lpLog, CALLBACK_CONNECT lpConnect, CALLBACK_KEY lpKey) {
    assert(cookie && lpLog && lpConnect && lpKey);
    lpLogFn = lpLog;
    lpConnectFn = lpConnect;
    lpKeyFn = lpKey;
    callbackCookie = cookie;

    Log("SystemShutdown plugin loaded\n");
}

void GetPhoneCapabilities(struct S_PHONE_CAPABILITIES **caps) {
    static struct S_PHONE_CAPABILITIES capabilities = {
        0
    };
    *caps = &capabilities;
}

void ShowSettings(HANDLE parent) {
    MessageBox((HWND)parent, "No additional settings.", "Device DLL", MB_ICONINFORMATION);
}

int Connect(void)
{
    DWORD dwtid;

    if ( bPSAPIInitialized )
        return ( TRUE ) ;



    // loading PSAPI dll
    hPSAPI = LoadLibrary ( "PSAPI.DLL" ) ;

    if (hPSAPI != NULL)
	{
	    // get functions pointers
		p_GetPerformanceInfo =	(GETPERFORMANCEINFO)GetProcAddress (hPSAPI,  "GetPerformanceInfo" );
	}

	if (p_GetPerformanceInfo ) {
		bPSAPIInitialized = TRUE ;
        Thread = CreateThread(NULL,0,ThreadProc,0,0,&dwtid);
	}

    return 0;
}

int Disconnect(void)
{
    if (bPSAPIInitialized) {
        TerminateThread(Thread,0);

        if (hPSAPI)
        {
            FreeLibrary (hPSAPI);
            hPSAPI = NULL;
        }
        bPSAPIInitialized = FALSE;
    }
    return 0;
}

void SetCallbackSetAppStatus(CALLBACK_SET_APP_STATUS lpFn)
{
    lpSetAppStatusFn = lpFn;
}
