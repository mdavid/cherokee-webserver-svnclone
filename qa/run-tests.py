#!/usr/bin/env python

import os
import sys
import time
import signal
import shutil
import tempfile

from base import *
from conf import *

# Make the DocumentRoot directory
www = tempfile.mkdtemp ("cherokee_www")


CONF_BASE = \
"""# Cherokee QA tests
Port %d
Keepalive On
Listen 127.0.0.1
DocumentRoot %s
""" % (PORT, www)

# Make the files list
files = os.listdir('.') 
files = filter (lambda x: x[-3:] == '.py', files)
files = filter (lambda x: x[2] == '-', files)
files.sort()

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

# Generate configuration
mod_conf = ""
for obj in objs:
    if obj.conf is not None:
        mod_conf += obj.conf

# Prepare www files
for obj in objs:
    obj.Prepare(www)

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
    print "Test '%s': " % (obj.name),
    sys.stdout.flush()
    
    ret = obj.Run()
    if ret is 0:
        print "Sucess"
    else:
        print "Failed"
        print obj
        break

# Clean up
os.unlink (cfg_file)
shutil.rmtree (www, True)

# Kill Cherokee.. :-(
os.kill (pid, signal.SIGTERM)
time.sleep (.5)
os.kill (pid, signal.SIGKILL)
