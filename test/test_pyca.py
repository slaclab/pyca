import time
import pytest
import numpy as np
import pyca
from conftest import test_pvs, ConnectCallback, GetCallback

@pytest.mark.parametrize('pvname', test_pvs.values())
def test_create_and_clear_channel(pvname):
    pv = pyca.capv(pvname)
    pv.connection_handler = ConnectCallback()
    pv.create_channel()
    pv.connection_handler.wait(timeout=1)
    assert pv.connection_handler.connected
    pv.clear_channel()
    pv.connection_handler.wait_dc(timeout=1)
    assert not pv.connection_handler.connected


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_get_data(pvname):
    pv = pyca.capv(pvname)
    pv.connection_handler = ConnectCallback()
    pv.getevent_handler = GetCallback()
    pv.create_channel()
    pv.connection_handler.wait(timeout=1)
    # get ctrl vars
    pv.get_data(True, 1.0)
    # get time vars
    pv.get_data(False, 1.0)
    # check that the data has all the keys
    all_keys = ('status', 'warn_llim', 'severity', 'alarm_hlim', 'warn_hlim',
                'alarm_llim', 'precision', 'value', 'display_llim', 'secs',
                'nsec', 'ctrl_llim', 'units', 'ctrl_hlim', 'display_hlim')
    for key in all_keys:
        assert key in pv.data
    # check that value is not None
    assert pv.data['value'] is not None


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_subscribe(pvname):
    pv = pyca.capv(pvname)
    pv.connection_handler = ConnectCallback()
    pv.create_channel()
    pv.connection_handler.wait(timeout=1)
    # Just make sure nothing bad happens
    pv.subscribe_channel()
    time.sleep(1)


@pytest.mark.parametrize('pvname', test_pvs.values())
def test_misc(pvname):
    pv = pyca.capv(pvname)
    pv.connection_handler = ConnectCallback()
    pv.create_channel()
    pv.connection_handler.wait(timeout=1)
    assert isinstance(pv.host(), str)
    assert isinstance(pv.state(), int)
    assert pv.count() == 1
    assert isinstance(pv.type(), str)
    assert isinstance(pv.rwaccess(), int)


def test_waveform(waveform_pv):
    waveform_pv.create_channel()
    waveform_pv.connection_handler.wait(timeout=1)
    # Do as a tuple
    waveform_pv.numpy_arrays = False
    val = waveform_pv.data['value']
    assert isinstance(val, tuple)
    assert len(val) == waveform_pv.count()
    # Do as a np.ndarray
    waveform_pv.numpy_arrays = True
    assert isinstance(val, np.ndarray)
    assert len(val) == waveform_pv.count()