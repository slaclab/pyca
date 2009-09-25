#!/usr/bin/env python

description = \
'''Compile the the specified Qt Designer UI file, 'ui-file', save as a
python file, 'ui-file'.py, and execute the script.  Will automatically
set up the environment for running PyQt scripts.  Can optionally
choose to use released or local librairies.'''

usage = \
'''usage: %prog [options] ui-file'''

import sys
import os
import subprocess
import copy
import errno
import traceback
import optparse
import pyca_config

optparser = optparse.OptionParser( usage=usage,
                                   description=description )

optparser.add_option( '-l', '--local',
                      help=\
'''Absolute path to local files; ignore files in the release area.
Useful for developers of pyca and epics_widgets.''' )

optparser.add_option( '-d', '--dump-env', action='store_true',
                      help=\
'''Display the specific environment variables set for this run.''' )

(opts, args) = optparser.parse_args()

args.insert( 0, '-x' )
args.insert( 0, pyca_config.PYUIC )

newenv = copy.deepcopy( os.environ )

pyca_config.set_environment_vars( newenv, opts.local, opts.dump_env )

print '%s' % ( ' '.join( args ) )

try:
   pyuic = subprocess.Popen( args,
                             stdout=subprocess.PIPE,
                             env=newenv )
   python = subprocess.Popen( [ pyca_config.PYTHON ],
                              stdin=pyuic.stdout,
                              env=newenv )
   python.wait()
   
except OSError, e:
   print "Error executing GUI!  Error: '%s' (%d)" % \
         ( errno.errorcode[e.errno], e.errno )
   print "Meaning: " + e.strerror
   traceback.print_exc()
