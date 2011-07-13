#include "util.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <io.h>
#include <direct.h>
#include <errno.h>

#include <string>
#include <map>
#pragma warning (disable:4996)

using namespace std;

static const char* tmpdir() {
    const char* r=getenv("TMP");
    if(!r) r=getenv("TEMP");
    if(!r) {
	r="c:\\";
    }
    return r;
}

#define REGISTRY_KEY "Software\\LOCAL\\ExecServer"

const struct configuration *cfg(void) {
    static struct configuration* _cfg=NULL;
    if(_cfg) return _cfg;
    _cfg=new struct configuration;
    _cfg->start_dir=tmpdir();
    _cfg->listen_port=DEFAULT_PORT;
    /* read registry */
    HKEY hkey;
    if(RegOpenKey(HKEY_LOCAL_MACHINE, REGISTRY_KEY, &hkey) == ERROR_SUCCESS) {
	char value[256];
	LONG len;
	len=sizeof(value);
	if(RegQueryValue(hkey,"StartDir",value,&len)==ERROR_SUCCESS) {
	    if(len>0 && len<sizeof(value)) _cfg->start_dir=strdup(value);
	}
	len=sizeof(value);
	if(RegQueryValue(hkey,"ListenPort",value,&len)==ERROR_SUCCESS) {
	    value[sizeof(value)-1]='\0'; // just for sanity
	    int port=atoi(value);
	    if(port) _cfg->listen_port=port;
	}
	RegCloseKey(hkey);
    }
    return _cfg;
}

void clear_error() {
    ::SetLastError(0);
    errno=0;
}

int get_os_error(const char** pdesc) {
    // If we have posix-like error, return it
    *pdesc=NULL;
    int e=errno;
    if(e) {
        *pdesc=strerror(e);
        return e;
    }

    // Otherwise deal with GetLastError()
    DWORD we=::GetLastError();
    if(we) {
        static char errbuf[512];
        if(::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL,
                           we, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           errbuf, sizeof(errbuf), NULL) == 0) {
            sprintf(errbuf,"Unknown error %08x",(unsigned)we);
        }
        *pdesc=errbuf;
        return we;
    }

    // No error
    return 0;
}

static int rmfile(char const* fname) {
    struct stat st;
	if(::stat(fname, &st)<0) return -1;
    if(st.st_mode & _S_IFDIR) {
		return rmdir_recursive(fname);
    } else {
		return unlink(fname);
    }
}

int rmdir_recursive(char const* dirname) {
    struct _finddata_t d;
    char wildcard[_MAX_PATH];
    _snprintf(wildcard, sizeof(wildcard), "%s/*", dirname);
    intptr_t p=_findfirst(wildcard, &d);
    if(p==-1) {
        if(errno==ENOENT) { /* empty dir */
	    errno=0;
	    goto done;
	} else {
	    return -1;
	}
    }
    do {
	char fname[_MAX_PATH];
	if(strcmp(d.name,".")==0 || strcmp(d.name,"..")==0) continue;
	_snprintf(fname,sizeof(fname), "%s/%s",dirname, d.name);
	if(rmfile(fname)<0) {
	    _findclose(p);
	    return -1;
	}
    } while(_findnext(p, &d)==0);
done:
    if(p!=-1) _findclose(p);
    clear_error();
    return rmdir(dirname);
}

map<int, HANDLE> pid_table;

static HANDLE get_process_handle(int pid) {
    if(pid_table.find(pid) != pid_table.end())
        return pid_table[pid];
    HANDLE h=::OpenProcess(
                 PROCESS_QUERY_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,
                 FALSE, (DWORD)pid);
    if(!h)
        return h;
    pid_table[pid]=h;
    return h;
}

int pkill(int pid) {
    HANDLE h=get_process_handle(pid);
    if(!h)
        return -1;
    ::TerminateProcess(h, 254);
    DWORD rc;
    if(!::GetExitCodeProcess(h,&rc)) {
        rc=(DWORD)(-1);
    }
//    pid_table.erase(pid);
//    ::CloseHandle(h);
    return (int)rc;
}

int pwait(int pid, unsigned seconds) {
    HANDLE h=get_process_handle(pid);
    if(!h)
        return -2;
    DWORD wr=::WaitForSingleObject(h, seconds*1000);
    if(wr==WAIT_TIMEOUT) {
        return -1;
    }
    DWORD rc;
    if(::GetExitCodeProcess(h,&rc)) {
        pid_table.erase(pid);
        ::CloseHandle(h);
        return (int)rc;
    } else {
        return -1;
    }
}

static string quote_arg(const string& arg) {
    bool q=false;
    if(arg.empty())
        q=true;
    if(arg.find_first_of(" \t\"") != string::npos)
        q=true;
    if(q) {
        string r("\"");
        for(string::const_iterator p=arg.begin(), e=arg.end(); p!=e; ++p) {
            if(*p=='\\')
                r.append("\\\\");
            else if(*p=='"')
                r.append("\\\"");
            else
                r.push_back(*p);
        }
        r.push_back('"');
        return r;
    } else {
        return arg;
    }
}

