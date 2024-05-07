import threading
import time
import traceback
import warnings

import numpy as np

import pyca

from . import utils

"""
   Pv module
"""


def now():
    warnings.warn(('This function has been moved to psp.utils, '
                   'it will be deprecated soon.'),
                  warnings.DeprecationWarning)
    return utils.now()


logprint = print
DEBUG = 0
pv_cache = {}
pyca_sems = {}
DEFAULT_TIMEOUT = 1.0


class Pv(pyca.capv):
    """
    The base class to represent a Channel Access PV

    Many of the parameters can be modified on a per get/put call basis,
    however, the default behavior will be determined by the state variables of
    the class. While information can be requested via the method
    :meth:`.Pv.get`, there are places where both the value of the PV and
    information on the metadata persist.

    Parameters
    ----------
    name : str
        Name of the PV

    initialize : bool , optional
        Whether to attempt to connect to the PV upon creation of the class. If
        True a connection will be attempted and data will be requested

    control : bool, optional
        Whether to gather the control Channel Access data type or the time.

    monitor : callable or bool, optional
        The choice to begin monitoring the PV upon initialization. A callable
        can also be passed and will automatically be added as a monitor
        callback using the method :meth:`.add_monitor_callback`

    count : int, optional
        If the PV interfaces with a waveform record, a smaller subsection of
        the array can be selected by setting this to the number of elements you
        wish to interact with.

    use_numpy : bool, optional
        If set to True and the PV is a waveform record, the return of get / put
        will be a NumPy type. By default, the PV will use the current pyca
        setting that can be modified by using :func:`utils.set_numpy`


    Attributes
    ----------
    isinitialized : bool
        Current initialization state

    isconnected : bool
        Current connection state

    ismonitored : bool
        Current monitor state

    data : dict
        A dictionary containing all the metadata from the channel. The contents
        of this dictionary will vary depending on the PV and
        :attr:`.Pv.control` setting

    value : float, string, int, array
        Last value seen through the channel.

    monitor_append : bool
        When the PV is being monitored the :attr:`.Pv.Value` is automatically
        updated. In addition, if this flag is set to True, when the PV channel
        sees a monitor event the value will be appended to the
        :attr:`.Pv.values`. This is useful if you want to catch all the value
        updates but you don't want to constantly poll the value of the PV.

    values : list
        If the PV is being monitored with monitor_append set, updates to the PV
        value will be saved to this list.

    timestamps : list
        If the PV object is in a mode where timestamps are included in the
        channel monitor, the timestamp of each monitor append will be saved to
        this list
    """
    def __init__(self, name, initialize=False, count=None,
                 control=False, monitor=False, use_numpy=None,
                 **kw):

        pyca.capv.__init__(self, name)

        # Threading Support
        utils.ensure_context()
        self.__con_sem = threading.Event()
        self.__init_sem = threading.Event()
        if name in pyca_sems:
            self.__pyca_sem = pyca_sems[name]
        else:
            self.__pyca_sem = pyca_sems[name] = threading.Lock()

        # Callback handlers / storage
        self.cbid = 1
        self.con_cbs = {}
        self.mon_cbs = {}
        self.rwaccess_cbs = {}
        self.connect_cb = self.__connection_handler
        self.monitor_cb = self.__monitor_handler
        self.getevt_cb = self.__getevt_handler
        self.rwaccess_cb = self.__rwaccess_handler

        # State variables
        self.ismonitored = False
        self.isconnected = False
        self.isinitialized = False
        self.monitor_append = False
        self.read_access = False
        self.write_access = False

        self.count = count
        self.control = control
        self.use_numpy = use_numpy
        self.do_initialize = initialize

        self.timestamps = []
        self.values = []

        # Create monitors
        if isinstance(monitor, bool):
            self.do_monitor = monitor
        else:
            self.do_monitor = True
            self.add_monitor_callback(monitor)

        if monitor is True:
            self.do_monitor = True
        elif monitor is False:
            self.do_monitor = False
        else:
            self.do_monitor = True
            self.add_monitor_callback(monitor)

        if self.do_initialize:
            self.connect(None)

    # Channel access callbacks
    def __connection_handler(self, isconnected):
        """
        Called by Channel Access when connection state changes
        """
        self.isconnected = isconnected

        if isconnected:
            self.__con_sem.set()
        else:
            self.__con_sem.clear()

        if self.do_initialize:
            self.get_data(self.control, -1.0, self.count)
            pyca.flush_io()

        if self.count is None:
            try:
                self.count = super().count()
            except pyca.pyexc:
                pass

        for (id, cb) in self.con_cbs.items():
            try:
                cb(isconnected)
            except Exception:
                err = "Exception in connection callback for {}:"
                logprint(err.format(self.name))
                traceback.print_exc()

    def __getevt_handler(self, e=None):
        """
        Called when data is requested over the Channel
        """
        if e is None:
            self.isinitialized = True
            self.do_initialize = False
            self.getevt_cb = None
            self.replace_access_rights_event()
            if self.do_monitor:
                self.monitor(pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM,
                             self.control, self.count)
                pyca.flush_io()
            self.__init_sem.set()

    def __monitor_handler(self, e=None):
        """
        Called during a monitor event
        """
        if not self.isinitialized:
            self.__getevt_handler(e)
        if self.monitor_append:
            self.values.append(self.value)
            self.timestamps.append(self.timestamp())
        for (id, (cb, once)) in self.mon_cbs.items():
            try:
                cb(e)
            except Exception:
                err = "Exception in monitor callback for {}:"
                logprint(err.format(self.name))
                traceback.print_exc()
            if once and e is None:
                self.del_monitor_callback(id)
        if e is None:
            if DEBUG != 0:
                logprint("{} monitoring {} {}".format(
                         utils.now(), self.name, self.timestr()))
                logprint(self.value)
        else:
            logprint("%-30s %s" % (self.name, e))

    def __rwaccess_handler(self, read_access_state, write_access_state):
        """
        Called during a read/write access change event
        """
        self.read_access = read_access_state
        self.write_access = write_access_state
        for (id, (cb, once)) in self.rwaccess_cbs.items():
            try:
                cb(read_access_state, write_access_state)
            except Exception:
                err = "Exception in read/write access callback for {}:"
                logprint(err.format(self.name))
                traceback.print_exc()
            if once:
                self.del_rwaccess_callback(id)

    def add_connection_callback(self, cb):
        """
        Add a connection callback

        Parameters
        ----------
        cb : callable
            A function to be run when the PV object makes a Channel Access
            connection. The function must accept one boolean variable which
            represents the success of the connection attempt.

        Returns
        -------
        id : int
            An integer ID number for the callback, which can be later used to
            delete the callback

        See Also
        --------
        :meth:`.add_monitor_callback`
        """
        id = self.cbid
        self.cbid += 1
        self.con_cbs[id] = cb
        return id

    def del_connection_callback(self, id):
        """
        Delete a connection callback

        Parameters
        ----------
        id : int
            The id of the callback to be deleted

        Raises
        ------
        KeyError
            If the id does not correspond to an existing callback
        """
        del self.con_cbs[id]

    def add_monitor_callback(self, cb, once=False):
        """
        Add a monitor callback

        Parameters
        ----------
        cb : callable
            A function to be run when the PV object makes a Channel Access sees
            a monitor event. The function must accept one boolean variable
            which represents the success of the connection attempt.

        Returns
        -------
        id : int
            An integer ID number for the callback, which can be later used to
            delete the callback

        See Also
        --------
        :meth:`.add_connection_callback`
        """
        id = self.cbid
        self.cbid += 1
        self.mon_cbs[id] = (cb, once)
        return id

    def del_monitor_callback(self, id):
        """
        Delete a monitor callback

        Parameters
        ----------
        id : int
            The id of the callback to be deleted

        Raises
        ------
        KeyError
            If the id does not correspond to an existing callback
        """
        del self.mon_cbs[id]

    def add_rwaccess_callback(self, cb, once=False):
        """
        Add a read/write access state change callback

        Parameters
        ----------
        cb : callable
            A function to be run when the PV object makes a Channel Access sees
            a read/write access state change event. The function must have the
            following signature: rwcb(read_access, write_access). Both
            read_access and write_access are booleans.

        Returns
        -------
        id : int
            An integer ID number for the callback, which can be later used to
            delete the callback

        See Also
        --------
        :meth:`.add_connection_callback`
        :meth:`.add_monitor_callback`
        """
        id = self.cbid
        self.cbid += 1
        self.rwaccess_cbs[id] = (cb, once)
        return id

    def del_rwaccess_callback(self, id):
        """
        Delete a read/write access state change callback

        Parameters
        ----------
        id : int
            The id of the callback to be deleted

        Raises
        ------
        KeyError
            If the id does not correspond to an existing callback
        """
        del self.rwaccess_cbs[id]

    def connect(self, timeout=None):
        """
        Create a connection to the PV through Channel Access

        If a timeout is specified, the function will wait for the connection
        state to report back as a success, raising an Exception upon failure.
        However, if no timeout is specified, no Exception will be raised, even
        upon a failed connection

        Parameters
        ----------
        timeout : float, optional
            The amount of time to wait for PV to make connection

        Returns
        -------
        isconnected : bool
            The connection state

        Raises
        ------
        pyca.pyexc
            If the user defined timeout is exceeded

        Note
        ----
        This function does not call pyca.flush_io
        """
        try:
            self.create_channel()

        except pyca.pyexc:
            logprint(f'Channel for PV {self.name} already exists')
            return self.isconnected

        if timeout is not None:
            tmo = float(timeout)
            if tmo > 0:
                self.__con_sem.wait(tmo)
                if not self.__con_sem.isSet():
                    self.disconnect()
                    raise pyca.pyexc("connection timedout for PV %s" %
                                     self.name)

        return self.isconnected

    def disconnect(self):
        """
        Disconnect the associated channel
        """
        try:
            self.clear_channel()

        except pyca.pyexc:
            logprint('Channel for PV {:} is already '
                     'disconnected'.format(self.name))

        self.__con_sem.clear()
        self.isconnected = False

    def monitor(self, mask=pyca.DBE_VALUE | pyca.DBE_LOG | pyca.DBE_ALARM,
                ctrl=None, count=None, wait_for_init=True):
        """
        Subscribe to monitor events from the PV channel

        Much of the functionality of this meth is wrapped in higher level
        utilities

        Parameters
        ----------
        mask : int
            Mask to select which monitor events to subscribe to using pyca
            variables including, pyca.DBE_VALUE for changes to PV value,
            pyca.DBE_ALARM, for changes in the alarm state, and pyca.DBE_LOG
            for anything else

        ctrl : bool, optional
            Choice to monitor the control or time value. By default,
            :attr:`.control` is used

        count : int, optional
            Subsection of waveform record to monitor. By default,
            :attr:`.count` is used

        wait_for_init : bool, optional
            Whether to wait for an initial value to arrive for the PV before
            returning.  If False, the PV's value and metadata might not be
            populated when this function returns.  Defaults to True.

        See Also
        --------
        :meth:`.monitor_start`, :meth:`.monitor_stop`
        """
        if not self.isconnected:
            self.connect(DEFAULT_TIMEOUT)

        if not self.isconnected:
            raise pyca.pyexc("monitor: connection timedout for PV %s" %
                             self.name)

        if ctrl is None:
            ctrl = self.control

        if count is None:
            count = self.count

        self.subscribe_channel(mask, ctrl, count)

        # capv.get_data cannot be called inside a callback. Putting this
        # inside a try/except block lets us skip this step when it would
        # fail. Tons of code calls Pv.monitor inside a connection callback,
        # such as our built-in Pv.__getevt_handler.
        #
        # The purpose of this step is to initialize the .value when we call
        # .monitor() in an interactive session.
        if wait_for_init:
            try:
                self.get()
            except pyca.caexc:
                pass

        self.ismonitored = True

    def unsubscribe(self):
        """
        Close Channel Access Subscription

        This is the lower level implementation. It is best to use
        :meth:`monitor_stop` instead.

        See Also
        --------
        :meth:`.monitor_stop`
        """
        self.unsubscribe_channel()
        self.ismonitored = False

    def get(self, count=None, timeout=DEFAULT_TIMEOUT, as_string=False, **kw):
        """
        Get and return the value of the PV

        If the PV has not been previously connected, this will automatically
        attempt to use :meth:`.connect`. If you are expecting a waveform PV
        and want to choose to use a numpy array or not, set the attribute
        :attr:`.use_numpy`.

        Parameters
        ----------
        count : int, optional
            Maximum number of array elements to be return. By default uses
            :attr:`.count`

        ctrl : bool, optional
            Whether to get the control form information

        timeout : float or None, optional
            Time to wait for data to be returned. If None, no timeout is used.
            If timeout is None, this method may return None if the PV's
            value has not arrived yet.

        as_string : bool , optional
            Return the value as a string type. For Enum PVs, the default
            behavior is to return the integer representing the current value.
            However, if as_string is set to True, this will return the
            associated string for the Enum

        Returns
        -------
        value : float, int, str
            Current value of the PV

        Raises
        ------
        pyca.pyexc
            If PV connection fails
        """
        if not count:
            count = self.count

        if timeout:
            try:
                tmo = float(timeout)
            except ValueError:
                tmo = DEFAULT_TIMEOUT
        else:
            tmo = -1.0

        try:
            ctrl = kw['ctrl']
            if ctrl is None:
                ctrl = False

        except KeyError:
            ctrl = self.control

        if DEBUG != 0:
            logprint("caget %s: " % self.name)

        if not self.isconnected and not self.connect(DEFAULT_TIMEOUT):
            raise pyca.pyexc("get: connection timedout for PV %s" % self.name)

        with utils.TimeoutSem(self.__pyca_sem, tmo):
            self.get_data(ctrl, tmo, count)

        if tmo > 0 and DEBUG != 0:
            logprint("got %s\n" % self.value.__str__())

        if "value" not in self.data:
            # If the timeout was None, there's a chance that the value hasn't arrived yet.
            return None

        if as_string:
            if self.type() == 'DBF_ENUM':
                enums = self.get_enum_set(timeout=tmo)
                if len(enums) > self.value >= 0:
                    return enums[self.value]
                else:
                    raise IndexError('{:} is not a valid enumeration '
                                     'of {:}'.format(self.value, self.name))
            else:
                return str(self.value)
        return self.value

    def put(self, value, timeout=DEFAULT_TIMEOUT, **kw):
        """
        Set the PV value

        If the PV has not been previously connected, this will automatically
        attempt to use :meth:`.connect`.

        Parameters
        ----------
        value : float, int, str or array
            Desired PV value

        timeout : float or None, optional
            Time to wait for put to be completed. If None, no timeout is used

        Returns
        -------
        value : float, int, str, or array
            The value given to the PV

        TODO : Add a put_complete which confirms the success of the function
        """
        if DEBUG != 0:
            logprint("caput {} in {}\n".format(value, self.name))

        if timeout:
            try:
                tmo = float(timeout)
            except ValueError:
                tmo = DEFAULT_TIMEOUT
        else:
            tmo = -1.0

        if not self.isinitialized:
            if self.isconnected:
                self.get_data(self.control, -1.0, self.count)
                pyca.flush_io()
            else:
                self.do_initialize = True
                self.connect()

            self.wait_ready(DEFAULT_TIMEOUT * 2)

        with utils.TimeoutSem(self.__pyca_sem, tmo):
            self.put_data(value, tmo)

        return value

    def get_enum_set(self, timeout=1.0):
        """
        Return the ENUM types associated with the PV

        Since this information usually only changes when an IOC is remade, it
        is only necessary to get this information once. After the first call,
        the tuple is stored in the dictionary :attr:`data` and accessible via
        the property :attr:`enum_set`

        Parameters
        ----------
        timeout : float or None, optional
            Maximum time to wait to hear a response

        Returns
        -------
        enum_set : tuple
            A tuple of each ENUM value with the associated integer as the index

        Raises
        ------
        pyca.pyexc
            If the PV is not an ENUM type, this will be raised
        """
        tmo = float(timeout)
        self.get_enum_strings(tmo)
        self.enum_set = self.data["enum_set"]
        return self.enum_set

