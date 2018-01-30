#ifdef PTHREAD
#include <pthread.h>
#endif

#include <windows.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <argument/argument.h>
#include "winmkservice.h"
#include "winaddaccess.h"

SERVICE_STATUS          service_status;
SERVICE_STATUS_HANDLE   service_status_h;
PROCESS_INFORMATION     service_pi;
STARTUPINFO             service_si;
int                     service_must_stop;

VOID  WINAPI ctrl_handler (DWORD opcode);
VOID  WINAPI service_start (DWORD argc, LPTSTR *argv);

const char *sn;

DWORD init_service(DWORD   argc, LPTSTR  *argv, DWORD *specificError)
{
    SECURITY_ATTRIBUTES psa;
    SECURITY_ATTRIBUTES tsa;
    char buffer[1024];

    Argument a;

    if ( ((std::string)a["workdir"]) != "" )
        if ( SetCurrentDirectory(((std::string)a["workdir"]).c_str()) == 0 )
        {
            sprintf(buffer, "kann nicht in Ordner <%s> wechseln", ((std::string)a["workdir"]).c_str());
            report_log(sn, EVENTLOG_ERROR_TYPE, buffer);
            return 1;
        }

    if ( ((std::string)a["extrapath"]) != "")
    {
        unsigned int i;
        i = GetEnvironmentVariable("PATH", buffer, sizeof(buffer));
        if ( i > sizeof(buffer) )
        {
            sprintf(buffer, "kann $PATH <%s> nicht erweitern", ((std::string)a["extrapath"]).c_str());
            report_log(sn, EVENTLOG_ERROR_TYPE, buffer);
            return 1;
        }

        buffer[i] = '\0';
        SetEnvironmentVariable("PATH", ((std::string)buffer + ";" + ((std::string)a["extrapath"])).c_str());
    }

    ZeroMemory( &service_si, sizeof(service_si) );
    service_si.cb = sizeof(service_si);
    service_si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    service_si.wShowWindow = SW_HIDE;
    // service_si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    buffer[GetEnvironmentVariable("PATH", buffer, sizeof(buffer))] = '\0';

    if ( ((std::string)a["logfile"]) != "" )
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        service_si.hStdOutput = CreateFile(((std::string)a["logfile"]).c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

        service_si.hStdError  = service_si.hStdOutput;
        if ( service_si.hStdOutput == INVALID_HANDLE_VALUE )
        {
            sprintf(buffer, "konnte logfile %s nicht öffnen", ((std::string)a["logfile"]).c_str());
            report_log( sn, EVENTLOG_ERROR_TYPE, buffer);
            *specificError = 1;
            return 1;
        }

    }
    else
    {
        service_si.hStdOutput  = GetStdHandle(STD_OUTPUT_HANDLE);
        service_si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    }


    ZeroMemory( &psa, sizeof(psa) );
    psa.nLength = sizeof(psa);
    psa.bInheritHandle = true;

    ZeroMemory( &tsa, sizeof(tsa) );
    tsa.nLength = sizeof(tsa);
    tsa.bInheritHandle = true;

    ZeroMemory( &service_pi, sizeof(service_pi) );

    if( !CreateProcess( NULL, // No module name (use command line).
            (char*)(((std::string)a["prog"]).c_str()),     // Command line.
            &psa,                 // Process handle inheritable.
            &tsa,                 // Thread handle inheritable.
            TRUE,                 // Set handle inheritance to FALSE.
            0,                    // No creation flags.
            NULL,                 // Use parent's environment block.
            NULL,                 // Use parent's starting directory.
            &service_si,          // Pointer to STARTUPINFO structure.
            &service_pi )         // Pointer to PROCESS_INFORMATION structure.
    )
    {
        char str[1024];
        sprintf(str, "Kommando %s konnte nicht ausgeführt werden", ((std::string)a["prog"]).c_str());
        report_log( sn, EVENTLOG_ERROR_TYPE, str);
        *specificError = 1;
        return 1;
    }

    *specificError = 0;
    service_must_stop = 0;
    return 0;
}

void stop_service()
{
    DWORD exitcode;

    Argument a;

    service_must_stop = 1;
    TerminateProcess(service_pi.hProcess, 0 );
    WaitForSingleObject( service_pi.hProcess, INFINITE );
    GetExitCodeProcess(service_pi.hProcess, &exitcode);

    if ( (std::string)a["logfile"] != "" )
        CloseHandle(service_si.hStdOutput);

    // Close process and thread handles.
    CloseHandle( service_pi.hProcess );
    CloseHandle( service_pi.hThread );
}

