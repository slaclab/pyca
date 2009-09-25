#!/usr/bin/env python

description = \
'''Run the specific Qt Designer for creating GUIs using Python and the
EPICS interface library 'pyca'.  Pyca developers can optionally choose
to run with local (non-released) plugins and librairies.'''

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

args.insert( 0, pyca_config.DESIGNER )

print '%s' % ( ' '.join( args ), )
try:
   os.execve( pyca_config.DESIGNER, args, newenv )
except OSError, e:
   print "Error executing designer!  Error: '%s' (%d)" % \
         ( errno.errorcode[e.errno], e.errno )
   print "Meaning: " + e.strerror
   traceback.print_exc()
