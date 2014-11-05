from distutils.core import setup, Extension
import os

os.environ['CC'] = 'g++'
epics_inc = os.getenv("EPICS_BASE") + "/include"
epics_lib = os.getenv("EPICS_BASE") + "/lib"

pyca = Extension('pyca', language = 'c++', sources = ['pyca/pyca.cc'],
                 include_dirs=['pyca', epics_inc, epics_inc + '/os/Linux'],
                 library_dirs=[epics_lib,
                               epics_lib + "/linux-x86",
                               epics_lib + "/linux-x86_64"],
                 libraries=['Com', 'ca'])

setup(name='pyca', version='2.0.1',
      description='python channel access library',
      ext_modules=[pyca], url='http://slac.stanford.edu',
      author='Amedeo Perazzo', author_email='amedeo@slac.stanford.edu')

