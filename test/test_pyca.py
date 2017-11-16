import time
import threading
import logging
import pytest
import numpy as np
import pyca
from conftest import test_pvs, setup_pv, pvbase

logger = logging.getLogger(__name__)


def test_server_start(server):
    pass


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_create_and_clear_channel(pvname):
    logger.debug('test_create_and_clear_channel %s', pvname)
    pv = setup_pv(pvname)
    assert pv.connect_cb.connected
    # No callbacks on dc
    pv.clear_channel()
    time.sleep(1)
    with pytest.raises(pyca.pyexc):
        pv.get_data(False, -1.0)


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_get_data(pvname):
    logger.debug('test_get_data %s', pvname)
    pv = setup_pv(pvname)
    # get time vars
    pv.get_data(False, -1.0)
    pyca.flush_io()
    assert pv.getevt_cb.wait(timeout=1)
    pv.getevt_cb.reset()
    if not isinstance(pv.data['value'], str):
        # get ctrl vars
        pv.get_data(True, -1.0)
        pyca.flush_io()
        assert pv.getevt_cb.wait(timeout=1)
    # check that the data has all the keys
    all_keys = ('status', 'value', 'secs', 'nsec')
    for key in all_keys:
        assert key in pv.data
    # check that value is not None
    assert pv.data['value'] is not None


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_put_get(pvname):
    logger.debug('test_put_get %s', pvname)
    pv = setup_pv(pvname)
    pv.get_data(False, -1.0)
    pyca.flush_io()
    assert pv.getevt_cb.wait(timeout=1)
    old_value = pv.data['value']
    pv_type = type(old_value)
    logger.debug('%s is of type %s', pvname, pv_type)
    if pv_type in (int, long, float):
        new_value = old_value + 1
    elif pv_type == str:
        new_value = "putget"
    elif pv_type == tuple:
        new_value = tuple([1] * len(old_value))
    logger.debug('caput %s %s', pvname, new_value)
    pv.put_data(new_value, 1.0)
    pv.getevt_cb.reset()
    pv.get_data(False, -1.0)
    pyca.flush_io()
    assert pv.getevt_cb.wait(timeout=1)
    recv_value = pv.data['value']
    assert recv_value == new_value


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_subscribe(pvname):
    logger.debug('test_subscribe %s', pvname)
    pv = setup_pv(pvname)
    ev = threading.Event()
    def mon_cb(exception=None):
        logger.debug('monitor_cb in %s, exception=%s',
                     pvname, exception)
        if exception is None:
            ev.set()
    pv.monitor_cb = mon_cb
    pv.subscribe_channel(pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM, False)
    # Repeat the put/get test without the get
    pv.get_data(False, -1.0)
    pyca.flush_io()
    assert pv.getevt_cb.wait(timeout=1)
    old_value = pv.data['value']
    pv_type = type(old_value)
    logger.debug('%s is of type %s', pvname, pv_type)
    if pv_type in (int, long, float):
        new_value = old_value + 1
    elif pv_type == str:
        new_value = "putmon"
    elif pv_type == tuple:
        new_value = tuple([1] * len(old_value))
    logger.debug('caput %s %s', pvname, new_value)
    ev.clear()
    pv.put_data(new_value, 1.0)
    assert ev.wait(timeout=1)
    recv_value = pv.data['value']
    assert recv_value == new_value


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_misc(pvname):
    logger.debug('test_misc %s', pvname)
    pv = setup_pv(pvname)
    assert isinstance(pv.host(), str)
    assert isinstance(pv.state(), int)
    assert isinstance(pv.count(), int)
    assert isinstance(pv.type(), str)
    assert isinstance(pv.rwaccess(), int)


@pytest.mark.timeout(10)
def test_waveform():
    logger.debug('test_waveform')
    pv = setup_pv(pvbase + ":WAVE")
    # Do as a tuple
    pv.use_numpy = False
    pv.get_data(False, -1.0)
    pyca.flush_io()
    assert pv.getevt_cb.wait(timeout=1)
    val = pv.data['value']
    assert isinstance(val, tuple)
    assert len(val) == pv.count()
    pv.getevt_cb.reset()
    # Do as a np.ndarray
    pv.use_numpy = True
    pv.get_data(False, -1.0)
    pyca.flush_io()
    assert pv.getevt_cb.wait(timeout=1)
    val = pv.data['value']
    assert isinstance(val, np.ndarray)
    assert len(val) == pv.count()


@pytest.mark.timeout(10)
def test_threads():
    logger.debug('test_threads')
    def some_thread_thing(pvname):
        pyca.attach_context()
        pv = setup_pv(pvname)
        pv.get_data(False, -1.0)
        pyca.flush_io()
        assert pv.getevt_cb.wait(timeout=1)
        assert isinstance(pv.data['value'], tuple)

    pvname = pvbase + ":WAVE"
    thread = threading.Thread(target=some_thread_thing, args=(pvname,))
    thread.start()
    thread.join()