###########################
#  "Higher level" methods #
###########################

    def wait_ready(self, timeout=None):
        """
        Wait for the PV to be initialized

        Parameters
        ----------
        timeout : float or None, optional
            Maximum time to wait to hear a response

        Raises
        ------
        pyca.pyexc
            If timeout is exceeded
        """
        pyca.flush_io()
        self.__init_sem.wait(timeout)
        if not self.__init_sem.isSet():
            raise pyca.pyexc("ready timedout for PV %s" % (self.name))

    # The monitor callback used in wait_condition.
    def __wc_mon_cb(self, e, condition, sem):
        if (e is None) and condition():
            sem.set()

    def wait_condition(self, condition, timeout=60, check_first=True):
        """
        Wait for an arbitrary condition to be True

        Parameters
        ----------
        condition : callable
            Method to be run with no arguments that returns a bool

        timeout : float, optional
            Maximum time to wait for condition to evaluate to True

        check_first : bool, optional
            Whether to check if the condition currently evaluates to True
            first.  Since the wait condition is only retried from a monitor
            callback, the condition will only be rechecked when the PV receives
            a monitor event. This means that if you don't immediately check
            the wait condition and then the PV never receives a monitor event
            the condition will never be tested

        Returns
        -------
        result : bool
            Whether or not the wait has stopped due to a timeout (False) or
            because the condition has evaluated to True
        """
        self._ensure_monitored()
        sem = threading.Event()
        id = self.add_monitor_callback(lambda e: self.__wc_mon_cb(e, condition,
                                                                  sem),
                                       False)
        if check_first and condition():
            self.del_monitor_callback(id)
            return True

        sem.wait(timeout)
        self.del_monitor_callback(id)
        return sem.is_set()

    def wait_until_change(self, timeout=60):
        """
        Wait until the PV value changes

        Parameters
        ----------
        timeout : float, optional
            Maximum time to wait for PV value to change

        Returns
        -------
        result : bool
            Whether or not the wait has stopped due to a timeout (False) or
            because the PV value has changed
        """
        self._ensure_monitored()
        value = self.value
        # Consider changed if any element is different
        condition = utils.any_condition(lambda: self.value != value)
        result = self.wait_condition(condition, timeout, True)
        if not result:
            logprint("waiting for pv %s to change timed out" % self.name)
        return result

    def wait_for_value(self, value, timeout=60):
        """
        Wait for a PV to reach a specific value

        Parameters
        ----------
        value : float, int, string
            The desired value to wait for the PV to reach

        timeout : float, optional
            Maximum time to wait for PV value to reach value

        Returns
        -------
        result : bool
            Whether or not the wait has stopped due to a timeout (False) or
            because the PV value has reached the value
        """
        self._ensure_monitored()
        # Consider correct value if all values match
        condition = utils.all_condition(lambda: self.value == value)
        result = self.wait_condition(condition, timeout, True)
        if not result:
            logprint("waiting for pv {} to become {} timed out".format(self.name,
                                                                       value))
        return result

    def wait_for_range(self, low, high, timeout=60):
        """
        Wait for a PV to enter a specific range

        Parameters
        ----------
        low : float, int
            The lower bound of the desired range

        high : float, int
            The upper bound of the desired range

        timeout : float, optional
            Maximum time to wait for PV value to reach value

        Returns
        -------
        result : bool
            Whether or not the wait has stopped due to a timeout (False) or
            because the PV value has reached the value
        """
        self._ensure_monitored()
        # Consider in range if all values above low and all below high
        low_cond = utils.all_condition(lambda: low <= self.value)
        high_cond = utils.all_condition(lambda: self.value <= high)
        result = self.wait_condition(lambda: low_cond() and high_cond(),
                                     timeout, True)
        if not result:
            logprint("waiting for pv %s to be between %s and %s timed out" %
                     (self.name, low, high))
        return result

    def _ensure_monitored(self):
        """
        Make sure PV has been initialized and monitored
        """
        self.get()
        if not self.ismonitored:
            self.monitor()
            pyca.flush_io()

    def timestamp(self):
        """
        Return a timestamp from the last time the PV received an update
        """
        return (self.secs + pyca.epoch, self.nsec)

    def timestr(self):
        """
        Return a string representation of the PV timestamp
        """
        ts = time.localtime(self.secs+pyca.epoch)
        tstr = time.strftime("%Y-%m-%d %H:%M:%S", ts)
        tstr = tstr + ".%09d" % self.nsec
        return tstr

    def monitor_start(self, monitor_append=False):
        """
        Start a monitoring process on the PV channel.

        Parameters
        ----------
        monitor_apppend : bool, optional
            The choice of storing all updated values in a list, or simply
            overwriting the value attribute each time. This will change the
            :attr:`.monitor_append` attribute
        """
        if not self.isinitialized:
            if self.isconnected:
                self.get_data(self.control, -1.0, self.count)
                pyca.flush_io()
            else:
                self.do_initialize = True
                self.connect()

            self.wait_ready()

        if self.ismonitored:
            if monitor_append == self.monitor_append:
                return
            if monitor_append:
                self.monitor_clear()
            self.monitor_append = monitor_append
            return
        self.monitor_append = monitor_append
        self.monitor_clear()
        self.monitor()
        pyca.flush_io()

    def monitor_stop(self):
        """
        Stop monitoring the channel for updates

        Note
        ----
        This does not clear the :attr:`values` list
        """
        if self.ismonitored:
            self.unsubscribe()

    def monitor_clear(self):
        """
        Clear the :attr:`values` list
        """
        self.values = []
        self.timestamps = []

    def monitor_get(self):
        """
        Returns the statistics for the current :attr:`values` list as
        dictionary

        Returns
        -------
        ret : dict
            A dictionary with the keys : mean, std, num, err. If no monitor
            events have been stored, these will simply be np.nan values
        """
        a = np.array(self.values[1:])
        ret = {}
        if (len(a) == 0):
            ret["mean"] = ret["std"] = ret["err"] = np.nan
            ret["num"] = 0
            if DEBUG != 0:
                logprint("No pulses.... while monitoring %s" % self.name)
            return ret

        ret["mean"] = a.mean()
        ret["std"] = a.std()
        ret["num"] = len(a)
        ret["err"] = ret["std"]/np.sqrt(ret["num"])
        if DEBUG != 0:
            logprint("get monitoring for %s" % self.name)
        return ret

    def __getattr__(self, name):
        """
        Redefined to look in self.data for a keyword
        """
        if name in self.data:
            return self.data[name]
        else:
            return self.__dict__[name]