static HANDLE open_read(const char* fname) {
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = &sd;
    if(::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) == FALSE)
        return INVALID_HANDLE_VALUE;
    if(::SetSecurityDescriptorDacl(&sd, TRUE, (PACL)NULL, FALSE) == FALSE)
        return INVALID_HANDLE_VALUE;

    return ::CreateFile(fname, FILE_READ_DATA, FILE_SHARE_READ,
                        &sa, OPEN_EXISTING, 0, NULL);
}

static HANDLE open_write(const char* fname) {
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = &sd;
    if(::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) == FALSE)
        return INVALID_HANDLE_VALUE;
    if(::SetSecurityDescriptorDacl(&sd, TRUE, (PACL)NULL, FALSE) == FALSE)
        return INVALID_HANDLE_VALUE;
    return ::CreateFile(fname, FILE_WRITE_DATA|FILE_APPEND_DATA,
                        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                        &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

#define NULFILE "nul"

int pspawn(const char* const* argv, const char* const* envp, const char* cwd,
           const char* fstdin, const char* fstdout, const char* fstderr) {
    int res=-1;
    char* newenv=NULL;
    STARTUPINFO si;
    memset(&si,0,sizeof(si));
    si.cb=sizeof(si);
    si.dwFlags=STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    memset(&pi,0,sizeof(pi));

    /* make command line */
    char* cmd=NULL;
    {
        std::string cmdline;

        for(const char* const* a=argv; *a; ++a) {
            if(!cmdline.empty())
                cmdline += " ";
            cmdline += quote_arg(*a);
        }
        cmd=new char[cmdline.size()+1];
        strcpy(cmd, cmdline.c_str());
    }
    if(envp) {
        /* prepare environment */
        int esize=0;
        for(const char* const* e=envp; *e; ++e)
            esize += strlen(*e)+1;
        esize++;
        newenv=new char[esize];
        char* s=newenv;
        for(const char* const* e=envp; *e; ++e) {
            strcpy(s, *e);
            s += strlen(*e);
            *s++ = 0;
        }
        *s++ = 0;
    }

    /* open files */
    if(!fstdin)
        fstdin=NULFILE;
    si.hStdInput=open_read(fstdin);
    if(si.hStdInput==INVALID_HANDLE_VALUE)
        goto cleanup;

    if(!fstdout)
        fstdout=NULFILE;
    si.hStdOutput=open_write(fstdout);
    if(si.hStdOutput==INVALID_HANDLE_VALUE)
        goto cleanup;

    if(!fstderr)
        fstderr=NULFILE;
    si.hStdError=open_write(fstderr);
    if(si.hStdError==INVALID_HANDLE_VALUE)
        goto cleanup;

    /* run */
    if(::CreateProcess(NULL, cmd, NULL, NULL,
                       TRUE, 0,
                       newenv, cwd, &si, &pi)) {
        /* we don't need handles */
        res=(int)pi.dwProcessId;
        pid_table[res]=pi.hProcess;
        ::CloseHandle(pi.hThread);
    }

cleanup:
    if(cmd) {
        delete[] cmd;
        cmd=NULL;
    }
    if(newenv) {
        delete[] newenv;
        newenv=NULL;
    }

    if(si.hStdInput != 0 && si.hStdInput != INVALID_HANDLE_VALUE)
        ::CloseHandle(si.hStdInput);
    if(si.hStdOutput != 0 && si.hStdOutput != INVALID_HANDLE_VALUE)
        ::CloseHandle(si.hStdOutput);
    if(si.hStdError != 0 && si.hStdError != INVALID_HANDLE_VALUE)
        ::CloseHandle(si.hStdError);

    return res;
}

int uname(struct utsname *name) {
    
    OSVERSIONINFO ovi;
    SYSTEM_INFO si;
    memset(name,0,sizeof(*name));
    
    ovi.dwOSVersionInfoSize=sizeof(ovi);
    GetVersionEx(&ovi);
    
    const char* sysn="Windows?";
    switch (ovi.dwPlatformId) {
    	case VER_PLATFORM_WIN32_NT:      sysn="WinNT"; break;
    	case VER_PLATFORM_WIN32_WINDOWS: sysn="Win9x"; break;
    	case VER_PLATFORM_WIN32s:        sysn="Win32s"; break;
    }
    strncpy(name->sysname, sysn, sizeof(name->sysname));
    DWORD nn=sizeof(name->nodename);
    GetComputerNameExA(ComputerNameDnsFullyQualified, name->nodename, &nn);
    sprintf(name->release, "%u.%u.%u",
    	ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber);
    strncpy(name->version, ovi.szCSDVersion, sizeof(name->version));

    GetSystemInfo(&si);
    const char* arch="Unknown";
    switch (si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL: arch="i386"; break;
	case PROCESSOR_ARCHITECTURE_IA64:  arch="ia64"; break;
	case PROCESSOR_ARCHITECTURE_AMD64: arch="x86_64"; break;
    }
    strncpy(name->machine, arch, sizeof(name->machine));
    
    return 0;
}
