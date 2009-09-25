#!/usr/bin/env python

description = \
'''Copy locally-built binaries and scripts to the release area.'''

import sys
import pyca_config
import optparse
import shutil
import os

def pr( toprint ):
   if opts.should_print:
      print toprint

def parse_opts():
   optparser = optparse.OptionParser( description=description )
   optparser.add_option( '-l', '--local', default='.',
                         help=\
   '''Absolute path to local files; Files will be copies from here to the
   release area. [default: %default]''' )
   optparser.add_option( '-t', '--target',
                         default='.',
                         help=\
   '''Target directory for installed files.  [default: %default]''' )
   optparser.add_option( '-s', '--symlink', dest='should_link', default=False,
                         action='store_true', help=\
   '''Create symlink for PyQt pyuic4 plugin.''' )
   optparser.add_option( '-p', '--print', dest='should_print', default=False,
                         action='store_true', help=\
   '''Print all file-system operations. [default: %default]''' )

   return optparser.parse_args()

PYUIC_PLUGIN_STUB = 'epics_widgets_pyuic_stub.py'

to_install = { 'PYCA_LIB_DIR': [ ( 'libpyca.so', 'pyca.so' ) ],
               'PYCAQT_LIB_DIR': [ ( 'libpycaqt.so', 'pycaqt.so' ) ],
               'EPICS_WIDGETS_DIR': [ ( 'epics_widgets.py', None ),
                                      ( 'PvConnector.py', None ) ],
               'PYQTDESIGNERPATH': [ ( 'epics_widgets_designer_plugin.py',
                                       None ) ],
               'PYUIC_PLUGIN_DIR': [ ( 'epics_widgets_pyuic_plugin.py',
                                       None ) ],
               'PYCA_DOC_DIR': [ ( 'pyca.txt', None ),
                                 ( 'using_pycaqt.txt', None ) ],
               'PYCA_UTILS_DIR': [ ( 'pyca-designer.py', None ),
                                   ( 'pyca-run-ui.py', None ),
                                   ( 'pyca_config.py', None ),
                                   ( 'pyca-python.py', None ) ] }

( opts, args ) = parse_opts()

for where, what in to_install.items():
   srcdir = opts.local + pyca_config.local_dirs[where]
   destdir = opts.target + pyca_config.install_dirs[where]

   if not os.access( destdir, os.F_OK ):
      pr( "Creating '%s'" % (destdir,) )
      os.makedirs( destdir, 0775 )

   for srcfile, destfile in what:
      srcabs = srcdir + '/' + srcfile
      destabs = destdir + '/' + srcfile

      if destfile is not None:
         destabs = destdir + '/' + destfile

      pr( '%s\n   => %s' % (srcabs, destabs) )
      shutil.copy( srcabs, destabs )

# Install the pyuic4 stub for EPICS widgets not directly derived from
# QWidget.  This is a fixed location; it uses an environment variable,
# PYCA_PYUIC_PLUG, to indirectly point to the correct plugin.

# srcdir = opts.local + pyca_config.local_dirs['PYUIC_PLUGIN_DIR']
# destdir = pyca_config.ABS_PYUIC_DIR
# srcabs = srcdir + '/' + PYUIC_PLUGIN_STUB
# destabs = destdir + '/' + PYUIC_PLUGIN_STUB
# pr( '%s\n   => %s' % (srcabs, destabs) )
# shutil.copy( srcabs, destabs )
