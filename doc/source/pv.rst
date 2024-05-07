The PV Object
*************
This object is used to represent an individual PV and the associated Channel
Access metadata

Accessing Information
^^^^^^^^^^^^^^^^^^^^^
The PV object stores all of the gathered information in the :attr:`.Pv.data`
dictionary. Once collected this information can be quickly accessed by just
using the desired keyword like you would a class property

.. code-block:: python

    from psp import PV

    pv = PV(<pvname>, initialize=True)
    >>> 'units' in pv.data.keys():
    True
    >>> pv.units
    'mm'

The ``data`` dictionary can contain a number useful pieces of metadata
including:

    * **status** : The alarm status of the PV
    * **severity** : The severity of the PV's current alarm
    * **value** : The last updated value
    * **secs** : Time when record was last processed
    * **nsec** : Fractional seconds when record was last processed
    * **units** : Units for displaying this PV's value
    * **display_llim** : Low display limit
    * **display_hlim** : High display limit
    * **warn_llim** : Warning low limit
    * **warn_hlim** : Warning high limit
    * **alarm_llim** : The lower alarm limit
    * **alarm_hlim** : The high alarm limit
    * **ctrl_llim** : Low control limit
    * **ctrl_hlim** : The high control limit

A note on ``control``
^^^^^^^^^^^^^^^^^^^^^
What metadata is looked at during a :meth:`.Pv.get` call depends on the
:attr:`.Pv.control` setting of the PV. This flag can either be set globally for all
calls the PV makes, or be modified in individual functions where the keyword is
available. When set to ``True``, the PV will look at the limit information, the
unit of measurement, **but**, will not get the metadata associated with the
time stamp.  When set to ``False`` the timestamp information will be available,
but none of the previously mentioned limit or unit metadata will be gathered.
In either case, the alarm status severity, and PV value will be saved.

Unfortunately, this is not a choice of either this module or the underlying
``pyca`` code that performs channel access calls. Instead this is an artifact
of the way that EPICS packages together the metadata associated with PVs.
Getting all of a PVs metadata takes two seperate `get` calls, one to each
structure that stores this information.

The recommended way to deal with this caveat is to generally request Channel
Access information with :attr:`.Pv.control` set to ``False``. This way the
timestamp information is retrieved along with the PV. Then, if the user wants
to see things like the EGU field, one call using :meth:`.Pv.get` with
``control`` set to ``True`` will store the desired information in the
:attr:`.Pv.data` dictionary. Since this kind of metadata rarely changes, this
single call is usually enough for most applications. Here is an example:

.. code-block:: python

    import time
    from psp import PV

    pv = PV(<pvname>, initialize=True)
    initial = pv.get(ctrl=True) # First call to get EGU

    for i in range(5):
        new = pv.get()
        print 'Found {0} {1} at time {2}'.format(new,
                                                 pv.units,
                                                 pv.secs)
        time.sleep(1)

The ``PV`` API
^^^^^^^^^^^^^^
.. autoclass:: psp.Pv.Pv
   :members:
