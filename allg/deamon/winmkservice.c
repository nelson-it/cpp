#include <stdio.h>
#include <stdlib.h>

/*
#define _WIN32_WINNT  0x0500
#define  WINVER       0x0500
*/

#include <windows.h>
#include <Lm.h>
#include <Ntsecapi.h>
#include <ntdef.h>
#include <Sddl.h>
#include <ntstatus.h>

#include "winaddaccess.h"
#include "winmkservice.h"

static int errorfound = 0;
void report_log(const char *name, DWORD t, const char *str)
{
    char *s;

    errorfound = 1;
    FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &s,
            0,
            NULL
    );

    if ( 1 || GetStdHandle(STD_ERROR_HANDLE) == 0 )
    {
	const char *strings[2];
	HANDLE rs;

	rs = RegisterEventSource(NULL, name);

	strings[0] = str;
	strings[1] = s;
	ReportEvent(rs, t, 0, 0, NULL, 2, 0, strings, NULL);

	DeregisterEventSource(rs);
    }
    else
        fprintf(stderr, "%s\n", str);
        fprintf(stderr, "error: %s\n", s);

    LocalFree(s);
}

void create_user(const char* name, const char* passwd, const char* kommentar)
{
    USER_INFO_1 ui;
    DWORD error = 0;
    NET_API_STATUS result;

    WCHAR w_user[1024], w_password[1024], w_kommentar[1024];

    MultiByteToWideChar(CP_ACP, 0, name, -1, w_user, 1024);
    MultiByteToWideChar(CP_ACP, 0, passwd, -1, w_password, 1024);
    MultiByteToWideChar(CP_ACP, 0, kommentar, -1, w_kommentar, 1024);

    ui.usri1_name = w_user;
    ui.usri1_password = w_password;
    ui.usri1_priv = USER_PRIV_USER;
    ui.usri1_home_dir = NULL;
    ui.usri1_comment = w_kommentar;
    ui.usri1_flags = UF_SCRIPT | UF_DONT_EXPIRE_PASSWD;
    ui.usri1_script_path = NULL;

    result = NetUserAdd(NULL, 1, (LPBYTE) &ui, &error);
    if (result != NERR_Success)
    {
        switch (result)
        {
        case ERROR_ACCESS_DENIED:
            report_log("create user", EVENTLOG_ERROR_TYPE, "Zugriff verweigert");
            break;
        case NERR_InvalidComputer:
            report_log("create user", EVENTLOG_ERROR_TYPE, "Der Rechnername ist unbekannt");
            break;
        case NERR_NotPrimary:
            report_log("create user", EVENTLOG_ERROR_TYPE, "Der Rechner ist kein PDC");
            break;
        case NERR_GroupExists:
            report_log("create user", EVENTLOG_ERROR_TYPE, "Die Gruppe existiert schon");
            break;
        case NERR_UserExists:
            report_log("create user", EVENTLOG_ERROR_TYPE, "Der Benutzer existiert schon");
            break;
        case NERR_PasswordTooShort:
            report_log("create user", EVENTLOG_ERROR_TYPE, "Das Password wird nicht akzeptiert");
            break;
        default:
            report_log("create user", EVENTLOG_ERROR_TYPE,  "unbekannter Fehler");
            break;
        };
    }
}