#########################
# Stand alone routines! #
#########################

def add_pv_to_cache(pvname):
    """
    Add a PV to the save cache of PV objects

    If a PV with that name is already in the cache, it will not be added twice.

    Parameters
    ----------
    pvname : str
        The name of the desired PV

    Returns
    -------
    PV : object
        A PV object
    """
    utils.ensure_context()
    if pvname not in pv_cache.keys():
        pv_cache[pvname] = Pv(pvname)
    return pv_cache[pvname]


def monitor_start(pvname, monitor_append=False):
    """
    Start monitoring a PV.

    Parameters
    ----------
    pvname : str
        The name of the desired PV

    monitor_append : bool , optional
        The choice of storing all updated values in a list, or simply
        overwriting the value attribute each time. This will change the
        :attr:`.monitor_append` attribute

    See Also
    --------
    :meth:`.Pv.monitor_start`
    """
    add_pv_to_cache(pvname)
    pv_cache[pvname].monitor_start(monitor_append)


def monitor_stop(pvname):
    """
    Stop monitoring a PV

    Parameters
    ----------
    pvname : str
        The name of the desired PV

    See Also
    --------
    :meth:`.Pv.monitor_stop`
    """
    add_pv_to_cache(pvname)
    pv_cache[pvname].monitor_stop()


