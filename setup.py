from setuptools import setup, Extension
import os, sys
import numpy as np

if sys.platform == 'darwin':
    libsrc = 'Darwin'
elif sys.platform.startswith('linux'):
    libsrc = 'Linux'
else:
    libsrc = None
    
epics_inc = os.getenv("EPICS_BASE") + "/include"
epics_lib = os.getenv("EPICS_BASE") + "/lib/" + os.getenv("EPICS_HOST_ARCH")
numpy_inc = np.get_include()

pyca = Extension('pyca',
                 language='c++',
                 sources=['pyca/pyca.cc'],
                 include_dirs=['pyca', epics_inc, epics_inc + '/os/' + libsrc,
                               numpy_inc],
                 library_dirs=[epics_lib],
                 libraries=['Com', 'ca'])

setup(name='pyca', version='3.0.0',
      description='python channel access library',
      ext_modules=[pyca], url='https://confluence.slac.stanford.edu/display/PCDS/Using+pyca',
      author='Amedeo Perazzo', author_email='amedeo@slac.stanford.edu')
