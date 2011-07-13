#include "xmlrpcpp/XmlRpc.h"
#include "sha1.h"
#include "util.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#if defined(_WINDOWS)
#include <direct.h>
#include <io.h>

#define F_OK 0
#define W_OK 2
#define R_OK 4

#define mkdir(__path,__mode) _mkdir(__path)

#pragma warning (disable:4996)

#define DIR_SEPARATOR '\\'

#else

#define DIR_SEPARATOR '/'

#endif

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

using namespace XmlRpc;
using namespace std;

void throw_on_os_error(const char* desc /*=NULL*/) {
    const char* errtext;
    int e=get_os_error(&errtext);
    if(e) {
        stringstream ss;
        if(desc)
            ss << desc << ": ";
        ss << "Error " << e;
        if(errtext)
            ss << ": " << errtext;
        throw XmlRpcException(ss.str(), e);
    }
}


/* converts string to char*, or NULL if string is empty */
const char* str(const string& s) {
    if(s.empty())
        return NULL;
    return s.c_str();
}

/* converts vector<string> to NULL-terminated char** */
class strlist {
    char** data;
public:
    strlist(const vector<string>& vs): data(NULL) {
        size_t vsize=vs.size();
        if(!vsize)
            return;
        data=(char**)malloc((vsize+1)*sizeof(char*));
        for(size_t i=0; i<vsize; ++i) {
            data[i]=strdup(vs[i].c_str());
        }
        data[vsize]=NULL;
    }
    ~strlist() {
        if(data) {
            for(char** pp=data; *pp; ++pp)
                free(*pp);
            free(data);
        }
    }

    operator char**() {
        return data;
    }
};

class Path {
    char* data;

    static size_t path_max() {
#if defined PATH_MAX
        return PATH_MAX;
#elif defined _MAX_PATH
        return _MAX_PATH;
#elif defined _PC_PATH_MAX
        return pathconf("/", _PC_PATH_MAX);
#else
    #error "cannot find PATH_MAX"
#endif
    }
public:
    Path(const char* value=NULL): data(new char[path_max()]) {
        if(value) strncpy(data, value, size());
    }
    ~Path() { delete[] data; }
    size_t size() const { return path_max(); }
    operator char*() { return data; }
    char* get() { return data; }
    const char* get() const { return data; }
};

class FileHolder {
    FILE* f;
public:
    FileHolder(FILE* f_): f(f_) {}
    ~FileHolder() {
        if(f)
            fclose(f);
    }
    operator FILE*() {
        return f;
    }
};

class M_dir_tmpname: public XmlRpcServerMethod {
public:
    M_dir_tmpname(XmlRpcServer* server = 0):
	XmlRpcServerMethod("dir.tmpname", server) {}
    std::string help() {
	return "dir.tmpname(): return a unique temporary file name in the current directory";
    }
    void execute(XmlRpcValue& /*params*/, XmlRpcValue& result) {
	int attempts=256;
	Path cwd;
	clear_error();
	getcwd(cwd, cwd.size());
	throw_on_os_error("getcwd");
	if(access(cwd, W_OK)<0) {
	    throw XmlRpcException("Current directory is not writeable");
	}
        Path fname;
	for(int i=(rand() << 8) ^ (int)(time(NULL)); attempts; ++i,--attempts) {
	    snprintf(fname, fname.size(), "%s%cEX%06x", cwd.get(), DIR_SEPARATOR, i&0xFFFFFF);
	    clear_error();
	    if(access(fname, F_OK)<0) {
		if(errno == ENOENT) {
		    /* great! this file doesn't exist */
		    clear_error();
		    result=fname;
		    return;
		}
	    }
	}
	throw XmlRpcException("Cannot create temporary file name");
    }
};

class M_dir_chdir: public XmlRpcServerMethod {
public:
    M_dir_chdir(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("dir.chdir", server) {}

    std::string help() {
        return "dir.chdir([dir]): change current directory\n"
	       "    If <dir> is empty, go to the start directory\n"
               "Return value: current directory";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        Path buf;
        string dir;
        try {
            if(params.getType()!=XmlRpcValue::TypeInvalid)
                dir=string(params[0]);
        } catch(...) {
            throw XmlRpcException("parameters error");
        }
	if(dir.empty()) dir=cfg()->start_dir;
        clear_error();
	chdir(dir.c_str());
	throw_on_os_error("chdir");
        getcwd(buf, buf.size());
        throw_on_os_error("getcwd");
        result=string(buf);
    }
};

class M_dir_mkdir: public XmlRpcServerMethod {
public:
    M_dir_mkdir(XmlRpcServer* server = 0):
            XmlRpcServerMethod("dir.mkdir", server) {}

