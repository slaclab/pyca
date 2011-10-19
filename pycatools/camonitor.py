#!/usr/bin/env python

import pyca
from Pv import Pv

import sys
import time

from options import Options

class monitor(Pv):
  def __init__(self, name, maxlen):
    Pv.__init__(self, name)
    self.monitor_cb = self.monitor_handler
    self.__maxlen = maxlen
    print self.__maxlen

  def monitor_handler(self, exception=None):
    try:
      if exception is None:
        if self.status == pyca.NO_ALARM:
          ts = time.localtime(self.secs+pyca.epoch)
          tstr = time.strftime("%Y-%m-%d %H:%M:%S", ts)
          if (self.__maxlen is not None) and (len(self.value) > int(self.__maxlen)):
            value = self.value[0:10]
          else:
            value = self.value
          print "%-30s %08x.%08x" %(self.name, self.secs, self.nsec), value
        else:
          print "%-30s %s %s" %(self.name, 
                                pyca.severity[self.severity],
                                pyca.alarm[self.status])
      else:
        print "%-30s " %(self.name), exception
    except Exception, e:
      print e

if __name__ == '__main__':
  options = Options(['pvnames'], ['timeout', 'maxlen'], [])
  try:
    options.parse()
  except Exception, msg:
    options.usage(str(msg))
    sys.exit()

  pvnames = options.pvnames.split()
  if options.timeout is not None:
    timeout = float(options.timeout)
  else:
    timeout = 1

  evtmask = pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM 

  for pvname in pvnames:
    try:
      pv = monitor(pvname, options.maxlen)
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

