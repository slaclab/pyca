from setuptools import setup, Extension
import os, sys
import numpy as np

if sys.platform == 'darwin':
    libsrc = 'Darwin'
    compiler = 'clang'
elif sys.platform.startswith('linux'):
    libsrc = 'Linux'
    compiler = 'gcc'
else:
    libsrc = None

epics_inc = os.getenv("EPICS_BASE") + "/include"
epics_lib = os.getenv("EPICS_BASE") + "/lib/" + os.getenv("EPICS_HOST_ARCH")
numpy_inc = np.get_include()
numpy_lib = np.__path__[0]

pyca = Extension('pyca',
                 language='c++',
                 sources=['pyca/pyca.cc'],
                 include_dirs=['pyca', epics_inc,
                                epics_inc + '/os/' + libsrc,
                                epics_inc + '/compiler/' + compiler,
                                numpy_inc],
                 library_dirs=[epics_lib,numpy_lib],
                 runtime_library_dirs=[epics_lib,numpy_lib],
                 libraries=['Com', 'ca'])

setup(ext_modules=[pyca,])