    std::string help() {
        return "dir.mkdir(dir): create directory";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        string dir;
        try {
            if(params.getType()!=XmlRpcValue::TypeInvalid)
                dir=string(params[0]);
        } catch(...) {
            throw XmlRpcException("parameters error");
        }
        if(dir.empty()) throw XmlRpcException("directory name is empty");
        clear_error();
        mkdir(dir.c_str(),0777);
        throw_on_os_error("mkdir");
        (void)result;
    }
};

class M_dir_rmdir: public XmlRpcServerMethod {
public:
    M_dir_rmdir(XmlRpcServer* server = 0):
            XmlRpcServerMethod("dir.rmdir", server) {}

    std::string help() {
        return "dir.rmdir(dir, [recursive=False]): remove directory\n"
        "\tif recursive==True, delete non-empty directory and all its contents";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        string dir;
        bool recursive=false;
        try {
            if(params.getType()!=XmlRpcValue::TypeInvalid)
                dir=string(params[0]);
            if(params.size()>1) {
            	recursive=bool(params[1]);
            }
        } catch(...) {
            throw XmlRpcException("parameters error");
        }
        if(dir.empty()) throw XmlRpcException("directory name is empty");
        clear_error();
        if(recursive)
	    rmdir_recursive(dir.c_str());
	else
	    rmdir(dir.c_str());
	throw_on_os_error("rmdir");
        (void)result;	
    }
};

class M_file_put: public XmlRpcServerMethod {
public:
    M_file_put(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("file.put", server) {}

    std::string help() {
        return "file.put(filename, data, [append=False]): write to file <filename> the string <data> (or a base64 encoded <data>)\n"
        	"\tIf append==True, append to the end of file\n"
        	"Return value: number of bytes written";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        string fname, data;
        bool binary=false;
        bool append=false;
        try {
            fname=string(params[0]);
            switch(params[1].getType()) {
            case XmlRpcValue::TypeString:
                data=string(params[1]);
                binary=false;
                break;
            case XmlRpcValue::TypeBase64: {
                    XmlRpcValue::BinaryData& b(params[1]);
                    data.assign(b.begin(), b.end());
                    binary=true;
                }
                break;
            default:
                throw XmlRpcException("parameters error");
            }
            if(params.size()>2) append=bool(params[2]);
        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        FileHolder fo=fopen(fname.c_str(),
            binary? 
        	(append? "ab":"wb"): 
        	(append? "a":"w"));
        if(!fo)
            throw_on_os_error("fopen");
	clear_error();
        size_t nw=fwrite(data.c_str(), 1, data.size(), fo);
        throw_on_os_error("fwrite");
	result=(int)nw;
    }
};

class M_file_get: public XmlRpcServerMethod {
public:
    M_file_get(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("file.get", server) {}

    std::string help() {
        return "file.get(filename, [binary=False], [pos=0], [maxbytes=ALL]): receive a file from the host filesystem\n"
               "Arguments:\n"
               "   filename: name of file to receive\n"
               "   binary:   boolean value, if set to TRUE, the contents will be sent as Base64, otherwise as String\n"
               "   pos:      initial position in file\n"
               "   maxbytes: maximum number of bytes to send, file will be truncated\n"
               "Return value:\n"
               "   the contents of the file (string or base64 on demand)\n";
    }

    enum { BUFSZ = 1024*64 };

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        string fname;
        bool binary=false;
        int pos=0, maxbytes=-1;
        try {
            fname=string(params[0]);
            if(params.size()>1) {
                binary=bool(params[1]);
            }
            if(params.size()>2) {
                pos=int(params[2]);
            }
            if(params.size()>3) {
                maxbytes=int(params[3]);
            }
        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        FileHolder fi=fopen(fname.c_str(), binary? "rb":"r");
        if(!fi)
            throw_on_os_error("fopen");
        if(pos) {
            fseek(fi, pos, SEEK_SET);            
            throw_on_os_error("fseek");
        }

        string res;
        char buf[BUFSZ];
        while(maxbytes) {
            size_t nr=maxbytes<0? BUFSZ: maxbytes;
            if(nr>BUFSZ)
                nr=BUFSZ;
            if(!nr)
                break;
            nr=fread(buf,1,nr,fi);
            if(!nr) {
                throw_on_os_error("fread");
                break;
            }
            res.append(buf, buf+nr);
            if(maxbytes>=0)
                maxbytes-=nr;
        }
        if(binary) {
            result=XmlRpcValue((void*)res.c_str(), res.size());
        } else {
            result=res;
        }
    }
};

class M_file_sha1: public XmlRpcServerMethod {
public:
    M_file_sha1(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("file.sha1", server) {}

    std::string help() {
        return "file.sha1(filename): return SHA1 hexdigest of a file";
    }

    enum { BUFSZ = 1024*64 };

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        string fname;
        try {
            fname=string(params[0]);
        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        FileHolder fi=fopen(fname.c_str(), "rb");
        if(!fi)
            throw_on_os_error("fopen");

	SHA1 sha1;

        char buf[BUFSZ];
        while(1) {
            size_t nr=BUFSZ;
            if(nr>BUFSZ)
                nr=BUFSZ;
            if(!nr)
                break;
            nr=fread(buf,1,nr,fi);
            if(!nr) {
                throw_on_os_error("fread");
                break;
            }
	    sha1.Input(buf, nr);
        }
	unsigned rd[5];
	if(!sha1.Result(rd)) throw XmlRpcException("SHA message corrupted");
	stringstream ss;
	for(int i=0; i<5; ++i) ss << hex << setw(8) << setfill('0') << rd[i];
	result=ss.str();
    }
};

class M_file_remove: public XmlRpcServerMethod {
public:
    M_file_remove(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("file.remove", server) {}

    std::string help() {
        return "file.remove(filename): remove file from the host filesystem\n";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        string fname;
        try {
            fname=string(params[0]);
        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        unlink(fname.c_str());
        throw_on_os_error("unlink");
        (void)result;        
    }
};

class M_process_spawn: public XmlRpcServerMethod {
public:
    M_process_spawn(XmlRpcServer * server = 0): 
        XmlRpcServerMethod("process.spawn", server) {}
    std::string help() {
        return "process.spawn(args, timeout, options): spawn a subprocess\n"
               "Arguments:\n"
               "    args:    list of parameters (first is the program name)\n"
               "    timeout: if zero, the process is started asynchronously, otherwise, it's a maximum execution time\n"
               "    options: an optional struct with the following keys:\n"
               "        cwd:     the process will chdir there before execution\n"
               "        stdin:   name of a file to be fed to the subprocess's stdin\n"
               "        stdout:  name of a file where the suprocess's stdout will be written\n"
               "        stderr:  name of a file where the suprocess's stderr will be written\n"
               "        env:     dict of environment variables to set\n"
               "Return value:\n"
               "    for asynchronous requests:\n"
               "        pid (integer)\n"
               "    for synchronous requests:\n"
               "        return value (integer)\n"
               "    on timeout:\n"
               "        kill process and raise Fault";
    }
    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        vector<string> args, envs;
        string argstr;
        string cwd, fin, fout, ferr;
        int timeout=0;

        try {
            XmlRpcValue& vargs=params[0];
            switch(vargs.getType()) {
            case XmlRpcValue::TypeArray:
                for(int i=0; i<vargs.size(); ++i) {
                    args.push_back(string(vargs[i]));
                }
                break;
            case XmlRpcValue::TypeString:
                args.push_back(string(vargs));
                break;
            default:
                throw XmlRpcException("parameters error");
            }
            if(params.size()>1) {
                timeout=int(params[1]);
            }
            if(params.size()>2) {
                XmlRpcValue& vopts=params[2];
                if(vopts.hasMember("cwd"))
                    cwd=string(vopts["cwd"]);
                if(vopts.hasMember("stdin"))
                    fin=string(vopts["stdin"]);
                if(vopts.hasMember("stdout"))
                    fout=string(vopts["stdout"]);
                if(vopts.hasMember("stderr"))
                    ferr=string(vopts["stderr"]);
                if(vopts.hasMember("env")) {
                    XmlRpcValue& venv=vopts["env"];
                    vector<string> keys=venv.keys();
                    for(vector<string>::const_iterator p=keys.begin();
                    	    p!=keys.end(); ++p) {
                    	envs.push_back(*p+string("=")+string(venv[*p]));
              	    }
                }
            }

        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        int pid=pspawn(strlist(args), strlist(envs), str(cwd),
                       str(fin), str(fout), str(ferr));
        if(pid<0)
            throw_on_os_error("exec");

        if(timeout==0) {
            result=pid;
        } else {
            int res=pwait(pid, timeout);
            if(res<0) {
                pkill(pid);
                throw_on_os_error("kill");
                throw XmlRpcException("Process killed on timeout");
            }
            result=res;
        }
    }

};

class M_process_wait: public XmlRpcServerMethod {
public:
    M_process_wait(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("process.wait", server) {}

    std::string help() {
        return "process.wait(pid, timeout): wait for completion of <pid> or for <timeout> seconds";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        int pid, timeout=0;
        try {
            pid=params[0];
            timeout=params[1];
        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        int res=pwait(pid, timeout);
        throw_on_os_error("wait");
        result=res;
    }
};

class M_process_kill: public XmlRpcServerMethod {
public:
    M_process_kill(XmlRpcServer* server = 0): 
        XmlRpcServerMethod("process.kill", server) {}

    std::string help() {
        return "process.kill(pid): kill the process <pid>";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        int pid;
        try {
            pid=params[0];
        } catch(...) {
            throw XmlRpcException("parameters error");
        }

        clear_error();
        int res=pkill(pid);
        throw_on_os_error("kill");
        result=res;
    }
};

class M_system_version: public XmlRpcServerMethod {
public:
    M_system_version(XmlRpcServer* server = 0): XmlRpcServerMethod("system.version", server) {}

    std::string help() {
        return "system.version(): return ExecServer version";
    }

    void execute(XmlRpcValue& /*params*/, XmlRpcValue& result) {
    	result = _VERSION_;
    }
};

class M_system_uname: public XmlRpcServerMethod {
public:
    M_system_uname(XmlRpcServer* server = 0): XmlRpcServerMethod("system.uname", server) {}

    std::string help() {
        return "system.uname(): return machine info";
    }

    void execute(XmlRpcValue& /*params*/, XmlRpcValue& result) {
    	struct utsname un;
    	uname(&un);
    	result["sysname"]=un.sysname;
    	result["nodename"]=un.nodename;
    	result["release"]=un.release;
    	result["version"]=un.version;
    	result["machine"]=un.machine;    	
    }
};

class M_system_getenv: public XmlRpcServerMethod {
public:
    M_system_getenv(XmlRpcServer* server = 0): XmlRpcServerMethod("system.getenv", server) {}

    std::string help() {
        return "system.getenv(): return environment variable";
    }

    void execute(XmlRpcValue& params, XmlRpcValue& result) {
        std::string var;
        try {
            var=string(params[0]);
        } catch(...) {
            throw XmlRpcException("parameters error");
        }
        char* res=getenv(var.c_str());
        result=res ? string(res): string("");
    }
};


class ExecServer: public XmlRpcServer {
    int port_;
    volatile bool stop_flag_;
public:
    ExecServer(int port): XmlRpcServer(), port_(port), stop_flag_(false) {
	addMethod(new M_dir_tmpname(this));
        addMethod(new M_dir_chdir(this));
	addMethod(new M_dir_mkdir(this));
	addMethod(new M_dir_rmdir(this));
        addMethod(new M_file_put(this));
        addMethod(new M_file_get(this));
	addMethod(new M_file_sha1(this));
        addMethod(new M_file_remove(this));
        addMethod(new M_process_spawn(this));
        addMethod(new M_process_wait(this));
        addMethod(new M_process_kill(this));
        addMethod(new M_system_getenv(this));
        addMethod(new M_system_version(this));
        addMethod(new M_system_uname(this));
    }

    bool start() {
        if(!bindAndListen(port_)) return false;
        enableIntrospection();
        while(!stop_flag_) work(0.5);
        shutdown();
        return true;
    }

    void stop() {
        stop_flag_=true;
    }
};

ExecServer* srv=NULL;

int main0(void) {
    if(!cfg()) {
        // cannot load configuration
        return 2;
    }
    srand((unsigned)time(NULL));
    chdir(cfg()->start_dir);
    srv=new ExecServer(cfg()->listen_port);
    srv->start();
    delete srv;
    srv=NULL;
    return 0;
}

void stop_all() {
    if(srv) {
        srv->stop();
    }
}

#ifdef _WINDOWS
#include "ServiceModule.h"

static BOOL WINAPI ctrl_handler(DWORD /*dwCtrlType*/) {
    stop_all();
    return TRUE;
}


int main(int argc, char** argv) {
    if(argc>1) {
        if(strcmp(argv[1],"--install") == 0) {
            return CServiceModule::Install();
        } else if(strcmp(argv[1],"--uninstall") == 0) {
            return CServiceModule::Uninstall();
        } else if(strcmp(argv[1],"--start") == 0) {
            return CServiceModule::Start();
        } else if(strcmp(argv[1],"--no-detach") == 0) {
	    SetConsoleCtrlHandler(ctrl_handler, TRUE);
	    XmlRpc::setVerbosity(2);
            return main0();
        }
    }
    return CServiceModule::Start();
}

#else // !_WINDOWS
int main(int argc, char** argv) {
    bool detach=true;
    if(argc>1) {
        if(strcmp(argv[1],"--no-detach") == 0) {
	    XmlRpc::setVerbosity(2);
            detach=false;
        }
    }
    if(detach) daemon(0,0);
    return main0();
}
#endif