def monitor_clear(pvname):
    """
    Clear the :attr:`.Pv.values` list

    Parameters
    ----------
    pvname : str
        the name of the desired pv

    See Also
    --------
    :meth:`.Pv.monitor_clear`
    """
    add_pv_to_cache(pvname)
    pv_cache[pvname].monitor_clear()


def monitor_get(pvname):
    """
    Returns the statistics for the current :attr:`values` list as
    dictionary

    Parameters
    ----------
    pvname : str
        the name of the desired pv

    Returns
    -------
    ret : dict
        A dictionary with the keys : mean, std, num, err. If no monitor
        events have been stored, these will simply be np.nan values

    See Also
    --------
    :meth:`.Pv.monitor_get`
    """
    add_pv_to_cache(pvname)
    return pv_cache[pvname].monitor_get()


def monitor_stop_all(clear=False):
    """
    Stop monitoring for all PVs defined in cache list

    Parameters
    ----------
    clear : bool, optional
        Choice to also clear all saved values

    See Also
    --------
    :func:`.monitor_stop`, :meth:`.Pv.monitor_clear`

    """
    for pv in pv_cache.keys():
        pv_cache[pv].monitor_stop()
        if clear:
            pv_cache[pv].monitor_clear()
        logprint("stopping monitoring for %s" % pv)


