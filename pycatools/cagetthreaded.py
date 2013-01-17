#!/usr/bin/env python

import pyca
from pycathread import PycaThread
from Pv import Pv

import sys
import time
import threading
import Queue

from options import Options

pvq = Queue.Queue()

def caget(pvname):
  pv = Pv(pvname)
  pv.connect(1.)
  pv.get(False, 1.)
  pv.disconnect()
  return pv.value


def caworker():
  while True:
    try:
      pvname = pvq.get(True,.1)
      v = caget(pvname)
      print "%s:\t%s\t%s\n" % (threading.currentThread().getName(), pvname, v)
    except Queue.Empty, e:
      if pvq.empty():
        return
      pass
    pass
  pass
    
  

if __name__ == '__main__':
  options = Options(['pvnames'], [], [])
  try:
    options.parse()
  except Exception, msg:
    options.usage(str(msg))
    sys.exit()

  pvnames = options.pvnames.split()

  for pvname in pvnames:
    pvq.put(pvname)
    pass

  t1 = PycaThread(name="thread-1",target=caworker)
  t2 = PycaThread(name="thread-2",target=caworker)

  t1.start()
  t2.start()
  t1.join()
  t2.join()
  pass
