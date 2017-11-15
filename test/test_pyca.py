import time
import pytest
import numpy as np
import pyca
from conftest import test_pvs, ConnectCallback, GetCallback


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_create_and_clear_channel(pvname):
    pv = pyca.capv(pvname)
    pv.connect_cb = ConnectCallback()
    pv.create_channel()
    pv.connect_cb.wait(timeout=1)
    assert pv.connect_cb.connected
    # No callbacks on dc
    pv.clear_channel()
    time.sleep(1)
    with pytest.raises(pyca.pyexc):
        pv.get_data()


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_get_data(pvname):
    pv = pyca.capv(pvname)
    pv.connect_cb = ConnectCallback()
    pv.getevt_cb = GetCallback()
    pv.create_channel()
    pv.connect_cb.wait(timeout=1)
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


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_subscribe(pvname):
    pv = pyca.capv(pvname)
    pv.connect_cb = ConnectCallback()
    pv.create_channel()
    pv.connect_cb.wait(timeout=1)
    # Just make sure nothing bad happens
    pv.subscribe_channel(pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM, False)
    time.sleep(1)


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_misc(pvname):
    pv = pyca.capv(pvname)
    pv.connect_cb = ConnectCallback()
    pv.create_channel()
    pv.connect_cb.wait(timeout=1)
    assert isinstance(pv.host(), str)
    assert isinstance(pv.state(), int)
    assert pv.count() == 1
    assert isinstance(pv.type(), str)
    assert isinstance(pv.rwaccess(), int)


def test_waveform(waveform_pv):
    waveform_pv.create_channel()
    waveform_pv.connect_cb.wait(timeout=1)
    # Do as a tuple
    waveform_pv.use_numpy = False
    waveform_pv.get_data(False, 1.0)
    waveform_pv.getevt_cb.wait(timeout=1)
    val = waveform_pv.data['value']
    assert isinstance(val, tuple)
    assert len(val) == waveform_pv.count()
    waveform_pv.getevt_cb.reset()
    # Do as a np.ndarray
    waveform_pv.use_numpy = True
    waveform_pv.get_data(False, 1.0)
    waveform_pv.getevt_cb.wait(timeout=1)
    val = waveform_pv.data['value']
    assert isinstance(val, np.ndarray)
    assert len(val) == waveform_pv.count()
