#!/usr/bin/env python

description = \
'''Run the specific Python distribution for running EPICS extensions
using the interface library 'pyca'.  Just like calling 'python', but
pyca-python ensures that your environment is correct and that you're
choosing the correct Python distribution (as well as Qt and Qwt).
Pyca developers can optionally choose to run with local (non-released)
plugins and librairies.

Usage: pyca-python.py [options] [-- python-args] [your-python-script]'''

import sys
import os
import copy
import errno
import traceback
import optparse
import pyca_config

optparser = optparse.OptionParser( description=description )

optparser.add_option( '-l', '--local',
                      help=\
'''Absolute path to local files; ignore files in the release area.
Useful for developers of pyca and epics_widgets.''' )
optparser.add_option( '-d', '--dump-env', action='store_true',
                      help=\
'''Display the specific environment variables set for this run.''' )

(opts, args) = optparser.parse_args()

newenv = copy.deepcopy( os.environ )

pyca_config.set_environment_vars( newenv, opts.local, opts.dump_env )

args.insert( 0, pyca_config.PYTHON )

print '%s' % ( ' '.join( args ), )
try:
   os.execve( pyca_config.PYTHON, args, newenv )
except OSError, e:
   print "Error executing python!  Error: '%s' (%d)" % \
         ( errno.errorcode[e.errno], e.errno )
   print "Meaning: " + e.strerror
   traceback.print_exc()
