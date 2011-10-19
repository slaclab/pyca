#include <Python.h>
#include <structmember.h>

#include <cadef.h>
#include <alarm.h>

#include <pthread.h>

#include "pyca.hh"
#include "getfunctions.hh"
#include "putfunctions.hh"
#include "handlers.hh"

extern "C" {
  // Python methods for channel access PV types
  static PyObject* create_channel(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (pv->cid) {
      pyca_raise_pyexc_pv("create_channel", "channel already created", pv);
    }
    const char* name = PyString_AsString(pv->name);
    const int capriority = 10;
    int result = ca_create_channel(name,
                                   pyca_connection_handler,
                                   self,
                                   capriority,
                                   &pv->cid);
    if (result != ECA_NORMAL) {
      pyca_raise_caexc_pv("ca_create_channel", result, pv);
    }
    return ok();
  }

  static PyObject* clear_channel(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    chid cid = pv->cid;
    if (!cid) {
      pyca_raise_pyexc_pv("clear_channel", "channel is null", pv);
    }
    int result = ca_clear_channel(cid);
    if (result != ECA_NORMAL) {
      pyca_raise_caexc_pv("ca_clear_channel", result, pv);
    }
    pv->cid = 0;
    return ok();
  }

  static PyObject* subscribe_channel(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    PyObject* pyctrl;
    PyObject* pymsk;
    if (!PyArg_ParseTuple(args, "OO:subscribe", &pymsk, &pyctrl) ||
	!PyLong_Check(pymsk) ||
	!PyBool_Check(pyctrl)) {
      pyca_raise_pyexc_pv("subscribe_channel", "error parsing arguments", pv);
    }
    chid cid = pv->cid;
    if (!cid) {
      pyca_raise_pyexc_pv("subscribe_channel", "channel is null", pv);
    }
    int count = ca_element_count(cid);
    short type = ca_field_type(cid);
    if (count == 0 || type == TYPENOTCONN) {
      pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
    }
    short dbr_type = (Py_True == pyctrl) ?
      dbf_type_to_DBR_CTRL(type) : // Asks IOC to send status+time+limits+value
      dbf_type_to_DBR_TIME(type);  // Asks IOC to send status+time+value
    unsigned long event_mask = PyLong_AsLong(pymsk);
    int result = ca_create_subscription(dbr_type,
                                        count,
                                        cid,
                                        event_mask,
                                        pyca_monitor_handler,
                                        pv,
                                        &pv->eid);
    if (result != ECA_NORMAL) {
      pyca_raise_caexc_pv("ca_create_subscription", result, pv);
    }
    return ok();
  }

  static PyObject* unsubscribe_channel(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    evid eid = pv->eid;
    if (eid) {
      int result = ca_clear_subscription(eid);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_clear_subscription", result, pv);
      }
      pv->eid = 0;
    }
    return ok();
  }

  static PyObject* get_data(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    PyObject* pyctrl;
    PyObject* pytmo;
    if (!PyArg_ParseTuple(args, "OO:get", &pyctrl, &pytmo) ||
	!PyBool_Check(pyctrl) ||
	!PyFloat_Check(pytmo)) {
       pyca_raise_pyexc_pv("get_data", "error parsing arguments", pv);
    }
    chid cid = pv->cid;
    if (!cid) {
      pyca_raise_pyexc_pv("subscribe_channel", "channel is null", pv);
    }
    int count = ca_element_count(cid);
    short type = ca_field_type(cid);
    if (count == 0 || type == TYPENOTCONN) {
      pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
    }
    short dbr_type = (Py_True == pyctrl) ?
      dbf_type_to_DBR_CTRL(type) : // Asks IOC to send status+time+limits+value
      dbf_type_to_DBR_TIME(type);  // Asks IOC to send status+time+value
    double timeout = PyFloat_AsDouble(pytmo);
    if (timeout < 0) {
      int result = ca_array_get_callback(dbr_type,
                                         count,
                                         cid,
                                         pyca_getevent_handler,
                                         pv);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_array_get_callback", result, pv);
      }
    } else {
      void* buffer = _pyca_adjust_buffer_size(pv, dbr_type, count);
      if (!buffer) {
        pyca_raise_pyexc_pv("get_data", "un-handled type", pv);
      }
      int result = ca_array_get(dbr_type,
                                count,
                                cid,
                                buffer);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_array_get", result, pv);
      }
      result = ca_pend_io(timeout);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_pend_io", result, pv);
      }      
      if (!_pyca_event_process(pv, buffer, dbr_type, count)) {
        pyca_raise_pyexc_pv("get_data", "un-handled type", pv);
      }
    }
    return ok();
  }

  static PyObject* put_data(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    PyObject* pyval;
    PyObject* pytmo;
    if (!PyArg_ParseTuple(args, "OO:put", &pyval, &pytmo) ||
	!PyFloat_Check(pytmo)) {
      pyca_raise_pyexc_pv("put_data", "error parsing arguments", pv);
    }    
    chid cid = pv->cid;
    if (!cid) {
      pyca_raise_pyexc_pv("subscribe_channel", "channel is null", pv);
    }
    int count = ca_element_count(cid);
    short type = ca_field_type(cid);
    if (count == 0 || type == TYPENOTCONN) {
      pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
    }
    short dbr_type = dbf_type_to_DBR(type);
    const void* buffer = _pyca_put_buffer(pv, pyval, dbr_type, count);
    if (!buffer) {
      pyca_raise_pyexc_pv("put_data", "un-handled type", pv);
    }
    double timeout = PyFloat_AsDouble(pytmo);
    if (timeout < 0) {
      int result = ca_array_put_callback(dbr_type,
                                         count,
                                         cid,
                                         buffer,
                                         pyca_putevent_handler,
                                         pv);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_array_put_callback", result, pv);
      }
    } else {
      int result = ca_array_put(dbr_type,
                                count,
                                cid,
                                buffer);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_array_put", result, pv);
      }
      result = ca_pend_io(timeout);
      if (result != ECA_NORMAL) {
        pyca_raise_caexc_pv("ca_pend_io", result, pv);
      }
    }
    return ok();
  }

  static PyObject* host(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
    return PyString_FromString(ca_host_name(pv->cid));
  }

  static PyObject* state(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
    return PyInt_FromLong(ca_state(pv->cid));
  }

  static PyObject* count(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
    return PyInt_FromLong(ca_element_count(pv->cid));
  }

  static PyObject* type(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
    return PyString_FromString(dbf_type_to_text(ca_field_type(pv->cid)));
  }

  static PyObject* rwaccess(PyObject* self, PyObject* args)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
    int rw = ca_read_access(pv->cid) ? 1 : 0;
    rw |= ca_write_access(pv->cid) ? 2 : 0;
    return PyInt_FromLong(rw);
  }

  // Built-in methods for the capv type
  static int capv_init(PyObject* self, PyObject* args, PyObject* kwds)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    if (!PyArg_ParseTuple(args, "O:capv_init", &pv->name) ||
	!PyString_Check(pv->name)) {
      pyca_raise_pyexc_int("capv_init", "cannot get PV name", pv);
    }
    Py_INCREF(pv->name);
    pv->processor = 0;
    pv->connect_cb = 0;
    pv->monitor_cb = 0;
    pv->getevt_cb = 0;
    pv->putevt_cb = 0;
    pv->cid = 0;
    pv->getbuffer = 0;
    pv->getbufsiz = 0;
    pv->putbuffer = 0;
    pv->putbufsiz = 0;
    pv->eid = 0;
    return 0;
  }

  static void capv_dealloc(PyObject* self)
  {
    capv* pv = reinterpret_cast<capv*>(self);
    Py_XDECREF(pv->data);
    Py_XDECREF(pv->name);
    if (pv->cid) {
      ca_clear_channel(pv->cid);
      pv->cid = 0;
    }
    if (pv->getbuffer) {
      delete [] pv->getbuffer;
      pv->getbuffer = 0;
      pv->getbufsiz = 0;
    }
    if (pv->putbuffer) {
      delete [] pv->putbuffer;
      pv->putbuffer = 0;
      pv->putbufsiz = 0;
    }
    self->ob_type->tp_free(self);
  }

  static PyObject* capv_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
  {
    PyObject* self = type->tp_alloc(type, 0);
    capv* pv = reinterpret_cast<capv*>(self);
    if (!pv) {
      pyca_raise_pyexc("capv_new", "cannot allocate new PV");
    }
    pv->data = PyDict_New();
    if (!pv->data) {
      pyca_raise_pyexc("capv_new", "cannot allocate dictionary for new PV");
    }
    Py_INCREF(pv);
    return self;
  }

  // Register capv methods
  static PyMethodDef capv_methods[] = {
    {"create_channel", create_channel, METH_VARARGS},
    {"clear_channel", clear_channel, METH_VARARGS},
    {"subscribe_channel", subscribe_channel, METH_VARARGS},
    {"unsubscribe_channel", unsubscribe_channel, METH_VARARGS},
    {"get_data", get_data, METH_VARARGS},
    {"put_data", put_data, METH_VARARGS},
    {"host", host, METH_VARARGS},
    {"state", state, METH_VARARGS},
    {"count", count, METH_VARARGS},
    {"type", type, METH_VARARGS},
    {"rwaccess", rwaccess, METH_VARARGS},
    {NULL,  NULL},
  };

  // Register capv members
  static PyMemberDef capv_members[] = {
    {"name", T_OBJECT_EX, offsetof(capv, name), 0, "name"},
    {"data", T_OBJECT_EX, offsetof(capv, data), 0, "data"},
    {"processor", T_OBJECT_EX, offsetof(capv, processor), 0, "processor"},
    {"connect_cb", T_OBJECT_EX, offsetof(capv, connect_cb), 0, "connect_cb"},
    {"monitor_cb", T_OBJECT_EX, offsetof(capv, monitor_cb), 0, "monitor_cb"},
    {"getevt_cb", T_OBJECT_EX, offsetof(capv, getevt_cb), 0, "getevt_cb"},
    {"putevt_cb", T_OBJECT_EX, offsetof(capv, putevt_cb), 0, "putevt_cb"},
    {NULL}
  };

  static PyTypeObject capv_type = {
    PyObject_HEAD_INIT(0)
    0,
    "pyca.capv",
    sizeof(capv),
    0,
    capv_dealloc,                           /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    0,                                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    0,                                      /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    capv_methods,                           /* tp_methods */
    capv_members,                           /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    capv_init,                              /* tp_init */
    0,                                      /* tp_alloc */
    capv_new,                               /* tp_new */
  };

  // Module functions
  static PyObject* initialize(PyObject*, PyObject*) {
//     PyEval_InitThreads();
//     int result = ca_context_create(ca_enable_preemptive_callback);
//     if (result != ECA_NORMAL) {
//       pyca_raise_caexc("ca_context_create", result);
//     }
    printf("warning: no need to invoke initialize with this version of pyca\n");
    return ok();
  }

  static PyObject* finalize(PyObject*, PyObject*) {
//     ca_context_destroy();
    printf("warning: no need to invoke finalize with this version of pyca\n");
    return ok();
  }

  static PyObject* pend_io(PyObject*, PyObject* args) {
    PyObject* pytmo;
    if (!PyArg_ParseTuple(args, "O:pend_io", &pytmo) ||
	!PyFloat_Check(pytmo)) {
      pyca_raise_pyexc("pend_io", "error parsing arguments");
    }
    double timeout = PyFloat_AsDouble(pytmo);
    int result = ca_pend_io(timeout);
    if (result != ECA_NORMAL) {
      pyca_raise_caexc("ca_pend_io", result);
    }
    return ok();
  }

  static PyObject* flush_io(PyObject*, PyObject*) {
    int result = ca_flush_io();
    if (result != ECA_NORMAL) {
      pyca_raise_caexc("ca_flush_io", result);
    }
    return ok();
  }

  static PyObject* pend_event(PyObject*, PyObject* args) {
    PyObject* pytmo;
    if (!PyArg_ParseTuple(args, "O:pend_event", &pytmo) ||
	!PyFloat_Check(pytmo)) {
      pyca_raise_pyexc("pend_event", "error parsing arguments");
    }
    double timeout = PyFloat_AsDouble(pytmo);
    int result = ca_pend_event(timeout);
    if (result != ECA_TIMEOUT) {
      pyca_raise_caexc("ca_pend_event", result);
    }
    return ok();
  }

  // Register module methods
  static PyMethodDef pyca_methods[] = {
    {"initialize", initialize, METH_VARARGS},
    {"finalize", finalize, METH_VARARGS},
    {"pend_io", pend_io, METH_VARARGS},
    {"flush_io", flush_io, METH_VARARGS},
    {"pend_event", pend_event, METH_VARARGS},
    {NULL, NULL}
  };
  
  static const char* AlarmSeverityStrings[ALARM_NSEV] = {
    "NO_ALARM", "MINOR", "MAJOR", "INVALID"
  };

  static const char* AlarmConditionStrings[ALARM_NSTATUS] = {
    "NO_ALARM",
    "READ_ALARM",
    "WRITE_ALARM",
    "HIHI_ALARM",
    "HIGH_ALARM",
    "LOLO_ALARM",
    "LOW_ALARM",
    "STATE_ALARM",
    "COS_ALARM",
    "COMM_ALARM",
    "TIMEOUT_ALARM",
    "HWLIMIT_ALARM",
    "CALC_ALARM",
    "SCAN_ALARM",
    "LINK_ALARM",
    "SOFT_ALARM",
    "BAD_SUB_ALARM",
    "UDF_ALARM",
    "DISABLE_ALARM",
    "SIMM_ALARM",
    "READ_ACCESS_ALARM",
    "WRITE_ACCESS_ALARM",
  };

  static bool ca_context_initialized = 0;

  // Initialize python module
  void initpyca()
  {
    if (PyType_Ready(&capv_type) < 0) {
      return;
    }

    PyObject* module = Py_InitModule("pyca", pyca_methods);
    if (!module) {
      return;
    }

    // Export selected channel access constants
    PyObject* d = PyModule_GetDict(module);
    PyDict_SetItemString(d, "DBE_VALUE", PyLong_FromLong(DBE_VALUE));
    PyDict_SetItemString(d, "DBE_LOG",   PyLong_FromLong(DBE_LOG));
    PyDict_SetItemString(d, "DBE_ALARM", PyLong_FromLong(DBE_ALARM));
    PyObject* s = PyTuple_New(ALARM_NSEV);
    for (unsigned i=0; i<ALARM_NSEV; i++) {
      PyDict_SetItemString(d, AlarmSeverityStrings[i], PyLong_FromLong(i));
      PyTuple_SET_ITEM(s,i,PyString_FromString(AlarmSeverityStrings[i]));
    }
    PyDict_SetItemString(d, "severity", s);
    PyObject* a = PyTuple_New(ALARM_NSTATUS);
    for (unsigned i=0; i<ALARM_NSTATUS; i++) {
      PyDict_SetItemString(d, AlarmConditionStrings[i], PyLong_FromLong(i));
      PyTuple_SET_ITEM(a,i,PyString_FromString(AlarmConditionStrings[i]));
    }
    PyDict_SetItemString(d, "alarm", a);
    // secs between Jan 1st 1970 and Jan 1st 1990
    PyDict_SetItemString(d, "epoch", PyLong_FromLong(7305 * 86400));

    // Add capv type to this module
    Py_INCREF(&capv_type);
    PyModule_AddObject(module, "capv", (PyObject*)&capv_type);

    // Add custom exceptions to this module
    pyca_pyexc = PyErr_NewException("pyca.pyexc", NULL, NULL);
    Py_INCREF(pyca_pyexc);
    PyModule_AddObject(module, "pyexc", pyca_pyexc);
    pyca_caexc = PyErr_NewException("pyca.caexc", NULL, NULL);
    Py_INCREF(pyca_caexc);
    PyModule_AddObject(module, "caexc", pyca_caexc);

    PyEval_InitThreads();
    if (ca_context_initialized == false) {
      ca_context_initialized = true;
      int result = ca_context_create(ca_enable_preemptive_callback);
      if (result != ECA_NORMAL) {
        fprintf(stderr, 
                "*** initpyca: ca_context_create failed with status %d\n", 
              result);
      } else {
        // The following seems to cause a segfault at exit
        // Py_AtExit(ca_context_destroy);
      }
    }
  }
}
