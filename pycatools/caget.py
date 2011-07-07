#!/usr/bin/env python

import pyca
from Pv import Pv

import sys
import time

from options import Options

def caget(pvname):
  pv = Pv(pvname)
  pv.connect(1.)
  pv.get(False, 1.)
  pv.disconnect()
  return pv.value
  

if __name__ == '__main__':
  options = Options(['pvnames'], ['connect_timeout','get_timeout'], ['ctrl'])
  try:
    options.parse()
  except Exception, msg:
    options.usage(str(msg))
    sys.exit()

  pvnames = options.pvnames.split()
  ctrl = options.ctrl is not None
  if options.connect_timeout is not None:
    connect_timeout = float(options.connect_timeout)
  else:
    connect_timeout = 1.0
  if options.get_timeout is not None:
    get_timeout = float(options.get_timeout)
  else:
    get_timeout = 1.0

  for pvname in pvnames:
    try:
      pv = Pv(pvname)
      pv.connect(connect_timeout)
      pv.get(ctrl, get_timeout)
      if ctrl:
        print "%-30s " %(pv.name), pv.data
      else:
        if pv.status == pyca.NO_ALARM:
          ts = time.localtime(pv.secs+pyca.epoch)
          tstr = time.strftime("%Y-%m-%d %H:%M:%S", ts)
          print "%-30s %s.%09d" %(pv.name, tstr, pv.nsec), pv.value
        else:
          print "%-30s %s %s" %(pv.name, 
                                pyca.severity[pv.severity],
                                pyca.alarm[pv.status])
    except pyca.pyexc, e:
      print 'pyca exception: %s' %(e)
    except pyca.caexc, e:
      print 'channel access exception: %s' %(e)
