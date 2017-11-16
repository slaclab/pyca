import threading
import logging
import pyca
from pcaspy import Driver, SimpleServer
from pcaspy.tools import ServerThread
import pytest

logger = logging.getLogger(__name__)

pvbase = "PYCA:TEST"
pvdb = dict(
    CHAR = dict(type="char"),
    LONG = dict(type="int"),
    DOUBLE = dict(type="float"),
    STRING = dict(type="string"),
    ENUM = dict(type="enum", enums=["zero", "one", "two", "three"]),
    WAVE = dict(type="char", count=100)
)
test_pvs = [pvbase + ":" + key for key in pvdb.keys()]


# We need a trivial subclass of Driver for pcaspy to work
class TestDriver(Driver):
    def __init__(self):
        super(TestDriver, self).__init__()


class TestServer(object):
    """
    Class to create temporary pvs to check in psp_plugin
    """
    def __init__(self, pvbase, **pvdb):
        self.pvbase = pvbase
        self.pvdb = pvdb
        self.kill_server()

    def make_server(self):
        """
        Create a new server and start it
        """
        logger.debug('Create new server')
        self.server = SimpleServer()
        self.server.createPV(self.pvbase + ":", self.pvdb)
        self.driver = TestDriver()

    def kill_server(self):
        """
        Remove the existing server (if it exists) and re-initialize
        """
        logger.debug('Kill server')
        self.stop_server()
        self.server = None
        self.driver = None

    def start_server(self):
        """
        Allow the current server to begin processing
        """
        logger.debug('Restarting pcaspy server')
        if self.server is None:
            self.make_server()
        self.stop_server()
        self.server_thread = ServerThread(self.server)
        logger.debug('Pressing start')
        self.server_thread.start()

    def stop_server(self):
        """
        Pause server processing
        """
        logger.debug('Stop old server, if exists')
        try:
            self.server_thread.stop()
        except:
            pass

@pytest.fixture(scope='module')
def server():
    server = TestServer(pvbase, **pvdb)
    server.start_server()
    yield server
    server.kill_server()


class ConnectCallback(object):
    def __init__(self, name):
        self.name = name
        self.connected = False
        self.cev = threading.Event()
        self.dcev = threading.Event()
        self.lock = threading.RLock()

    def wait(self, timeout=None):
        logger.debug('Wait on connect callback %s', self.name)
        ok = self.cev.wait(timeout=timeout)
        if ok:
            logger.debug('Wait complete on connect %s', self.name)
        else:
            logger.debug('Wait fail on connect %s', self.name)
        return ok

    def wait_dc(self, timeout=None):
        logger.debug('Wait on disconnect callback %s', self.name)
        ok = self.dcev.wait(timeout=timeout)
        if ok:
            logger.debug('Wait complete on disconnect %s', self.name)
        else:
            logger.debug('Wait fail on disconnect %s', self.name)
        return ok

    def __call__(self, is_connected):
        logger.debug('Connect callback in %s, is_connected=%s',
                     self.name, is_connected)
        with self.lock:
            self.connected = is_connected
            if self.connected:
                self.cev.set()
                self.dcev.clear()
            else:
                self.dcev.set()
                self.cev.clear()


class GetCallback(object):
    def __init__(self, name):
        self.name = name
        self.gev = threading.Event()

    def wait(self, timeout=None):
        logger.debug('Wait on get callback %s', self.name)
        ok = self.gev.wait(timeout=timeout)
        if ok:
            logger.debug('Wait complete on get %s', self.name)
        else:
            logger.debug('Wait fail on get %s', self.name)
        return ok

    def reset(self):
        logger.debug('Clear get callback %s', self.name)
        self.gev.clear()

    def __call__(self, exception=None):
        logger.debug('Get callback in %s, exception=%s',
                     self.name, exception)
        if exception is None:
            self.gev.set()


def setup_pv(pvname, connect=True):
    pv = pyca.capv(pvname)
    pv.connect_cb = ConnectCallback(pvname)
    pv.getevt_cb = GetCallback(pvname)
    if connect:
        pv.create_channel()
        pv.connect_cb.wait(timeout=1)
    return pv
