////////////////////////////////////////////////////////////////
///
/// \file ServiceModule.h
///
///     Definition of CServiceModule class
///
/// \author Kirill V. Kulikov
/// \date   2004-01-17
///
////////////////////////////////////////////////////////////////

#ifndef _SERVICE_MODULE_H_DEFINED_
#define _SERVICE_MODULE_H_DEFINED_

#include <windows.h>
#include <tchar.h>

class CServiceModule
{
private:
    SERVICE_STATUS          m_Status;
    SERVICE_STATUS_HANDLE   m_hStatus;

public:
    CServiceModule() throw() : m_hStatus(0) {}
    ~CServiceModule() throw() {}

private:
#ifndef _CONSOLE
    VOID            ReportStatus( DWORD status, DWORD hr = 0, DWORD checkPoint = 0 );
    VOID            ReportStatus();
#endif

public:
    static HRESULT          Install();
    static HRESULT          Uninstall();
#ifndef _CONSOLE
    VOID                    Run(int argc, LPTSTR* argv);
    DWORD                   Control( DWORD );
    static DWORD            Start();
#endif
};

#endif
