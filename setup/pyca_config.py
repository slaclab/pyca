# This file contains the current configuration for running pyca
# (Python/EPICS Channel Access) from the default release area.
#
# To use this, import it and then use the attributes defined here.

import os

PKG_DIR = '/reg/g/pcds/package'
PYCA_RELEASE_REL = '/epics/3.14/tools/current'
PYCA_RELEASE_DIR = PKG_DIR + PYCA_RELEASE_REL
EPICSCA_DIR = PKG_DIR + '/external/epicsca-pcds-R1.0-r410/lib/x86_64-linux'
PYTHON_DIR = PKG_DIR + '/python-2.5.2'
QT_DIR = PKG_DIR + '/qt-4.3.4_x86_64'
QWT_DIR = PKG_DIR + '/qwt-5.1.1'

PYTHON_LIB_DIR = PYTHON_DIR + '/lib'
PYTHON_BIN_DIR = PYTHON_DIR + '/bin'
PYTHON = PYTHON_BIN_DIR + '/python'
PYUIC = PYTHON_BIN_DIR + '/pyuic4'
ABS_PYUIC_DIR = \
   PYTHON_LIB_DIR + '/python2.5/site-packages/PyQt4/uic/widget-plugins'

QT_BIN_DIR = QT_DIR + '/bin'
QT_LIB_DIR = QT_DIR + '/lib'
QT_INC_DIR = QT_DIR + '/include'

QWT_LIB_DIR = QWT_DIR + '/lib'

DESIGNER = QT_BIN_DIR + '/designer'
PYUIC_PLUGIN_NAME = 'epics_widgets_pyuic_plugin.py'

# These should be prepended with the path to the current build directory.
local_dirs = { 'PYCA_LIB_DIR': '/build/pyca/lib/x86_64-linux',
               'PYCAQT_LIB_DIR': '/build/pycaqt/lib/x86_64-linux', 
               'EPICS_WIDGETS_DIR': '/pycaqt/epics_widgets',
               'PYQTDESIGNERPATH': '/pycaqt/epics_widgets',
               'PYUIC_PLUGIN_DIR': '/pycaqt/epics_widgets',
               'PYCA_DOC_DIR': '/',
               'PYCA_UTILS_DIR': '/' }

# These should be prepended with the destination directory.
install_dirs = { 'PYCA_LIB_DIR': '/lib/x86_64',
                 'PYCAQT_LIB_DIR': '/lib/x86_64',
                 'EPICS_WIDGETS_DIR': '/python',
                 'PYQTDESIGNERPATH': '/python/designer-plugins',
                 'PYUIC_PLUGIN_DIR': '/python/pyuic-plugins',
                 'PYCA_DOC_DIR': '/python/doc',
                 'PYCA_UTILS_DIR': '/python' }

# install_dirs = { 'PYCA_LIB_DIR': PYCA_RELEASE_REL + '/lib/x86_64',
#                  'PYCAQT_LIB_DIR': PYCA_RELEASE_REL + '/lib/x86_64',
#                  'EPICS_WIDGETS_DIR': PYCA_RELEASE_REL + '/python',
#                  'PYQTDESIGNERPATH': PYCA_RELEASE_REL + '/python',
#                  'PYUIC_PLUGIN_DIR': PYCA_RELEASE_REL + '/python',
#                  'PYCA_UTILS_DIR': PYCA_RELEASE_REL + '/python' }
# 
release_dirs = { 'PYCA_LIB_DIR': PYCA_RELEASE_DIR + '/lib/x86_64',
                 'PYCAQT_LIB_DIR': PYCA_RELEASE_DIR + '/lib/x86_64',
                 'EPICS_WIDGETS_DIR': PYCA_RELEASE_DIR + '/python',
                 'PYQTDESIGNERPATH': PYCA_RELEASE_DIR + '/python/designer-plugins',
                 'PYUIC_PLUGIN_DIR': PYCA_RELEASE_DIR + '/python/pyuic-plugins',
                 'PYCA_DOC_DIR': PYCA_RELEASE_DIR + '/python/doc',
                 'PYCA_UTILS_DIR': PYCA_RELEASE_DIR + '/python' }

