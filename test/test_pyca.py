import time
import threading
import pytest
import numpy as np
import pyca
from conftest import test_pvs, ConnectCallback, GetCallback, setup_pv, pvbase


def test_server_start(server):
    pass


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_create_and_clear_channel(pvname):
    pv = setup_pv(pvname)
    assert pv.connect_cb.connected
    # No callbacks on dc
    pv.clear_channel()
    time.sleep(1)
    with pytest.raises(pyca.pyexc):
        pv.get_data()


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_get_data(pvname):
    pv = setup_pv(pvname)
    # get time vars
    pv.get_data(False, 1.0)
    if not isinstance(pv.data['value'], str):
        # get ctrl vars
        pv.get_data(True, 1.0)
    # check that the data has all the keys
    all_keys = ('status', 'value', 'secs', 'nsec')
    for key in all_keys:
        assert key in pv.data
    # check that value is not None
    assert pv.data['value'] is not None


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_put_get(pvname):
    pv = setup_pv(pvname)
    pv.get_data(False, 0.0)
    pv.getevt_cb.wait()
    old_value = pv.data['value']
    pv_type = type(old_value)
    if pv_type in (int, long, float):
        new_value = old_value + 1
    elif pv_type == str:
        new_value = "putget"
    elif pv_type == tuple:
        new_value = tuple([1] * len(old_value))
    pv.getevt_cb.reset()
    pv.put_data(new_value, 0.0)
    pv.getevt_cb.wait()
    pv.getevt_cb.reset()
    pv.get_data(False, 0.0)
    pv.getevt_cb.wait()
    recv_value = pv.data['value']
    assert recv_value == new_value


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_subscribe(pvname):
    pv = setup_pv(pvname)
    ev = threading.Event()
    def mon_cb(exception=None):
        if exception is not None:
            ev.set()
    pv.monitor_cb = mon_cb
    pv.subscribe_channel(pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM, False)
    # Repeat the put/get test without the get
    pv.get_data(False, 0.0)
    pv.getevt_cb.wait()
    old_value = pv.data['value']
    pv_type = type(old_value)
    if pv_type in (int, long, float):
        new_value = old_value + 1
    elif pv_type == str:
        new_value = "putget"
    elif pv_type == tuple:
        new_value = tuple([1] * len(old_value))
    pv.getevt_cb.reset()
    pv.put_data(new_value, 0.0)
    pv.getevt_cb.wait()
    recv_value = pv.data['value']
    assert recv_value == new_value
    # Verify that monitor_cb was called
    assert ev.is_set()


@pytest.mark.timeout(10)
@pytest.mark.parametrize('pvname', test_pvs)
def test_misc(pvname):
    pv = setup_pv(pvname)
    assert isinstance(pv.host(), str)
    assert isinstance(pv.state(), int)
    assert pv.count() == 1
    assert isinstance(pv.type(), str)
    assert isinstance(pv.rwaccess(), int)


@pytest.mark.timeout(10)
def test_waveform():
    pv = setup_pv(pvbase + ":WAVE")
    # Do as a tuple
    pv.use_numpy = False
    pv.get_data(False, 0.0)
    pv.getevt_cb.wait(timeout=1)
    val = pv.data['value']
    assert isinstance(val, tuple)
    assert len(val) == pv.count()
    pv.getevt_cb.reset()
    # Do as a np.ndarray
    pv.use_numpy = True
    pv.get_data(False, 0.0)
    pv.getevt_cb.wait(timeout=1)
    val = pv.data['value']
    assert isinstance(val, np.ndarray)
    assert len(val) == pv.count()


@pytest.mark.timeout(10)
def test_threads():
    def some_thread_thing(pvname):
        pyca.attach_context()
        pv = setup_pv(pvname)
        pv.get_data(False, 0.0)
        pv.getevt_cb.wait(timeout=1)

    pvname = pvbase + ":WAVE"
    thread = threading.Thread(target=some_thread_thing, args=(pvname,))
    thread.start()
    thread.join()
    assert isinstance(pv.data['value'], tuple)