int main( int argc, char **argv)
{

    Argument::ListeMap liste;

    liste["mkservice"] = Argument::Liste("-mkservice", 'l', 0, "");
    liste["rmservice"] = Argument::Liste("-rmservice", 'l', 0, "");
    liste["start"]     = Argument::Liste("-start",     'l', 0, "");
    liste["stop"]      = Argument::Liste("-stop",      'l', 0, "");
    liste["restart"]   = Argument::Liste("-restart",   'l', 0, "");
    liste["svcname"]   = Argument::Liste("-svcname"  , 'c', 1, "");
    liste["auto"]      = Argument::Liste("-auto"  ,    'l', 0, "");
    liste["user"]      = Argument::Liste("-user"  ,    'c', 1, "");
    liste["passwd"]    = Argument::Liste("-passwd",    'c', 1, "");
    liste["rootdir"]   = Argument::Liste("-rdir",      'c', 1, "");

    liste["prog"]      = Argument::Liste("-prog"    , 'c', 1, "");
    liste["workdir"]   = Argument::Liste("-wdir"  ,   'c', 1, "");
    liste["logfile"]   = Argument::Liste("-logfile" , 'c', 1, "");
    liste["extrapath"] = Argument::Liste("-epath"   , 'c', 1, "");
    liste["umask"]       = Argument::Liste("-umask",  'l', 1, "");


    Argument a(&liste, *argv);
    a.scan(--argc, ++argv);

    if ( (std::string)a["svcname"] == "" )
        sn = a.getName().c_str();
    else
        sn = ((std::string)a["svcname"]).c_str();
    if ( a["restart"] )
    {
        if ( (std::string)a["svcname"] != "" )
        {
            stop_service(((std::string)a["svcname"]).c_str());
            start_service(((std::string)a["svcname"]).c_str());
        }
        else
            report_log(sn, EVENTLOG_ERROR_TYPE, "kein servicename angegeben");
        exit(0);
    }
    if ( a["stop"] )
    {
        if ( (std::string)a["svcname"] != ""  )
            stop_service(((std::string)a["svcname"]).c_str());
        else
            report_log(sn, EVENTLOG_ERROR_TYPE, "kein servicename angegeben");
        exit(0);
    }
    if ( a["start"] )
    {
        if ( (std::string)a["svcname"] != ""  )
            start_service(((std::string)a["svcname"]).c_str());
        else
            report_log(sn, EVENTLOG_ERROR_TYPE, "kein servicename angegeben");
        exit(0);
    }
    if ( a["rmservice"] )
    {
        if ((std::string)a["svcname"] != ""  )
            remove_service(((std::string)a["svcname"]).c_str());
        else
            report_log(sn, EVENTLOG_ERROR_TYPE, "kein servicename angegeben");
        exit(0);
    }
    if ( a["mkservice"] )
    {
        if ((std::string)a["svcname"] != ""  )
        {
            exit(create_service(((std::string)a["svcname"]).c_str(),
                    a.getFullname().c_str(),
                    a["auto"],
                    ((std::string)a["user"]).c_str(),
                    ((std::string)a["passwd"]).c_str(),
                    ((std::string)a["rootdir"]).c_str() ));
        }
        else
        {
            report_log(sn, EVENTLOG_ERROR_TYPE, "kein servicename angegeben");
            exit(0);
        }
    }

    SERVICE_TABLE_ENTRY   dtable[] =
    {
            { (CHAR*)"",    service_start },
            { NULL, NULL           }
    };

    if ( !StartServiceCtrlDispatcher( dtable) )
    {
        char str[1024];
        sprintf(str,
                "konnte ServiceCtrlDispatcher nicht starten (%d)",
                (int)GetLastError());
        report_log(sn, EVENTLOG_ERROR_TYPE, str);
    }
    return 0;
}

VOID WINAPI ctrl_handler (DWORD opcode)
{
    int status;

    switch(opcode)
    {
    case SERVICE_CONTROL_STOP:
        service_status.dwWin32ExitCode = 0;
        service_status.dwCurrentState  = SERVICE_STOPPED;
        service_status.dwCheckPoint    = 0;
        service_status.dwWaitHint      = 0;

        stop_service();
        break;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
    {
        char buffer[1024];
        status = GetLastError();
        sprintf(buffer, "falscher Opcode (%d)", (int)opcode);
        report_log(sn, EVENTLOG_ERROR_TYPE, buffer);
    }
    }

    if (!SetServiceStatus (service_status_h, &service_status))
    {
        char buffer[1024];
        status = GetLastError();
        sprintf(buffer, "konnte service status nicht setzen (%d)", status);
        report_log(sn, EVENTLOG_ERROR_TYPE, buffer);
    }

    return;
}

void WINAPI service_start (DWORD argc, LPTSTR *argv)
{
    int status;
    DWORD specificError;

    service_status.dwServiceType        = SERVICE_WIN32;
    service_status.dwCurrentState       = SERVICE_START_PENDING;
    service_status.dwControlsAccepted   = SERVICE_ACCEPT_STOP;
    service_status.dwWin32ExitCode      = 0;
    service_status.dwServiceSpecificExitCode = 0;
    service_status.dwCheckPoint         = 0;
    service_status.dwWaitHint           = 0;

    service_status_h = RegisterServiceCtrlHandler( "", ctrl_handler);
    if (service_status_h == (SERVICE_STATUS_HANDLE)0)
    {
        char buffer[1024];
        sprintf(buffer, "konnte ServiceCtrlHandler nicht registrieren(%d)",
                (int)GetLastError());
        report_log(sn, EVENTLOG_ERROR_TYPE, buffer);
        return;
    }

    status = init_service(argc,argv, &specificError);
    if (status != 0)
    {
        report_log(sn, EVENTLOG_ERROR_TYPE, "Fehler beim Start");
        service_status.dwCurrentState       = SERVICE_STOPPED;
        service_status.dwCheckPoint         = 0;
        service_status.dwWaitHint           = 0;
        service_status.dwWin32ExitCode      = status;
        service_status.dwServiceSpecificExitCode = specificError;

        SetServiceStatus (service_status_h, &service_status);
        return;
    }

    // Initialization complete - report running status.
    service_status.dwCurrentState       = SERVICE_RUNNING;
    service_status.dwCheckPoint         = 0;
    service_status.dwWaitHint           = 0;

    if (!SetServiceStatus (service_status_h, &service_status))
    {
        char buffer[1024];
        status = GetLastError();
        sprintf(buffer, "konnte service status nicht setzen (%d)", status);
        report_log(sn, EVENTLOG_ERROR_TYPE, buffer);
    }

    while ( 1 )
    {
        WaitForSingleObject( service_pi.hProcess, INFINITE );
        report_log(sn, EVENTLOG_ERROR_TYPE, "Prozess wird neu gestartet");
        init_service(argc, argv, &specificError);
        Sleep(10000);
    }

    return;
}
