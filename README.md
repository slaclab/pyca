# pyca

PyCA (Python Channel Access) is a module that offers lightweight bindings for Python applications to access EPICS PVs. It acts as a channel access client, much like pyepics. The intention of the module is to provide better performance for embedded applications, rather than to provide an interactive interface. The most significant gains will be found when monitoring large waveforms that need to be processed before exposing them the Python layer.
