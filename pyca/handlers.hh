// Callbacks invoked by EPICS channel access for:
// - connection events
static void pyca_connection_handler(struct connection_handler_args args)
{
  capv* pv = reinterpret_cast<capv*>(ca_puser(args.chid));
  long isconn = (args.op == CA_OP_CONN_UP) ? 1 : 0;
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject* pymethod = PyString_FromString("connection_handler");
  PyObject* pyisconn = PyBool_FromLong(isconn);
  PyObject_CallMethodObjArgs((PyObject*) pv, pymethod, pyisconn, NULL);
  Py_DECREF(pymethod);
  Py_DECREF(pyisconn);
  PyGILState_Release(gstate);
}

// - monitor data events
static void pyca_monitor_handler(struct event_handler_args args)
{
  capv* pv = reinterpret_cast<capv*>(args.usr);
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject* pyexc = NULL;
  if (args.status == ECA_NORMAL) {
    if (!_pyca_event_process(pv, args.dbr, args.type, args.count)) {
      pyexc = pyca_data_status_msg(ECA_BADTYPE, pv);
    }
  } else {
    pyexc = pyca_data_status_msg(args.status, pv);
  }
  PyObject* pymethod = PyString_FromString("monitor_handler");
  PyObject_CallMethodObjArgs((PyObject*) pv, pymethod, pyexc, NULL);
  Py_DECREF(pymethod);
  PyGILState_Release(gstate);
}

// - get data events
static void pyca_getevent_handler(struct event_handler_args args)
{
  capv* pv = reinterpret_cast<capv*>(args.usr);
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject* pyexc = NULL;
  if (args.status == ECA_NORMAL) {
    if (!_pyca_event_process(pv, args.dbr, args.type, args.count)) {
      pyexc = pyca_data_status_msg(ECA_BADTYPE, pv);
    }
  } else {
    pyexc = pyca_data_status_msg(args.status, pv);
  }
  PyObject* pymethod = PyString_FromString("getevent_handler");
  PyObject_CallMethodObjArgs((PyObject*) pv, pymethod, pyexc, NULL);
  Py_DECREF(pymethod);
  PyGILState_Release(gstate);
}

// - put data events
static void pyca_putevent_handler(struct event_handler_args args)
{
  capv* pv = reinterpret_cast<capv*>(args.usr);
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject* pyexc = NULL;
  if (args.status != ECA_NORMAL) {
    pyexc = pyca_data_status_msg(args.status, pv);
  }
  PyObject* pymethod = PyString_FromString("putevent_handler");
  PyObject_CallMethodObjArgs((PyObject*) pv, pymethod, pyexc, NULL);
  Py_DECREF(pymethod);
  PyGILState_Release(gstate);
}

