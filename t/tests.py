from xmlrpclib import *
import unittest

import time
import os,sys

SERVER_URL=os.environ.get("EXECSERVER_URL", "http://localhost:5840")
MANYFILES=10
BIGREPEAT=10000
REMOTE_PROJECT_PATH=os.environ.get("REMOTE_PROJECT_PATH")
LOCAL_TEST_PATH=os.path.dirname(os.path.realpath(__file__))
if REMOTE_PROJECT_PATH:
    REMOTE_TEST_PATH=REMOTE_PROJECT_PATH+"/t"
else:
    REMOTE_TEST_PATH=LOCAL_TEST_PATH

def simplify_filename(d):
    d=re.sub('\\\\','/',d)
    d=re.sub('/$','',d)
    d=re.sub('//+','/',d)
    return d

class dir_tests(unittest.TestCase):
    def assertEqualFilename(self,a,b):
        return self.assertEqual(simplify_filename(a), simplify_filename(b))
        
    def setUp(self):
        self.s=ServerProxy(SERVER_URL)
        self.workdir=None
        self.s.dir.chdir("")
    
    def tearDown(self):
        self.s.dir.chdir("")
        if self.workdir:
            try:
                self.s.dir.rmdir(self.workdir)
            except:
                pass

    def test_mkdir(self):
        self.workdir=self.s.dir.tmpname()
        self.assertRaises(Fault, self.s.dir.chdir, self.workdir)
        self.assertEqual(self.s.dir.mkdir(self.workdir), "")
        self.assertEqualFilename(self.workdir, 
                                 self.s.dir.chdir(self.workdir))
        self.assertEqual(self.s.dir.mkdir(self.s.dir.tmpname()), "")
        self.s.dir.chdir()
        self.assertRaises(Fault, self.s.dir.rmdir, self.workdir)
        self.assertEqual(self.s.dir.rmdir(self.workdir, True), "")
        
    def test_tmpfile(self):
        self.workdir=self.s.dir.tmpname()
        self.assert_(self.workdir)
        self.s.dir.mkdir(self.workdir)
        self.assertEqualFilename(self.s.dir.chdir(self.workdir), self.workdir)
        for i in range(MANYFILES): # generate MANYFILES different tmpfile names
            newfile=self.s.dir.tmpname()
            self.s.dir.mkdir(newfile)
    
def sha1_hexdigest(data):
    import hashlib
    m=hashlib.sha1()
    m.update(data)
    return m.hexdigest()

class file_tests(unittest.TestCase):
    def setUp(self):
        self.s=ServerProxy(SERVER_URL)
        self.s.dir.chdir("")

    def test_ascii(self):
        wf=self.s.dir.tmpname()
        self.assertRaises(Fault, self.s.file.get, wf)
        self.assertEqual(self.s.file.put(wf, BIGREPEAT*'hello'), BIGREPEAT*5)
        self.assertEqual(self.s.file.get(wf), BIGREPEAT*'hello')
        self.assertEqual(self.s.file.get(wf,False,0,100), 20*'hello')
        self.assertEqual(self.s.file.get(wf,False,1,5), 'elloh')
        self.assertEqual(self.s.file.get(wf,True), Binary(BIGREPEAT*'hello'))
        self.s.file.remove(wf)
        self.assertRaises(Fault, self.s.file.remove, wf)

    def test_binary(self):
        wf=self.s.dir.tmpname()
        self.assertRaises(Fault, self.s.file.get, wf)
        self.assertEqual(self.s.file.put(wf, Binary(BIGREPEAT*'hello!')), BIGREPEAT*6)
        self.assertEqual(self.s.file.get(wf,True), Binary(BIGREPEAT*'hello!'))
        self.assertEqual(self.s.file.get(wf,False,1,6), 'ello!h')
#        self.assertRaises(Fault, self.s.file.get, wf, False, BIGREPEAT*6+100)
        self.assertEqual(self.s.file.get(wf,True,0,20*6), Binary(20*'hello!'))
        self.assertEqual(self.s.file.get(wf), BIGREPEAT*'hello!')
        self.s.file.remove(wf)
        self.assertRaises(Fault, self.s.file.remove, wf)
    
    def test_append(self):
        wf=self.s.dir.tmpname()
        self.assertEqual(self.s.file.put(wf, Binary("the")), 3)
        self.assertEqual(self.s.file.put(wf, "rapist", True), 6)
        self.assertEqual(self.s.file.get(wf), "therapist")
        self.s.file.remove(wf)

    def test_replace(self):
        wf=self.s.dir.tmpname()
        self.assertEqual(self.s.file.put(wf, "hello world"), 11)
        self.assertEqual(self.s.file.put(wf, "goodbye"), 7)
        self.assertEqual(self.s.file.get(wf), "goodbye")
        self.s.file.remove(wf)

    def test_many_files(self): # check that we don't have file handle leaks
        try:
            wd=self.s.dir.tmpname()
            self.s.dir.mkdir(wd)
            self.s.dir.chdir(wd)
            for i in range(MANYFILES):
                wf=self.s.dir.tmpname()
                self.s.file.put(wf,'%d'%i)
                self.assertEquals(i, int(self.s.file.get(wf)))
        finally:
            self.s.dir.chdir('')
            self.s.dir.rmdir(wd, True)

    def test_embedded_zero(self):
        wf=self.s.dir.tmpname() 
        self.assertEqual(self.s.file.put(wf,Binary('world\0hello\0')), 12)
        self.assertEqual(self.s.file.get(wf, True).data, 'world\0hello\0')

    def test_digest(self):
        wf=self.s.dir.tmpname() 
        data=''.join((chr(k) for k in range(256)))
        self.s.file.put(wf,Binary(data))
        self.assertEqual(self.s.file.sha1(wf), sha1_hexdigest(data))
        self.assertRaises(Fault, self.s.file.sha1, self.s.dir.tmpname()) # file not found...
        self.s.file.remove(wf)

