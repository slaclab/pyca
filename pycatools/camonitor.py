#!/usr/bin/env python

import pyca
from Pv import Pv

import sys
import time

from options import Options

class monitor(Pv):
  def __init__(self, name):
    Pv.__init__(self, name)

  def monitor_handler(self, exception=None):
    try:
      if exception is None:
        if self.status == pyca.NO_ALARM:
          ts = time.localtime(self.secs+pyca.epoch)
          tstr = time.strftime("%Y-%m-%d %H:%M:%S", ts)
          print "%-30s %s.%09d" %(self.name, tstr, self.nsec), self.value
        else:
          print "%-30s %s %s" %(self.name, 
                                pyca.severity[self.severity],
                                pyca.alarm[self.status])
      else:
        print "%-30s " %(self.name), exception
    except Exception, e:
      print e

if __name__ == '__main__':
  options = Options(['pvnames'], ['timeout'], [])
  try:
    options.parse()
  except Exception, msg:
    options.usage(str(msg))
    sys.exit()

  pyca.initialize()

  pvnames = options.pvnames.split()
  if options.timeout is not None:
    timeout = float(options.timeout)
  else:
    timeout = 1

  evtmask = pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM 

  for pvname in pvnames:
    try:
      pv = monitor(pvname)
      pv.connect(timeout)
      pv.monitor(evtmask, ctrl=False)
    except pyca.pyexc, e:
      print 'pyca exception: %s' %(e)
    except pyca.caexc, e:
      print 'channel access exception: %s' %(e)

  pyca.flush_io()
  try:
    while True: raw_input()
  except:
    pass
  pyca.finalize()
