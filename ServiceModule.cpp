////////////////////////////////////////////////////////////////
///
/// \file PxLeSrvc/ServiceModule.cpp
///
///     Implementation of CServiceModule class
///
/// \author Kirill V. Kulikov
/// \date   2004-01-17
///
////////////////////////////////////////////////////////////////

#include "ServiceModule.h"

static const TCHAR szServiceName[]        = _T("ExecServer");
static const TCHAR szServiceDescription[] = _T("XML-RPC remote execution server");
static const TCHAR szServiceDisplayName[] = _T("ExecServer");

#define szServiceDependences NULL
#define DIRSIZE 1024

extern int main0(void);
extern void stop_all(void);

////////////////////////////////////////////////////////////////
// CServiceModule static functions
////////////////////////////////////////////////////////////////

#ifndef _CONSOLE
static VOID WINAPI ServiceMain( DWORD argc, LPTSTR *argv )
{
    CServiceModule().Run(argc, argv);
}

static DWORD WINAPI ServiceHandler( DWORD fdwControl, 
                                    DWORD dwEventType,
                                    LPVOID lpEventData,
                                    LPVOID lpContext )
{
    CServiceModule* module = (CServiceModule*)lpContext;
    return module ? module->Control( fdwControl ) : ERROR_CALL_NOT_IMPLEMENTED;
}
#endif

////////////////////////////////////////////////////////////////
// CServiceModule class
////////////////////////////////////////////////////////////////

#ifndef _CONSOLE
VOID CServiceModule::ReportStatus( DWORD status, DWORD hr, DWORD checkPoint )
{
    m_Status.dwCurrentState             = status;
    m_Status.dwWin32ExitCode            = hr;
    m_Status.dwServiceSpecificExitCode  = hr;
    m_Status.dwCheckPoint               = checkPoint;
    if( checkPoint )
    {
        m_Status.dwWaitHint           += 1;
    }
    else
    {
        m_Status.dwWaitHint            = 0;
    }
    ReportStatus();
}

VOID CServiceModule::ReportStatus()
{
    if( m_hStatus != 0 ) SetServiceStatus( m_hStatus, &m_Status );
}

VOID CServiceModule::Run( int argc, LPTSTR *argv )
{
    RtlZeroMemory( &m_Status, sizeof(m_Status) );

    m_Status.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    m_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    m_hStatus = RegisterServiceCtrlHandlerEx( szServiceName, ServiceHandler, this );

    ReportStatus( SERVICE_START_PENDING, 0, 500 );
    ReportStatus( SERVICE_RUNNING );

    try
    {
        main0();
    }
    catch( ... )
    {
    }

    ReportStatus( SERVICE_STOPPED );
}

DWORD CServiceModule::Start()
{
    // Initialize service table for a single service
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { (PTSTR)szServiceName, ServiceMain },
        {  NULL,                NULL        }
    };

    // Connect to service control dispatcher
    return StartServiceCtrlDispatcher( ServiceTable ) ? 0 : ERROR_OUTOFMEMORY;
}

DWORD CServiceModule::Control( DWORD fdwControl )
{
    switch( fdwControl )
    {
        case SERVICE_CONTROL_INTERROGATE: // Interrogate: report status of service.
            ReportStatus();
            break;

        case SERVICE_CONTROL_STOP:
            ReportStatus( SERVICE_STOP_PENDING, 0, 500 );
            stop_all();
            break;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }

    return NO_ERROR;
}

#endif // !_CONSOLE

HRESULT CServiceModule::Install()
{
    // install the application to run as a service
    SC_HANDLE hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if( hManager == 0 ) return HRESULT_FROM_WIN32(GetLastError());

    PTSTR szServicePath = new TCHAR[DIRSIZE];

    ::GetModuleFileName( NULL, szServicePath, DIRSIZE );
    if( _tcschr( szServicePath, TEXT(' ') ) ) ::GetShortPathName( szServicePath, szServicePath, DIRSIZE );

    HRESULT hr = S_OK;
    SC_HANDLE hService = ::OpenService( hManager, szServiceName, SERVICE_ALL_ACCESS );
    if( hService )
    {
        if( !ChangeServiceConfig(
                hService,
                SERVICE_WIN32_OWN_PROCESS,
                SERVICE_AUTO_START, 
                SERVICE_ERROR_NORMAL, 
                szServicePath,
                NULL, 
                NULL, 
                szServiceDependences, 
                NULL, 
                NULL, 
                szServiceDescription 
                ) )
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    else
    {
        hService = ::CreateService(
                        hManager, 
                        szServiceName, 
                        szServiceDisplayName,
                        SERVICE_ALL_ACCESS, 
                        SERVICE_WIN32_OWN_PROCESS,
                        SERVICE_AUTO_START, 
                        SERVICE_ERROR_NORMAL, 
                        szServicePath,
                        NULL, 
                        NULL, 
                        szServiceDependences, 
                        NULL, 
                        NULL
                        );
        if( !hService )
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }

    if( hService )
    {
        if( SUCCEEDED(hr) )
        {
            SERVICE_DESCRIPTION descr;
            descr.lpDescription = (LPTSTR)szServiceDescription;
            if( !::ChangeServiceConfig2( hService, SERVICE_CONFIG_DESCRIPTION, &descr ) )
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
        }
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hManager);

    delete [] szServicePath;

    return hr;
}

HRESULT CServiceModule::Uninstall()
{
    SC_HANDLE schSCManager = OpenSCManager(
                       NULL,                // machine (NULL == local)
                       NULL,                // database (NULL == default)
                       SC_MANAGER_ALL_ACCESS   // access required
                       );
    if( schSCManager )
    {
        SC_HANDLE schService = ::OpenService( 
                        schSCManager, 
                        szServiceName, 
                        DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS
                        );
        if( schService )
        {
            SERVICE_STATUS ssStatus;

            if( ControlService( schService, SERVICE_CONTROL_STOP, &ssStatus ) )
            {
                for( ;; )
                {
                    Sleep( 500 );

                    if( !QueryServiceStatus( schService, &ssStatus ) ) break;
                    if( ssStatus.dwCurrentState != SERVICE_STOP_PENDING ) break;
                }
            }
            DeleteService( schService );
            CloseServiceHandle( schService );
        }
        CloseServiceHandle( schSCManager );
    }
    return S_OK;
}
