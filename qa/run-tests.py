#!/usr/bin/env python

import os
import sys
import time
import signal
import shutil
import thread
import tempfile

from base import *
from conf import *

# Configuration parameters
num      = 1
thds     = 1
pause    = False
clean    = True
kill     = True
quiet    = False
valgrind = False
port     = PORT

# Make the DocumentRoot directory
www = tempfile.mkdtemp ("cherokee_www")

# Make the files list
files = []
param = []
if len(sys.argv) > 1:
    argv = sys.argv[1:]
    files = filter (lambda x: x[0] != '-', argv)
    param = filter (lambda x: x[0] == '-', argv)

# If not files were specified, use all of them
if len(files) == 0:
    files = os.listdir('.') 
    files = filter (lambda x: x[-3:] == '.py', files)
    files = filter (lambda x: x[2] == '-', files)
    files.sort()

# Process the parameters
for p in param:
    if   p == '-s': pause    = True
    elif p == '-c': clean    = False
    elif p == '-k': kill     = False
    elif p == '-q': quiet    = True
    elif p == '-v': valgrind = True
    elif p[:2] == '-n': num   = int(p[2:])
    elif p[:2] == '-t': thds  = int(p[2:])
    elif p[:2] == '-p': port  = int(p[2:])

# Configuration file base
CONF_BASE = """# Cherokee QA tests
               Port %d
               Keepalive On
               Listen 127.0.0.1
               DocumentRoot %s
            """ % (port, www)

# Import modules 
mods = []
for f in files:
    mod = importfile (f)
    mods.append (mod)

# Build objects
objs = []
for m in mods:
    obj = m.Test()
    objs.append(obj)

# Prepare www files
for obj in objs:
    obj.Prepare(www)

# Generate configuration
mod_conf = ""
for obj in objs:
    if obj.conf is not None:
        mod_conf += obj.conf+"\n"

# Write down the configuration file
cfg = CONF_BASE + mod_conf
cfg_file = tempfile.mktemp("cherokee_tmp_cfg")
cfg_fd = open (cfg_file, 'w')
cfg_fd.write (cfg)
cfg_fd.close()

# Launch Cherokee
pid = os.fork()
if pid == 0:
    if valgrind:
        os.execl (VALGRIND_PATH, "valgrind", "../cherokee/cherokee", "-C", cfg_file)
    else:
        os.execl ("../cherokee/cherokee", "cherokee", "-C", cfg_file)
else:
    print "Server PID: %d" % (pid)
    time.sleep(6)

# Execute the tests
def mainloop_iterator():
    quit = False

    for n in range(num):
        if quit: break
    
        for obj in objs:
            go_ahead = obj.Precondition()

            if go_ahead and pause:
                print "Press <Enter> to continue.."
                sys.stdin.readline()

            if not quiet:
                print "%s: " % (obj.name) + " "*(40 - len(obj.name)),
                sys.stdout.flush()

            if not go_ahead and not quiet:
                print "Skipped"
                continue
    
            try:
                ret = obj.Run()
            except:
                ret = -1

            if ret is not 0:
                quit = True
                print "Failed"
                print obj
                break
            elif not quiet:
                print "Sucess"

# Maybe launch some threads
for n in range(thds-1):
    thread.start_new_thread (mainloop_iterator, ())

# Main thread
mainloop_iterator()

# Clean up
if clean:
    os.unlink (cfg_file)
    shutil.rmtree (www, True)
else:
    print "Test directory %s" % (www)
    print "Configuration  %s" %(cfg_file)
    print

# It's time to kill Cherokee.. :-(
if kill:
    os.kill (pid, signal.SIGTERM)
    time.sleep (.5)
    os.kill (pid, signal.SIGKILL)