def get(pvname, as_string=False):
    """
    Return the current value for a PV

    Parameters
    ----------
    pvname : str
        the name of the desired pv

    as_string : bool , optional
        Return the value as a string type. For Enum PVs, the default
        behavior is to return the integer representing the current value.
        However, if as_string is set to True, this will return the
        associated string for the Enum

    Returns
    -------
    value : float, int, str
        Current value of the PV

    See Also
    --------
    :meth:`.Pv.get`
    """
    add_pv_to_cache(pvname)
    return pv_cache[pvname].get(as_string=as_string, timeout=DEFAULT_TIMEOUT)


def put(pvname, value):
    """
    Write value to a PV

    Parameters
    ----------
    pvname : str
        the name of the desired pv

    value : float, int, str or array
        Desired PV value

    Returns
    -------
    value : float, int, str, or array
        The value given to the PV

    See Also
    --------
    :meth:`.Pv.put`
    """
    add_pv_to_cache(pvname)
    return pv_cache[pvname].put(value, timeout=DEFAULT_TIMEOUT)


def wait_until_change(pvname, timeout=60):
    """
    Wait until the PV value changes

    Parameters
    ----------
    pvname : str
        the name of the desired pv

    timeout : float, optional
        Maximum time to wait for PV value to change

    Returns
    -------
    result : bool
        Whether or not the wait has stopped due to a timeout (False) or
        because the PV value has changed

    See Also
    --------
    :meth:`.Pv.wait_until_change`
    """
    pv = add_pv_to_cache(pvname)
    ismon = pv.ismonitored
    if not ismon:
        pv.monitor_start(False)
    changed = pv.wait_until_change(timeout=timeout)
    if not ismon:
        monitor_stop(pvname)
    return changed


