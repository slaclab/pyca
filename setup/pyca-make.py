#!/usr/bin/env python

description = \
'''Configure the environment for making the pyca and pycaqt
librairies.'''

# Build the pyca and pycaqt librairies.

import sys
import os
import copy
import errno
import traceback
import pyca_config
import optparse

UTILS = ['sip', 'python', 'make']
MAKE = 'make'
MAKE_ARGS = [ MAKE, 'x86_64-linux', 'verbose=y' ]

optparser = optparse.OptionParser( description=description )
optparser.add_option( '-p', '--print', dest='should_print',
                      action='store_true', help=\
'''Print the specific utilities used in the make.''' )
optparser.add_option( '-d', '--dump-env',
                      action='store_true', help=\
'''Dump the modified environment variables used during the make.''' )

(opts, args) = optparser.parse_args()

def pr( toprint ):
   if opts.should_print:
      print toprint

def find_on_path( executable, path=None ):
   if path is None:
      path = os.environ['PATH']
   for p in path.split( os.pathsep ):
      abs = p + os.sep + executable
      if os.access( abs, os.F_OK ):
         return abs
   return None

newenv = copy.deepcopy( os.environ )

pyca_config.set_environment_vars( newenv, dump=opts.dump_env )

if opts.should_print:
   for util in UTILS:
      print "'%s' is: %s" % ( util, find_on_path( util, newenv['PATH'] ) )

print '%s' % ( ' '.join( MAKE_ARGS ), )
try:
   os.execvpe( MAKE, MAKE_ARGS, newenv )
except OSError, e:
   print "Error executing make!  Error: '%s' (%d)" % \
         ( errno.errorcode[e.errno], e.errno )
   print "Meaning: " + e.strerror
   traceback.print_exc()
