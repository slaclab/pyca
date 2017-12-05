#!/usr/bin/env python

from Pv import Pv

import pyca
import sys

from options import Options

def caput(pvname, value):
    pv = Pv(pvname)
    pv.connect(1.)
    pv.put(value, timeout=1.)
    pv.disconnect()

if __name__ == '__main__':
    options = Options(['pvname', 'value'], ['connect_timeout','put_timeout'], [])
    try:
        options.parse()
    except Exception, msg:
        options.usage(str(msg))
        sys.exit()

    if options.connect_timeout is not None:
        connect_timeout = float(options.connect_timeout)
    else:
        connect_timeout = 1.0
    if options.put_timeout is not None:
        put_timeout = float(options.put_timeout)
    else:
        put_timeout = 1.0

    try:
        value = int(options.value)
    except:
        value = float(options.value)

    try:
        pv = Pv(options.pvname)
        pv.connect(connect_timeout)
        pv.get(ctrl=False, timeout=put_timeout)
        print('Old: %-30s' %(pv.name), pv.value)
        pv.put(value, put_timeout)
        pv.get(ctrl=False, timeout=put_timeout)
        print('New: %-30s' %(pv.name), pv.value)
    except pyca.pyexc, e:
        print('pyca exception: %s' %(e))
    except pyca.caexc, e:
        print('channel access exception: %s' %(e))