void modify_user(const char* name, const char* passwd, const char *kommentar)
{
    USER_INFO_1003 ui;
    USER_INFO_1007 uk;
    DWORD error = 0;
    NET_API_STATUS result;

    WCHAR w_user[1024], w_password[1024], w_kommentar[1024];

    MultiByteToWideChar(CP_ACP, 0, name, -1, w_user, 1024);
    MultiByteToWideChar(CP_ACP, 0, passwd, -1, w_password, 1024);
    MultiByteToWideChar(CP_ACP, 0, kommentar, -1, w_kommentar, 1024);

    ui.usri1003_password = w_password;
    result = NetUserSetInfo(NULL, w_user, 1003, (LPBYTE) &ui, &error);

    if (result != NERR_Success)
    {
        switch (result)
        {
        case ERROR_ACCESS_DENIED:
            report_log("modify user", EVENTLOG_ERROR_TYPE, "Zugriff verweigert");
            break;
        case NERR_InvalidComputer:
            report_log("modify user", EVENTLOG_ERROR_TYPE, "Der Rechnername ist unbekannt");
            break;
        case NERR_NotPrimary:
            report_log("modify user", EVENTLOG_ERROR_TYPE, "Der Rechner ist kein PDC");
            break;
        case NERR_GroupExists:
            report_log("modify user", EVENTLOG_ERROR_TYPE, "Die Gruppe existiert schon");
            break;
        case NERR_UserExists:
            report_log("modify user", EVENTLOG_ERROR_TYPE, "Der Benutzer existiert schon");
            break;
        case NERR_PasswordTooShort:
            report_log("modify user", EVENTLOG_ERROR_TYPE, "Das Password wird nicht akzeptiert");
            break;
        default:
            report_log("modify user", EVENTLOG_ERROR_TYPE,  "unbekannter Fehler");
            break;
        };
    }

    uk.usri1007_comment = w_kommentar;
    NetUserSetInfo(NULL, w_user, 1007, (LPBYTE) &uk, &error);

}

void add_user_and_access(const char *name, const char *passwd, const char *kommentar )
{
    DWORD refsize = 0;
    DWORD sidsize = 0;
    DWORD status;

    PSID sid;
    TCHAR *domain;
    SID_NAME_USE peUse;

    LSA_OBJECT_ATTRIBUTES ObjectAttributes;
    LSA_HANDLE PolicyHandle;

    LSA_UNICODE_STRING lsa_str;

    if (LookupAccountName(NULL, name, NULL, &sidsize, NULL, &refsize, &peUse) == 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        create_user(name, passwd, kommentar);
    }
    else
    {
        modify_user(name, passwd, kommentar);
    }

    refsize = sidsize = 0;
    LookupAccountName(NULL, name, NULL, &sidsize, NULL, &refsize, &peUse);

    sidsize++;
    refsize++;

    sid = malloc(sidsize);
    domain = malloc(sizeof(TCHAR) * refsize);

    if ( LookupAccountName(NULL, name, sid, &sidsize, domain, &refsize, &peUse) == 0 )
    {
        report_log("create user", EVENTLOG_ERROR_TYPE, "Benutzer wurde nicht erstellt");
        return;
    }

    ZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));
    if (LsaOpenPolicy(NULL, &ObjectAttributes, POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES | POLICY_VIEW_LOCAL_INFORMATION | POLICY_WRITE, &PolicyHandle) != STATUS_SUCCESS )
    {
        report_log("create user", EVENTLOG_ERROR_TYPE,  "Kann Policy nicht öffnen");
        free(sid);
        free(domain);
        return;
    }

    lsa_str.Buffer =  L"SeServiceLogonRight";
    lsa_str.Length = wcslen(lsa_str.Buffer) * sizeof(WCHAR);
    lsa_str.MaximumLength = lsa_str.Length + sizeof(WCHAR);

    if ( ( status = LsaAddAccountRights(PolicyHandle, sid, &lsa_str, 1)) != STATUS_SUCCESS )
    {
        report_log("create user", EVENTLOG_ERROR_TYPE,  "kann Recht zum anmeden als Service nicht vergeben");
    }

    LsaClose(PolicyHandle);
    free(sid);
    free(domain);
}