def t(s):
    return REMOTE_TEST_PATH+"/"+s

class process_tests(unittest.TestCase):
    def setUp(self):
        self.s=ServerProxy(SERVER_URL)
        self.s.dir.chdir("")

    def transfer_executable(self,filename):
        dir_there=self.s.dir.chdir('.')
        fn_here=os.path.join(LOCAL_TEST_PATH, filename)
        fn_there=dir_there+"/"+filename
        fdata=file(fn_here,"rb").read()
        self.s.file.put(fn_there, Binary(fdata))
        self.set_executable(fn_there)
        return fn_there

    def set_executable(self,filename):
        v=self.s.system.uname()
        if v["sysname"][:3] != "Win":
            self.s.process.spawn(['chmod','+x',filename], 10)
    
    def test_args(self):
        wf=self.s.dir.tmpname()
        argv = [ t("show_args"), "foo", "bar hack", "", "O'my", 'q"q' ]
        self.assertEqual(
                         self.s.process.spawn(argv, 1,
                             { "stdout": wf },
                             ),
                         0)
        data=self.s.file.get(wf).split('\n')[:-1]
#        self.assertEqual(data[0], t("show_args"))
        self.assertEqual(data[1:], argv[1:])
        self.s.file.remove(wf)
        
    def test_environ(self):
        wf=self.s.dir.tmpname()
        self.assertEqual(
                         self.s.process.spawn([t("show_env"), 
                             "foo", "baz"], 1,
                             { "stdout": wf, 
                               "env": {"foo":"aaa","bar":"bbb", "hack":"ccc"} },
                             ),
                         0)
        data=self.s.file.get(wf).split('\n')
        self.assertEqual(data[0], "foo=aaa")
        self.assertEqual(data[1], "baz not set")
                
        
    def test_time(self):
        wf=self.s.dir.tmpname()
        self.assertEqual(self.s.process.spawn([t("countdown")], 1), 1)
        self.assertEqual(self.s.process.spawn([t("countdown"), "3"], 4,
                              {"stdout":wf}), 0)
        lines=self.s.file.get(wf).split('\n')
        if not lines[-1]: del lines[-1]
#        print lines
        self.assert_(len(lines)==4)
        self.s.file.remove(wf)
        self.assertRaises(Fault,
            self.s.process.spawn, [t("countdown"), "3"], 1,
                              {"stdout":wf})
        lines=self.s.file.get(wf).split('\n')
        if not lines[-1]: del lines[-1]
#        print lines
        self.assert_(len(lines)==1 or len(lines)==2)
        self.s.file.remove(wf)

    def test_wait(self):
        pid=self.s.process.spawn([t("countdown"),"5"], 0)
        self.assertEquals(self.s.process.wait(pid,3), -1)
        self.assertEquals(self.s.process.wait(pid,3), 0)
        
    def test_kill(self):
        pid=self.s.process.spawn([t("countdown"),"5"], 0)
        time.sleep(2)
        self.s.process.kill(pid)
        time.sleep(1)
        self.assert_(self.s.process.wait(pid,0) > 0)

    def test_transfer(self):
        curdir=self.s.dir.chdir()
        v=self.s.system.uname()
        if v["sysname"][:3] == "Win":
            exe=".exe"
        else:
            exe=""
        fn_there = self.transfer_executable("countdown"+exe)
        self.assertEquals(self.s.process.spawn([fn_there, "2"],5), 0)
        
class system_tests(unittest.TestCase):
    def setUp(self):
        self.s=ServerProxy(SERVER_URL)
    
    def test_version(self):
        ver=self.s.system.version()
        self.assertEqual(type(ver),type(""))
        self.assert_(ver)
    def test_uname(self):
        un=self.s.system.uname()
        print "uname=",un
        self.assert_(un["sysname"])
        self.assert_(un["nodename"])
        self.assert_(un["release"])
        self.assert_(isinstance(un["version"], basestring)) # it's ok if empty
        self.assert_(un["machine"])

if __name__=="__main__":
    unittest.main()
