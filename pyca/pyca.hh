#ifdef PYCA_PLAYBACK
class playback_thread;
#endif // PYCA_PLAYBACK

// Structure to define a channel access PV for python
struct capv {
  PyObject_HEAD
  PyObject* name;      // PV name
  PyObject* data;      // data dictionary
  PyObject* processor; // user processor function
  PyObject* connect_cb; // connection callback
  PyObject* monitor_cb; // monitor callback
  PyObject* getevt_cb;  // event callback
  PyObject* putevt_cb;  // event callback
  chid cid;            // channel access ID  
  char* getbuffer;     // buffer for received data
  unsigned getbufsiz;  // received data buffer size
  char* putbuffer;     // buffer for send data
  unsigned putbufsiz;  // send data buffer size
  evid eid;            // monitor subscription
  int string_enum;     // Should enum be numeric or string?

#ifdef PYCA_PLAYBACK
  FILE *playback;        // NULL if not in playback mode
  int adjusttime;        // Should we adjust times to the present?
  int realtime;          // 1 = playback is realtime, otherwise single_step().

  chtype dbftype;        // DBF type in the playback file
  int dbrtype;           // DBR type in the playback file
  int nelem;             // nelem in the playback file

  char* nxtbuffer;       // buffer for next received data
  unsigned nxtbufsiz;    // next received data buffer size

  epicsTimeStamp actual; // actual time of the next buffered event, set from timestamp
                         // in file as soon as read in.
  epicsTimeStamp stamp;  // "present" time of the next buffered event.
                         // If playing back in real time, just add the increment between
                         // events in file.  Otherwise, set something close to the present,
                         // but preserve the fiducial bits.

  playback_thread *thid; // thread ID for subscriptions.

  enum { PB_NOTCONN, PB_OK, PB_EOF } status; // status of the playback.
  int play_forward;      // 1 = forwards, 0 = backwards
#endif // PYCA_PLAYBACK
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

static PyObject* ok()
{
  Py_INCREF(Py_None);
  return Py_None;
}

