#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>

static const char* tmpdir() {
    const char* r=getenv("TMP");
    if(!r) {
#ifdef P_tmpdir
        r=strdup(P_tmpdir);
#else
	r=strdup("/tmp");
#endif
    }
    return r;
}

const struct configuration *cfg(void) {
    static struct configuration* _cfg=NULL;
    if(_cfg) return _cfg;
    _cfg=new struct configuration;
    _cfg->start_dir=tmpdir();
    _cfg->listen_port=DEFAULT_PORT;
    /* read file /etc/ExecServer.conf */
    FILE* cfgfile=fopen("/etc/ExecServer.conf","r");
    if(cfgfile) {
	char buf[256], name[256], value[256];
	while(fgets(buf,sizeof(buf),cfgfile)) {
	    char* pcomment=strchr(buf, '#');
	    if(pcomment) *pcomment='\0'; // cut comments away
	    if(sscanf(buf,"%s%s",name,value)==2) {
		if(strcmp(name,"start_dir")==0) 
		    _cfg->start_dir=strdup(value);
		if(strcmp(name,"listen_port")==0) {
		    int port=atoi(value);
		    if(port) _cfg->listen_port=port;
		}
	    }
	}
	fclose(cfgfile);
    }
    return _cfg;
}

void clear_error() {
    errno=0;
}

int get_os_error(const char** pdesc) {
    *pdesc=NULL;
    int e=errno;
    if(!e)
        return 0;
    *pdesc=strerror(e);
    return e;
}

static int rmfile(char const* fname) {
    struct stat st;
    if(stat(fname, &st)<0) return -1;
    if(S_ISDIR(st.st_mode)) {
	return rmdir_recursive(fname);
    } else {
	return unlink(fname);
    }
}

int rmdir_recursive(char const* dirname) {
    DIR* d=opendir(dirname);
    if(!d) return -1;
    while(1) {
	struct dirent* dent=readdir(d);
	if(!dent) {
	    if(errno==0) break; // it was the last one
	    closedir(d);
	    return -1;
	}
	if(strcmp(dent->d_name,".")==0 || strcmp(dent->d_name,"..")==0) 
	    continue;
	char fname[PATH_MAX];
	snprintf(fname,sizeof(fname),"%s/%s",dirname, dent->d_name);
	if(rmfile(fname)<0) {
	    closedir(d);
	    return -1;
	}
    }
    closedir(d);
    clear_error();
    return rmdir(dirname);
}

int pkill(int pid) {
    if(kill(pid, SIGTERM)<0) {
        sleep(1);
        kill(pid, SIGKILL);
    }
    return 0;
}

int pwait(int pid, unsigned seconds) {
    int status;
    for(;;) {
        if(waitpid(pid, &status, WNOHANG)!=0) {
            if(WIFEXITED(status))
                return WEXITSTATUS(status);
            else if(WIFSIGNALED(status))
                return 0x100+WTERMSIG(status);
            else
                return 0x200;
        }
        if(seconds==0)
            return -1;
        sleep(1);
        --seconds;
    }
}

#define READ  O_RDONLY
#define WRITE O_WRONLY|O_CREAT|O_TRUNC

static int openfd(const char* fn, int mode) {
    if(!fn)
        fn="/dev/null";
    int fd=open(fn, mode, 0666);
    return fd;
}

static int test_chdir(const char* cwd) {
    if(access(cwd,X_OK)<0)
        return -1;
    struct stat st;
    if(stat(cwd,&st)<0)
        return -1;
    if(!(st.st_mode & S_IFDIR)) {
        errno=ENOTDIR;
        return -1;
    }
    return 0;
}

int pspawn(const char* const* argv, const char* const* envp, const char* cwd,
           const char* fstdin, const char* fstdout, const char* fstderr) {
    int fdstdin=-1, fdstdout=-1, fdstderr=-1, child=-1, ret=-1;
    if((fdstdin=openfd(fstdin, READ))<0)
        goto cleanup;
    if((fdstdout=openfd(fstdout, WRITE))<0)
        goto cleanup;
    if((fdstderr=openfd(fstderr, WRITE))<0)
        goto cleanup;
    if(cwd && test_chdir(cwd)<0)
        goto cleanup;
    if((child=fork())<0)
        goto cleanup;
    if(child>0) {
        ret=child;
        goto cleanup;
    }
    /* Now we are in the child process */
    dup2(fdstdin, 0);
    dup2(fdstdout,1);
    dup2(fdstderr,2);
    /* close the rest fd's */
    for(int i=3; i<1024; ++i) close(i);
    if(cwd)
        chdir(cwd);
    if(envp) {
        while(*envp) {
            putenv(const_cast<char*>(*envp));
            ++envp;
        }
    }
    execvp(argv[0], (char* const*)argv);
    exit(254); // if exec... failed
cleanup:
    if(fdstdin>=0)
        close(fdstdin);
    if(fdstdout>=0)
        close(fdstdout);
    if(fdstderr>=0)
        close(fdstderr);
    return ret;
}
