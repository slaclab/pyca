#!/usr/bin/env python
import sys
import time

from options import Options
from Pv import Pv

import pyca


def caget(pvname):
    pv = Pv(pvname)
    pv.connect(1.)
    pv.get(False, 1.)
    pv.disconnect()
    return pv.value


if __name__ == '__main__':
    options = Options(['pvnames'], ['connect_timeout', 'get_timeout'],
                      ['ctrl', 'hex'])
    try:
        options.parse()
    except Exception as msg:
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
                print("%-30s " % (pv.name), pv.data)
            else:
                if pv.status == pyca.NO_ALARM:
                    ts = time.localtime(pv.secs+pyca.epoch)
                    tstr = time.strftime("%Y-%m-%d %H:%M:%S", ts)
                    if options.hex is not None:
                        print("%-30s %08x.%08x" % (pv.name, pv.secs, pv.nsec),
                              pv.value)
                    else:
                        print("%-30s %s.%09d" % (pv.name, tstr, pv.nsec),
                              pv.value)
                else:
                    print("%-30s %s %s" % (pv.name,
                                           pyca.severity[pv.severity],
                                           pyca.alarm[pv.status]))
        except pyca.pyexc as e:
            print('pyca exception: %s' % (e))
        except pyca.caexc as e:
            print('channel access exception: %s' % (e))
