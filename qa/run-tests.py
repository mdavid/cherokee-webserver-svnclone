#!/usr/bin/env python

import os
import sys
import time
import signal
import shutil
import tempfile

from base import *
from conf import *

# Configuration parameters
pause = False
clean = True
kill  = True

# Make the DocumentRoot directory
www = tempfile.mkdtemp ("cherokee_www")

# Configuration file base
CONF_BASE = \
"""# Cherokee QA tests
Port %d
Keepalive On
Listen 127.0.0.1
DocumentRoot %s
""" % (PORT, www)

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
    if   p == '-p': pause = True
    elif p == '-n': clean = False
    elif p == '-k': kill  = False

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
        mod_conf += obj.conf

# Write down the configuration file
cfg = CONF_BASE + mod_conf
cfg_file = tempfile.mktemp("cherokee_tmp_cfg")
cfg_fd = open (cfg_file, 'w')
cfg_fd.write (cfg)
cfg_fd.close()

# Launch Cherokee
pid = os.fork()
if pid == 0:
    os.execl ("../cherokee/cherokee", "cherokee", "-C", cfg_file)
else:
    print "Server PID: %d" % (pid)
    time.sleep(2)

# Execute the tests
for obj in objs:
    if pause:
        print "Press <Enter> to continue.."
        sys.stdin.readline()

    print "%s: " % (obj.name) + " "*(40 - len(obj.name)),
    sys.stdout.flush()
    
    ret = obj.Run()
    if ret is 0:
        print "Sucess"
    else:
        print "Failed"
        print obj
        break

# Clean up
if clean:
    os.unlink (cfg_file)
    shutil.rmtree (www, True)

# It's time to kill Cherokee.. :-(
if kill:
    os.kill (pid, signal.SIGTERM)
    time.sleep (.5)
    os.kill (pid, signal.SIGKILL)
