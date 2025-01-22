import sys

import numpy
import platform
from setuptools_dso import Extension, setup

from numpy import get_include
def get_numpy_include_dirs():
    return [get_include()]

import epicscorelibs.path
import epicscorelibs.version
from epicscorelibs.config import get_config_var


extra = []
if sys.platform=='linux2':
    extra += ['-v']
elif platform.system()=='Darwin':
    # avoid later failure where install_name_tool may run out of space.
    #   install_name_tool: changing install names or rpaths can't be redone for:
    #   ... because larger updated load commands do not fit (the program must be relinked,
    #   and you may need to use -headerpad or -headerpad_max_install_names)
    extra += ['-Wl,-headerpad_max_install_names']


pyca = Extension(
    name='pyca',
    sources=['pyca/pyca.cc'],
    include_dirs= get_numpy_include_dirs()+[epicscorelibs.path.include_path],
    define_macros = get_config_var('CPPFLAGS'),
    extra_compile_args = get_config_var('CXXFLAGS'),
    extra_link_args = get_config_var('LDFLAGS')+extra,
    dsos = ['epicscorelibs.lib.ca',
            'epicscorelibs.lib.Com'
    ],
    libraries=get_config_var('LDADD'),
)

setup(
    name='pyca',
    description='python channel access library',
    packages=['psp', 'pyca'],
    ext_modules=[pyca],
    install_requires = [
        epicscorelibs.version.abi_requires(),
        'numpy >=%s'%numpy.version.short_version,
    ],
    zip_safe=False,
)