def wait_for_value(pvname, value, timeout=60):
    """
    Wait for a PV to reach a specific value

    Parameters
    ----------
    pvname : str
        the name of the desired pv

    value : float, int, string
        The desired value to wait for the PV to reach

    timeout : float, optional
        Maximum time to wait for PV value to reach value

    Returns
    -------
    result : bool
        Whether or not the wait has stopped due to a timeout (False) or
        because the PV value has reached the value

    See Also
    --------
    :meth:`.Pv.wait_for_value`
    """
    pv = add_pv_to_cache(pvname)
    ismon = pv.ismonitored
    if not ismon:
        pv.monitor_start(False)
    is_value = pv.wait_for_value(value, timeout=timeout)
    if not ismon:
        monitor_stop(pvname)
    return is_value


def wait_for_range(pvname, low, high, timeout=60):
    """
    Wait for a PV to enter a specific range

    Parameters
    ----------
    low : float, int
        The lower bound of the desired range

    high : float, int
        The upper bound of the desired range

    timeout : float, optional
        Maximum time to wait for PV value to reach value

    Returns
    -------
    result : bool
        Whether or not the wait has stopped due to a timeout (False) or
        because the PV value has reached the value

    See Also
    --------
    :meth:`.Pv.wait_for_value`
    """
    pv = add_pv_to_cache(pvname)
    ismon = pv.ismonitored
    if not ismon:
        pv.monitor_start(False)
    in_range = pv.wait_for_range(low, high, timeout=timeout)
    if not ismon:
        monitor_stop(pvname)
    return in_range


def clear():
    """
    Stop monitoring and disconnect all PVs
    """
    for pv in pv_cache:
        monitor_stop(pv)
        monitor_clear(pv)
        pv_cache[pv].disconnect()
    pv_cache.clear()


def what_is_monitored():
    """
    Print a list of PVs that are currently monitored
    """
    for pv in pv_cache:
        if (pv_cache[pv].ismonitored):
            logprint("pv %s is currently monitored" % pv_cache[pv].name)
