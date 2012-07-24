import pyca
import threading
import sys
import copy

class Pv(pyca.capv):
  def __init__(self, name):
    pyca.capv.__init__(self, name)
    self.__connection_sem = threading.Event()
    self.__putevt_sem = threading.Event()
    self.connect_cb = self.connection_handler
    self.putevt_cb = self.putevt_handler
    self.__putevt_exc = None

  # Channel access callbacks
  def connection_handler(self, isconnected):
    if isconnected:
      self.__connection_sem.set()

  def putevt_handler(self, e=None):
    self.__putevt_exc = e
    self.__putevt_sem.set()

  # Calls to channel access methods
  def connect(self, timeout=-1.0):
    tmo = float(timeout)
    self.create_channel()
    if tmo > 0:
      self.__connection_sem.wait(tmo)
      if not self.__connection_sem.isSet():
        raise pyca.pyexc, "connection timedout for PV %s" %(self.name)

  def disconnect(self):
    self.clear_channel()

  def monitor(self, mask, ctrl=False):
    self.subscribe_channel(mask, ctrl)

  def unsubscribe(self):
    self.unsubscribe_channel()

  def get(self, ctrl=False, timeout=-1.0):
    tmo = float(timeout)
    self.get_data(ctrl, tmo)

  #
  # Two simultaneous puts from different threads won't work with this code.
  # If we want to support this, we need to serialize puts using a lock.
  #
  def put(self, value, timeout=-1.0):
    tmo = float(timeout)
    self.__putevt_exc = None
    self.__putevt_sem.clear()
    self.put_data(value, -1.0)
    pyca.flush_io()
    if tmo > 0:
      self.__putevt_sem.wait(tmo)
      if not self.__putevt_sem.isSet():
        raise pyca.pyexc, "put timedout for PV %s" %(self.name)
      elif self.__putevt_exc != None:
        raise pyca.caexc, self.__putevt_exc
    
  # Used to obtain a copy of the data which won't be overwritten by ca callbacks
  def getcopy(self):
    interval = sys.getcheckinterval()
    try:
      sys.setcheckinterval(2**31-1)
      datacopy = copy.deepcopy(self.data)
    finally:
      sys.setcheckinterval(interval)
    return datacopy

  # Re-define getattr method to allow direct access to 'data' dictionary members
  def __getattr__(self, name):
    if self.data.has_key(name):
      return self.data[name]
    else:
      raise AttributeError
