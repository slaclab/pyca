# pyca

Pyca, which stands for Python Channel Access, is a module that offers lightweight bindings for Python applications to access EPICS PVs. It acts as a channel access client, much like pyepics. The intention of the module is to provide better performance for embedded applications, rather than to provide an interactive interface. The most significant gains will be found when monitoring large waveforms that need to be processed before exposing them the Python layer.

This module was imported from an old svn repository and was previously used only for internal projects. I plan to clean it up a bit and have a working conda recipe before tagging a public release. The module is typically used with another interface layer called psp, which I will also bring to git and github at some point.

### todo list
- cleaner documentation
- clean up setup.py
- conda-recipe
- Python 3 compatibility
