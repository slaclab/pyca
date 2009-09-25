import pyca
import threading
import sys
import copy

class Pv(pyca.capv):
  def __init__(self, name):
    pyca.capv.__init__(self, name)
    self.__connection_sem = threading.Event()

  # Channel access callbacks
  def connection_handler(self, isconnected):
    print "Connection: is_connected=%d" % isconnected
    if isconnected:
      self.__connection_sem.set()

  def monitor_handler(self, exception=None):
    pass

  def getevent_handler(self, exception=None):
    pass

  def putevent_handler(self, exception=None):
    pass

  # Calls to channel access methods
  def connect(self, timeout=-1):
    self.create_channel()
    if timeout > 0:
      self.__connection_sem.wait(timeout)
      if not self.__connection_sem.isSet():
        raise pyca.pyexc, "connection timedout for PV %s" %(self.name)

  def disconnect(self):
    self.clear_channel()

  def monitor(self, mask, ctrl=False):
    self.subscribe_channel(mask, ctrl)

  def get(self, ctrl=False, timeout=-1.0):
    self.get_data(ctrl, timeout)

  def put(self, value, timeout=-1.0):
    self.put_data(value, timeout)
    
  # Used to obtain a copy of the data which won't be overwritten by ca callbacks
  def getcopy(self):
    interval = sys.getcheckinterval()
    try:
      sys.setcheckinterval(sys.maxint)
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