int create_service(const char *name, const char *path, int sauto,
                    const char *user, const char *passwd, const char *rootdir)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    SC_HANDLE sc;
    SC_HANDLE sh;
    DWORD start;
    char scuser[1024];

    errorfound = 0;
    if ( *user != '\0' )
    {
        add_user_and_access(user, passwd, name);
        add_access(rootdir, user, 1);
    }

    if ( errorfound )
        return 0;

    sc = OpenSCManager(NULL, NULL, GENERIC_READ | GENERIC_WRITE);
    if ( sc == NULL )
    {
        report_log("create service",
	           EVENTLOG_ERROR_TYPE, "konnte Servicemanager nicht öffnen\n");
	return 0;
    }

    if ( sauto )
        start = SERVICE_AUTO_START;
    else
        start = SERVICE_DEMAND_START;

    if ( *user == '\0' ) { user = NULL; passwd = NULL; }
    else { sprintf(scuser, ".\\%s", user); user = scuser; }
    sh = CreateService(
        sc,
	name,
	name,
	SERVICE_ALL_ACCESS,
	SERVICE_WIN32_OWN_PROCESS,
	start,
	SERVICE_ERROR_IGNORE,
	path,
	NULL,
	NULL,
	NULL,
	user,
	passwd);

    if ( sh == NULL )
    {
        report_log("create service",
	           EVENTLOG_ERROR_TYPE, "konnte Service nicht erzeugen\n");
        return 0;
    }
    else
        CloseServiceHandle(sh);

    CloseServiceHandle(sc);
    return 1;
#endif
}

void remove_service(const char *name)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    SC_HANDLE sc;
    SC_HANDLE sh;

    sc = OpenSCManager(NULL, NULL, GENERIC_READ | GENERIC_WRITE);
    if ( sc == NULL )
    {
        report_log("remove service",
	           EVENTLOG_ERROR_TYPE, "konnte Servicemanager nicht öffnen\n");
	return;
    }

    sh = OpenService(
        sc,
	name,
	SERVICE_ALL_ACCESS
	);

    if ( sh == NULL )
        report_log("remove service",
	           EVENTLOG_ERROR_TYPE, "konnte Service nicht öffnen\n");
    else
    {
	DeleteService(sh);
        CloseServiceHandle(sh);
    }

    CloseServiceHandle(sc);
#endif
}

void start_service(const char *name)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    SC_HANDLE sc;
    SC_HANDLE sh;

    sc = OpenSCManager(NULL, NULL, GENERIC_READ | GENERIC_WRITE);
    if ( sc == NULL )
    {
        report_log("start service",
	           EVENTLOG_ERROR_TYPE, "konnte Servicemanager nicht öffnen\n");
	return;
    }

    sh = OpenService(
        sc,
	name,
	SERVICE_START
	);

    if ( sh == NULL )
        report_log("start service",
	           EVENTLOG_ERROR_TYPE, "konnte Service nicht öffnen\n");
    else
    {
	if ( StartService(sh, 0, NULL) == 0 )
	    report_log("start service",
	                EVENTLOG_ERROR_TYPE, "konnte Service nicht starten\n");
        CloseServiceHandle(sh);
    }

    CloseServiceHandle(sc);
#endif
}
void stop_service(const char *name)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    SC_HANDLE sc;
    SC_HANDLE sh;

    sc = OpenSCManager(NULL, NULL, GENERIC_READ | GENERIC_WRITE);
    if ( sc == NULL )
    {
        report_log("stop service",
	           EVENTLOG_ERROR_TYPE, "konnte Servicemanager nicht öffnen\n");
	return;
    }

    sh = OpenService(
        sc,
	name,
	SERVICE_STOP
	);

    if ( sh == NULL )
        report_log("stop service",
	           EVENTLOG_ERROR_TYPE, "konnte Service nicht öffnen\n");
    else
    {
	SERVICE_STATUS status;
	if ( ControlService(sh, SERVICE_CONTROL_STOP, &status) == 0 )
	    report_log("stop service",
	                EVENTLOG_ERROR_TYPE, "konnte Service nicht starten\n");
        CloseServiceHandle(sh);
    }

    CloseServiceHandle(sc);
#endif
}