def remove_from_path( pathstr, remstrs ):
   paths = pathstr.split( os.pathsep )
   newpath = []
   for path in paths:
      keep = True
      for str in remstrs:
         if str in path:
            keep = False
            break
      if keep:
         newpath.append( path )
   return ':'.join( newpath )

def set_environment_vars( env, local_prefix=None, dump=False ):
   if local_prefix is not None:
      local_prefix = os.path.abspath( os.path.expanduser( local_prefix ) )
      dirs = local_dirs
      PYQTDESIGNERPATH = local_prefix + dirs['PYQTDESIGNERPATH']
      PYCAQT_PYUIC_PLUGIN = '%s%s/%s' % ( local_prefix,
                                          dirs['PYUIC_PLUGIN_DIR'],
                                          PYUIC_PLUGIN_NAME )
      PYTHONPATH = ':'.join( [ local_prefix + dirs['PYCA_LIB_DIR'],
                               local_prefix + dirs['PYCAQT_LIB_DIR'],
                               local_prefix + dirs['EPICS_WIDGETS_DIR'],
                               os.environ.get( 'PYTHONPATH', '' ) ] )
   else:
      dirs = release_dirs
      PYQTDESIGNERPATH = dirs['PYQTDESIGNERPATH']
      PYCAQT_PYUIC_PLUGIN = '%s/%s' % ( dirs['PYUIC_PLUGIN_DIR'],
                                        PYUIC_PLUGIN_NAME )
      PYTHONPATH = ':'.join( [ dirs['PYCA_LIB_DIR'],
                               dirs['EPICS_WIDGETS_DIR'],
                               env.get( 'PYTHONPATH', '' ) ] )

   LD_LIBRARY_PATH =  ':'.join( [ QWT_LIB_DIR,
                                  QT_LIB_DIR,
                                  PYTHON_LIB_DIR,
                                  EPICSCA_DIR,
                                  env.get( 'LD_LIBRARY_PATH', '' ) ] )

   env['QTDIR'] = QT_DIR
   env['QTINC'] = QT_INC_DIR
   env['QTLIB'] = QT_LIB_DIR
   env['PYTHONHOME'] = PYTHON_DIR
   env['PYTHONPATH'] = PYTHONPATH
   env['PYQTDESIGNERPATH'] = PYQTDESIGNERPATH
   env['PYCAQT_PYUIC_PLUGIN'] = PYCAQT_PYUIC_PLUGIN
   env['LD_LIBRARY_PATH'] = LD_LIBRARY_PATH
   if 'PATH' in env:
      env['PATH']  = remove_from_path( env['PATH'], [ 'qt', 'python' ] )
      env['PATH']  = PYTHON_BIN_DIR + ':' + env['PATH']
   else:
      env['PATH']  = PYTHON_BIN_DIR

   if dump:
      vars = [ 'QTDIR', 'QTINC', 'QTLIB', 'PYTHONHOME', 'PYTHONPATH',
               'PYQTDESIGNERPATH', 'PYCAQT_PYUIC_PLUGIN',
               'LD_LIBRARY_PATH', 'PATH' ]
      for var in vars:
         print '%s=%s' % ( var, env[var] )



if __name__ == '__main__':
   d = {}
   set_environment_vars( d )
   for item in d.items():
      print item[0], '=', item[1]

   d = { 'PATH': '/bob/goes/here:/some/qtthing/dir' }
   # set_environment_vars( d, local_prefix='~/npa_projects' )
   set_environment_vars( d )
   for item in d.items():
      print item[0], '=', item[1]

# targets: all, install, distclean
# Need makefile at root of Python.
# Makefile in root of tools.
# epics-release script - make install INSTALL_PREFIX=<dest>
