from distutils.core import setup, Extension
import os

os.environ['CC'] = 'g++'
epics_inc = os.getenv("EPICS_BASE") + "/include"
epics_lib = os.getenv("EPICS_BASE") + "/lib"

#
# OK, we're trying to be all things to all people here.
#
# We don't know if we are compiling using a package manager
# version of epics, or an "old-style" release.  In the
# package manager, the libraries are in the lib directory,
# but in the old releases, they are either in lib/linux-x86
# or lib/linux-x86_64 depending on the architecture.  So,
# we add *all* of these to our library path and count on
# the fact that the loader will toss out the wrong ones,
# albeit with some warnings.
#
# In addition, the package manager python is apparently setup
# in a way that we don't find libpython!  So if we are running
# under a release, we just add the lib path directly.
#
libdirs = [epics_lib,
           epics_lib + "/linux-x86",
           epics_lib + "/linux-x86_64"]
try:
    libdirs.append(os.environ['PSPKG_RELDIR'] + "/lib")
except:
    pass

pyca = Extension('pyca', language = 'c++', sources = ['pyca/pyca.cc'],
                 include_dirs=['pyca', epics_inc, epics_inc + '/os/Linux'],
                 library_dirs=libdirs,
                 libraries=['Com', 'ca'])

setup(name='pyca', version='2.1.1',
      description='python channel access library',
      ext_modules=[pyca], url='https://confluence.slac.stanford.edu/display/PCDS/Using+pyca',
      author='Amedeo Perazzo', author_email='amedeo@slac.stanford.edu')

