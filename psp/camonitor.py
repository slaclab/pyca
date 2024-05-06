#!/usr/bin/env python
import sys

from options import Options
from Pv import Pv

import pyca


class monitor(Pv):
    def __init__(self, name, maxlen, hex):
        Pv.__init__(self, name)
        self.monitor_cb = self.monitor_handler
        self.__maxlen = maxlen
        self.__hex = hex

    def monitor_handler(self, exception=None):
        try:
            if exception is None:
                if self.status == pyca.NO_ALARM:
                    try:
                        if ((self.__maxlen is not None)
                                and (len(self.value) > int(self.__maxlen))):
                            value = self.value[0:10]
                        else:
                            value = self.value
                    except Exception:
                        value = self.value
                    if self.__hex:
                        try:
                            value = ["0x%x" % v for v in value]
                        except Exception:
                            value = "0x%x" % value  # must be a scalar!
                    print("%-30s %08x.%08x" % (self.name, self.secs,
                                               self.nsec), value)
                else:
                    print("%-30s %s %s" % (self.name,
                                           pyca.severity[self.severity],
                                           pyca.alarm[self.status]))
            else:
                print("%-30s " % (self.name), exception)
        except Exception as e:
            print(e)


if __name__ == '__main__':
    options = Options(['pvnames'], ['timeout', 'maxlen'], ['hex'])
    try:
        options.parse()
    except Exception as msg:
        options.usage(str(msg))
        sys.exit()

    hex = False if (options.hex is None) else True
    print(hex)
    pvnames = options.pvnames.split()
    if options.timeout is not None:
        timeout = float(options.timeout)
    else:
        timeout = 1

    evtmask = pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM

    for pvname in pvnames:
        try:
            pv = monitor(pvname, options.maxlen, hex)
            pv.connect(timeout)
            pv.monitor(evtmask, ctrl=False)
        except pyca.pyexc as e:
            print('pyca exception: %s' % (e))
        except pyca.caexc as e:
            print('channel access exception: %s' % (e))

    pyca.flush_io()
    if sys.version_info.major_version >= 3:
        raw_input = input
    try:
        while True:
            raw_input()
    except Exception:
        pass
