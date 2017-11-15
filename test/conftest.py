import threading
import pyca
import pytest

test_pvs = dict(
    dbr_string_t='XPP:USR:MMS:01.DESC',
    dbr_enum_t='HX2:SB1:PIM',
    #dbr_char_t=None,
    #dbr_short_t=None,
    #dbr_ulong_t=None,
    dbr_long_t='XPP:USR:MMS:01.RC',
    #dbr_float_t=None,
    dbr_double_t='XPP:USR:MMS:01')

class ConnectCallback(object):
    def __init__(self):
        self.connected = False
        self.cev = threading.Event()
        self.dcev = threading.Event()
        self.lock = threading.RLock()

    def wait(self, timeout=None):
        self.cev.wait(timeout=timeout)

    def wait_dc(self, timeout=None):
        self.dcev.wait(timeout=timeout)

    def __call__(self, is_connected):
        with self.lock:
            self.connected = is_connected
            if self.connected:
                self.cev.set()
                self.dcev.clear()
            else:
                self.dcev.set()
                self.cev.clear()


class GetCallback(object):
    def __init__(self):
        self.gev = threading.Event()

    def wait(self, timeout=None):
        self.gev.wait(timeout=timeout)

    def reset(self):
        self.gev.clear()

    def __call__(self, exception=None):
        if exception is None:
            self.gev.set()

@pytest.fixture(scope='function')
def waveform_pv():
    pv = pyca.capv(INSERTNAMEHERE)
    pv.connection_handler = ConnectCallback()
    pv.getevent_handler = GetCallback()
    return pv
