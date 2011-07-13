from subprocess import Popen
from SimpleXMLRPCServer import *
from xmlrpclib import Fault, Binary

import os, sys
import time

# workaround for a subprocess on pythonw.exe bug
nullfile = file(os.devnull,"r+b")

if os.name=='posix':
    import signal
    def pkill(pid):
	try:
	    os.kill(pid,signal.SIGKILL)
	except OSError:
	    pass
elif os.name=='nt':
    import win32api, win32con
    def pkill(pid):
	handle=win32api.OpenProcess(win32con.PROCESS_TERMINATE, 0, pid)
	if handle:
	    win32api.TerminateProcess(handle, 255)

def pwait(proc, maxtime):
    try:
	exitcode = proc.poll()
	if exitcode!=None:
	    return True 
	while maxtime>0:
	    dt=min(maxtime, 0.2)
	    time.sleep(dt)
	    exitcode = proc.poll()
	    if exitcode!=None:
		return True
	    maxtime -= dt
	return False
    except OSError:
	return False

class Functions:
    def __init__(self):
	self.processes={}

    def chdir(self, dir=None):
	"""chdir(dir): changes current directory"""
	if dir:
	    os.chdir(dir)
	return os.getcwd()

    def putFile(self, filename, data):
	"""putFile(filename, data): write to file <filename> the string <data> (or a base64 encoded <data>)"""
	if isinstance(data, Binary):
	    data=data.data
	fo=file(filename, "wb")
	fo.write(data)
	fo.close()
	return True

    def getFile(self, filename, maxbytes=65536, binary=False):
	"""getFile(filename, [maxbytes], [binary]): returns the contents of the file
	Return value: a string or a <binary> object with the file contents"""
	fi=file(filename, "rb")
	data=fi.read(maxbytes)
	fi.close()
	if binary:
	    data=Binary(data)
	return data

    def spawn(self, args, timeout, options={}):
	"""spawn(args, timeout, options): spawn a subprocess
	Arguments:
	    args:    either a string or a list of parameters
	    timeout: if zero, the process is started asynchronously, otherwise, it's a maximum execution time
	    options: an optional struct with the following keys:
		cwd:     the process will chdir there before execution
		env:     a struct of environment variables which will be set
		stdin:   name of a file to be fed to the subprocess's stdin
		stdout:  name of a file where the suprocess's stdout will be written
		stderr:  name of a file where the suprocess's stderr will be written
	Return value:
	    for asynchronous requests:
		pid (integer)
	    for synchronous requests:
		return value (integer)
	    on timeout:
		raise Fault
	"""
	params={}
	if options.get("cwd",None):
	    params["cwd"]=options["cwd"]
	if options.get("env",None):
	    params["env"]=options["env"]
	if options.get("stdin",None):
	    params["stdin"]=file(options["stdin"],"rb")
	else:
	    parans["stdin"]=nullfile
	if options.get("stdout",None):
	    params["stdout"]=file(options["stdout"],"wb")
	else:
	    parans["stdout"]=nullfile
	if options.get("stderr",None):
	    params["stderr"]=file(options["stderr"],"wb")
	else:
	    parans["stderr"]=nullfile
	if isinstance(args, str):
	    params["shell"]=True
	proc=Popen(args, **params)
	if not timeout: # async
	    self.processes[proc.pid]=proc
	    return proc.pid
	else:
	    if not pwait(proc, timeout):
		pkill(proc.pid)
		raise RuntimeError, "Subprocess execution timeout"
	    return proc.wait()

    def wait(self,pid,timeout):
	"""wait(pid, timeout): wait for timeout seconds
	returns process return code or -1 if still waiting"""
	proc=self.processes[pid]
	if pwait(proc, timeout):
	    del self.processes[pid]
	    return proc.poll()
	else:
	    return -1

    def kill(self,pid):
	"""kill(pid): terminate the process"""
	proc=self.processes[pid]
	if proc.poll()==None:
	    pkill(proc.pid)
	return self.wait(pid,1)

def runServer(port):
    server=SimpleXMLRPCServer(("",port))
    server.register_instance(Functions())
    server.register_introspection_functions()
    server.register_multicall_functions()
    server.serve_forever()

if __name__=="__main__":
    try:
	port=int(sys.argv[1])
    except:
	port=8901
    runServer(port)
