__all__ = ['Pv', 'options']

from psp.Pv import Pv as PV  # noqa

from ._version import get_versions

__version__ = get_versions()['version']
del get_versions
