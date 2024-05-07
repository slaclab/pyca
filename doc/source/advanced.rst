Advanced Uses
=============
While many of the use cases for this module will end with simple
:func:`.put` and :func:`.get` calls, **PSP** supports a number of
higher level functions to support a number of use cases.

Monitoring
^^^^^^^^^^
Often times the user may want to keep track of how a PV's value changes over
time. The :class:`.Pv` has a few internal methods that make this easy. Once the
PV is initialized, the most recent value can be quickly accessed from the
:attr:`.Pv.value`. Now if the object is unmonitored, this will only be updated
by a ``get`` call. While refreshing the channel information this way once or
twice in a script is fine, repeated calls can be cumbersome. This is where
monitor comes in. While the PV is actively being monitored, the value attribute
will be actively updated by an internal callback function. This mode of
operation can either be specified at the time of object initialization or later
by calling :meth:`monitor_start`. For example, here is a quick script that
looks at a PV and checks one second later if the value changed

.. code-block:: python

    import time
    from psp import PV

    mon_pv = PV(<pvname>,monitor=True)
    initial = mon_pv.value
    time.sleep(5.0)

    if not initial == mon_pv.value:
        print 'PV value has changed!'

Sometimes checking the current value of the PV isn't enough, instead the
history of the PV needs to be tracked. Each PV object has a
:attr:`.Pv.monitor_append` attribute that determines how this data is stored.
In the default mode, as shown above, the ``value`` is overwritten and the class
moves on, but in the appending mode, each update is stored in the
:attr:`.Pv.values`. This allows the user to quickly keep track of rapidly moving PV.

.. code-block:: python

    import time
    from psp import PV

    mon_pv = PV(<pvname>)
    mon_pv.monitor_start(monitor_append=True)

    time.sleep(5.0) #Wait for changes

    if len(mon_pv.values) > 1:
        print 'This PV has updated %s times' % len(mon_pv.values)

The class even has some built-in functionality to return some basic statistics
of the class with the :meth:`.Pv.monitor_get`. For most scalar values, this is
a perfect way to monitor an EPICS channel, but for large arrays and images, it
is prudent to not monitor for too long as it easy to put a large burden on
system memory.

User-Defined Callbacks
^^^^^^^^^^^^^^^^^^^^^^
Sometimes just keeping track of the PV value isn't enough, instead an action
should be performed whenever the PV updates or connects. Callbacks allow a
simple way to handle this all behind the scenes for you. The callback function
should accept one argument, see the :meth:`.Pv.add_connection_callback` or
:meth:`.Pv.add_monitor_callback` methods for more information about what this
arguement indicates. A quick example of this feature can be seen below:

.. code-block:: python

    def updated(e):
        print 'The PV has updated`

    from psp import PV

    mon_pv = PV(<pvname>)
    cb_id  = mon_pv.add_monitor_callback(updated) #The function returns an id
                                                  #for the callback

    >>> mon_pv.monitor_start()

    >>> mon_pv.put(5)
    'The PV has updated'
    >>> mon_pv.del_monitor_callback(cb_id) #Remove the callback
    >>> mon_pv.put(2)
    >>>                 #Print not executed, because callback was removed

There are two dictionaries ``mon_cbs`` and ``con_cbs`` where all of the
callbacks are kept in case you lose track of an id. While this example is
trivial, it is easy to imagine how this can be quickly adapted to make complex
control loops without the pain of creating threads to simultaneously watch PV
values.
