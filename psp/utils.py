import datetime
import threading

import pyca


def now():
    """
    Return string with current date and time
    """
    now = datetime.datetime.now()
    return "%04d-%02d-%02d %02d:%02d:%02d.%03d" % (now.year, now.month,
                                                   now.day, now.hour,
                                                   now.minute, now.second,
                                                   int(now.microsecond/1e3))


def set_numpy(use_numpy):
    """
    The choice to use numpy as the default handling for arrays

    Parameters
    ----------
    use_numpy: bool
        True means numpy will be used
    """
    pyca.set_numpy(use_numpy)


def ensure_context():
    """
    Let pyca create/attach context if needed. This is important if we're using
    the threading or multiprocessing modules.

    There is no harm in calling this function extra times, nothing will happen
    if the calls were unnecessary. The psp.Pv module will call this whenever a
    psp.Pv.Pv object is returned.

    The user may need to call this additional times if they are sharing pv
    objects between threads or processes.
    """
    pyca.new_context()
    pyca.attach_context()


def check_condition(fn, condition):
    """
    Check an arbitrary condition with against an arbitrary function
    """
    def inner():
        ok = condition()
        try:
            return fn(ok)
        except TypeError:
            return ok
    return inner


def any_condition(condition):
    """
    Check that any element in an array meets an arbitrary condition
    """
    return check_condition(any, condition)


def all_condition(condition):
    """
    Check that all elements in an array meets an arbitrary condition
    """
    return check_condition(all, condition)


class TimeoutSem:
    """
    Context manager/wrapper for semaphores, with a timeout on the acquire call.
    Timeout < 0 blocks indefinitely.

    Usage:
    .. code::
    with TimeoutSem(<Semaphore or Lock>, <timeout>):
        <code block>
    """
    def __init__(self, sem, timeout=-1):
        self.sem = sem
        self.timeout = timeout

    def __enter__(self):
        self.acq = False
        if self.timeout < 0:
            self.acq = self.sem.acquire(True)
        elif self.timeout == 0:
            self.acq = self.sem.acquire(False)
        else:
            self.tmo = threading.Timer(self.timeout, self.raise_tmo)
            self.tmo.start()
            self.acq = self.sem.acquire(True)
            self.tmo.cancel()
        if not self.acq:
            self.raise_tmo()

    def __exit__(self, type, value, traceback):
        try:
            if self.acq:
                self.sem.release()
        except threading.ThreadError:
            pass
        try:
            self.tmo.cancel()
        except AttributeError:
            pass

    def raise_tmo(self):
        raise threading.ThreadError("semaphore acquire timed out")
