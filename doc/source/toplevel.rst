Top Level Methods
*****************
The **PSP** package contains a number of methods for abstracting the chore of
object creation away from the user. While all of these functions are available
as class methods of the :class:`.psp.Pv` object, quickly accessing one or two
methods can be desirable in simpler programs.

The PV Cache
^^^^^^^^^^^^
In order to efficiently create and maintain connections formed in this lazier
mode, the module keeps track of all the PVs by adding them to the
``psp.Pv.pv_cache``, a simple Python dictionary with PV name / PV object
pairings. The benefit is that this allows multiple modules to use the same PV
object and associated channel without having to write any additional code. All
of the functions in these top level methods will first look in the cache for an
existing object with that PV name, before creating a new object. Keep in mind
this may not always be the optimal behavior. For instance in the
Camviewer application, which connects to image PVs twice, once with a small
``count`` setting to monitor a smaller subsection of the image for changes, and a
second time, without monitoring, to retrieve the whole image when needed.

Top Level API
^^^^^^^^^^^^^
.. autofunction:: psp.Pv.add_pv_to_cache
.. autofunction:: psp.Pv.monitor_start
.. autofunction:: psp.Pv.what_is_monitored
.. autofunction:: psp.Pv.monitor_stop
.. autofunction:: psp.Pv.monitor_stop_all
.. autofunction:: psp.Pv.monitor_clear
.. autofunction:: psp.Pv.clear
.. autofunction:: psp.Pv.get
.. autofunction:: psp.Pv.put
.. autofunction:: psp.Pv.wait_until_change
.. autofunction:: psp.Pv.wait_for_value
.. autofunction:: psp.Pv.wait_for_range
