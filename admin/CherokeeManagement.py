import os
import sys
import time
import signal
from select import select
from subprocess import *

from consts import *
from configured import *

DEFAULT_DELAY    = 2
WAIT_SERVER_STOP = 10
DEFAULT_PATH     = ['/usr/local/sbin', '/usr/local/bin',
                    '/usr/sbin', '/usr/bin', '/sbin', '/bin']

DEFAULT_PID_LOCATIONS = [
    '/var/run/cherokee.pid',
    os.path.join (PREFIX, 'var/run/cherokee.pid')
]

CHEROKEE_MIN_DEFAULT_CONFIG = """# Default configuration
server!pid_file = %s
vserver!1!nick = default
vserver!1!document_root = /tmp
vserver!1!rule!1!match = default
vserver!1!rule!1!handler = common
""" % (DEFAULT_PID_LOCATIONS[0])

# Cherokee Management 'factory':
#

cherokee_management = None

def cherokee_management_get (cfg):
    global cherokee_management

    # Fast path
    if cherokee_management:
        return cherokee_management

    # Needs to create a new object
    cherokee_management = CherokeeManagement(cfg)
    return cherokee_management

def cherokee_management_reset ():
    global cherokee_management
    cherokee_management = None

# Cherokee Management class
#

class CherokeeManagement:
    def __init__ (self, cfg):
        self._cfg = cfg
        self._pid = self._get_pid()
        self._is_child = False

    # Public
    #

    def save (self, restart=None):
        self._cfg.save()

        if not restart or restart.lower() == 'no':
            return
        if restart.lower() == 'graceful':
            self._restart (graceful=True)
        else:
            self._restart()

    def is_alive (self):
        if not self._pid:
            return False
        return is_PID_alive (self._pid)

    def launch (self):
        def daemonize():
            os.setsid() 

        # Ensure the a minimum path is set
        environ = os.environ.copy()
        if not "PATH" in environ:
            environ["PATH"] = ':'.join(DEFAULT_PATH)

        p = Popen ([CHEROKEE_SERVER, '-C', self._cfg.file], 
                   stdout=PIPE, stderr=PIPE, env=environ,
                   preexec_fn=daemonize, close_fds=True)

        stdout_f,  stderr_f  = (p.stdout, p.stderr)
        stdout_fd, stderr_fd = stdout_f.fileno(), stderr_f.fileno()
        stdout,    stderr    = '', ''

        while True:
            r,w,e = select([stdout_fd, stderr_fd], [], [stdout_fd, stderr_fd], 1)

            if e:
                self._pid = None
                return _("Could not access file descriptors: ") + str(e)

            if stdout_fd in r:
                stdout += stdout_f.read(1)
            if stderr_fd in r:
                stderr += stderr_f.read(1)

            nl = stderr.find('\n')
            if nl != -1:
                for e in ['ERROR', '(error) ', '(critical) ']:
                    if e in stderr:
                        self.__stop_process (p.pid)
                        self._pid = None
                        return stderr
                stderr = stderr[nl+1:]

            if stdout.count('\n') > 1:
                break

        self._pid = p.pid
        self._is_child = True
        time.sleep (DEFAULT_DELAY)
        return None

    def stop (self):
        # Stop Cherokee Guardian
        self.__stop_process (self._pid)
        self._pid = None
        self._is_child = False

    def create_config (self, file, template_file):
        if os.path.exists (file):
            return True

        dirname = os.path.dirname(file)
        if not os.path.exists (dirname):
            try:
                os.mkdir (dirname)
            except:
                return False

        conf_sample = os.path.join(CHEROKEE_ADMINDIR, template_file)
        if os.path.exists (conf_sample):
            content = open(conf_sample, 'r').read()
        else:
            content = CHEROKEE_MIN_DEFAULT_CONFIG

        try:
            f = open(file, 'w+')
            f.write (content)
            f.close()
        except:
            return False

        return True

    # Protected
    #
    def _get_pid_path (self):
        pid_file = self._cfg.get_val("server!pid_file")
        if not pid_file:
            pid_file = os.path.join (CHEROKEE_VAR_RUN, "cherokee.pid")
        return pid_file

    def _get_pid (self):
        pid_file = self._get_pid_path()
        return self.__read_pid_file (pid_file)

    def _restart (self, graceful=False):
        if not self._pid:
            return
        try:
            if graceful:
                os.kill (self._pid, signal.SIGHUP)
            else:
                os.kill (self._pid, signal.SIGUSR1)
        except:
            pass

    # Private
    #

    def __read_pid_file (self, file):
        if not os.access (file, os.R_OK):
            return
        f = open (file, "r")
        try:
            pid = int(f.readline())
        except:
            return None
        try: f.close()
        except: pass
        return pid

    def __stop_process (self, pid):
        if not pid:
            return

        try:
            os.kill (pid, signal.SIGTERM)
            self.__wait_process (pid)
        except:
            pass


    def __wait_process (self, pid):
        if self._is_child:
            try: os.waitpid (pid, 0)
            except: pass
        else:
            retries = 0
            while is_PID_alive (pid) and (retries < WAIT_SERVER_STOP):
                time.sleep (1)
                retries += 1

def is_PID_alive (pid):
    if sys.platform.startswith('linux') or \
       sys.platform.startswith('sunos') or \
       sys.platform.startswith('irix'):
        return os.path.exists('/proc/%s'%(pid))

    elif sys.platform == 'darwin' or \
         "bsd" in sys.platform.lower():
        f = os.popen('/bin/ps -p %s'%(pid))
        alive = len(f.readlines()) >= 2
        try: 
            f.close()
        except: pass
        return alive

    elif sys.platform == 'win32':
        None

    raise 'TODO'


#
# Plug-in checking
#

_server_info = None

def cherokee_get_server_info ():
    global _server_info

    if _server_info == None:
        try:
            f = os.popen ("%s -i" % (CHEROKEE_WORKER))
        except:
            msg = _("ERROR: Couldn't execute '%s -i'")
            print msg % (CHEROKEE_WORKER)

        _server_info = f.read()

        try:
            f.close()
        except: pass

    return _server_info


_built_in_lists = {}

def cherokee_build_info_has (filter, module):
    # Let's see whether it's built-in
    global _built_in_lists

    if not _built_in_lists.has_key(filter):
        _built_in_lists[filter] = {}

        cont = cherokee_get_server_info()

        try:
            filter_string = " %s: " % (filter)
            for l in cont.split("\n"):
                if l.startswith(filter_string):
                    line = l.replace (filter_string, "")
                    _built_in_lists[filter] = line.split(" ")
                    break
        except:
            pass

    return module in _built_in_lists[filter]

def cherokee_has_plugin (module):
    # Check for the dynamic plug-in
    try:
        mods = filter(lambda x: module in x, os.listdir(CHEROKEE_PLUGINDIR))
        if len(mods) >= 1:
            return True
    except:
        pass

    return cherokee_build_info_has ("Built-in", module)

def cherokee_has_polling_method (module):
    return cherokee_build_info_has ("Polling methods", module)
