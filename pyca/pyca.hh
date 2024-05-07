#include "p3compat.h"
// Structure to define a channel access PV for python
struct capv {
  PyObject_HEAD
  PyObject* name;       // PV name
  PyObject* data;       // data dictionary
  PyObject* processor;  // user processor function
  PyObject* connect_cb; // connection callback
  PyObject* monitor_cb; // monitor callback
  PyObject* rwaccess_cb;// access rights callback
  PyObject* getevt_cb;  // event callback
  PyObject* putevt_cb;  // event callback
  PyObject* simulated;  // None if real PV, otherwise just simulated.
  PyObject* use_numpy;  // True to use numpy array instead of tuple
  chid cid;             // channel access ID
  char* getbuffer;      // buffer for received data
  unsigned getbufsiz;   // received data buffer size
  char* putbuffer;      // buffer for send data
  unsigned putbufsiz;   // send data buffer size
  evid eid;             // monitor subscription
  int string_enum;      // Should enum be numeric or string?
  int count;            // How many elements are we monitoring?
  int didget;           // for simulation.
  int didmon;           // for simulation.
};

// Possible exceptions
static PyObject* pyca_pyexc = 0;
static PyObject* pyca_caexc = 0;

// Exception macros
#define pyca_raise_pyexc_int(function, reason, pv) { \
  PyErr_Format(pyca_pyexc, "%s in %s() file %s at line %d PV %p", \
    reason, function, __FILE__, __LINE__, pv); \
  return -1; }

#define pyca_raise_pyexc_pv(function, reason, pv) { \
  PyErr_Format(pyca_pyexc, "%s in %s() file %s at line %d PV %s", \
    reason, function, __FILE__, __LINE__, PyString_AsString(pv->name)); \
  return NULL; }

#define pyca_raise_pyexc(function, reason) { \
  PyErr_Format(pyca_pyexc, "%s in %s() file %s at line %d", \
    reason, function, __FILE__, __LINE__); \
  return NULL; }

#define pyca_raise_caexc_pv(function, reason, pvname) { \
  PyErr_Format(pyca_caexc, "error %d (%s) from %s() file %s at line %d PV %s",\
    reason, ca_message(reason), function, __FILE__, __LINE__, \
    PyString_AsString(pv->name)); \
  return NULL; }

#define pyca_raise_caexc(function, reason) { \
  PyErr_Format(pyca_caexc, "error %d (%s) from %s() in file %s at line %d", \
    reason, ca_message(reason), function, __FILE__, __LINE__); \
  return NULL; }

#define pyca_data_status_msg(reason, pvname) \
  PyString_FromFormat("data value status %d (%s) PV %s", \
    reason, ca_message(reason), PyString_AsString(pv->name));

// Notes on some python functions behavior towards the reference count:
// PyTuple_SetItem():        0  (tuple steals the item)
// PyDict_SetItemString():   +1
// PyDict_GetItemString():   0  (item is borrowed)
// PyObject_GetAttrString(): +1 (new reference)

// Utility function which decreases refcnt after adding item to dict
static inline int _pyca_setitem(PyObject* dict, const char* key, PyObject* val)
{
  if (val) {
    int result = PyDict_SetItemString(dict, key, val);
    Py_DECREF(val); // Above function increases refcnt for item
    return result;
  }
  return 0;
}
