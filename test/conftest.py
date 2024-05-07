import logging

import pytest
from pcaspy import Driver, SimpleServer
from pcaspy.tools import ServerThread

logger = logging.getLogger(__name__)

pvbase = "PYCA:TEST"
pvdb = dict(
    LONG=dict(type="int"),
    DOUBLE=dict(type="float"),
    STRING=dict(type="string"),
    ENUM=dict(type="enum", enums=["zero", "one", "two", "three"]),
    WAVE=dict(type="int", count=10)
)
test_pvs = [pvbase + ":" + key for key in pvdb.keys()]


# We need a trivial subclass of Driver for pcaspy to work
class TestDriver(Driver):
    def __init__(self):
        super().__init__()


class TestServer:
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
        except Exception:
            pass


has_server = False


@pytest.fixture(scope='session')
def server():
    global has_server
    if not has_server:
        has_server = True
        server = TestServer(pvbase, **pvdb)
        server.start_server()
        yield server
        server.kill_server()
