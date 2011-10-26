// Utility function to pack eventual arguments for callback
PyObject* pyca_new_cbtuple(PyObject* arg)
{
  PyObject* pytup;
  if (arg) {
    pytup = PyTuple_New(1);
    PyTuple_SET_ITEM(pytup, 0, arg);
  } else {
    pytup = PyTuple_New(0);
  }
  return pytup;
}

// Callbacks invoked by EPICS channel access for:
// - connection events
static void pyca_connection_handler(struct connection_handler_args args)
{
  capv* pv = reinterpret_cast<capv*>(ca_puser(args.chid));
  long isconn = (args.op == CA_OP_CONN_UP) ? 1 : 0;
  PyGILState_STATE gstate = PyGILState_Ensure();
  if (pv->connect_cb && PyCallable_Check(pv->connect_cb)) {
    PyObject* pyisconn = PyBool_FromLong(isconn);
    PyObject* pytup = pyca_new_cbtuple(pyisconn);
    PyObject* res = PyObject_Call(pv->connect_cb, pytup, NULL);
    Py_XDECREF(res);
    Py_DECREF(pytup);
  }
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
  if (pv->monitor_cb && PyCallable_Check(pv->monitor_cb)) {
    PyObject* pytup = pyca_new_cbtuple(pyexc);
    PyObject* res = PyObject_Call(pv->monitor_cb, pytup, NULL);
    Py_XDECREF(res);
    Py_DECREF(pytup);
  }
  Py_XDECREF(pyexc);
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
  if (pv->getevt_cb && PyCallable_Check(pv->getevt_cb)) {
    PyObject* pytup = pyca_new_cbtuple(pyexc);
    PyObject* res = PyObject_Call(pv->getevt_cb, pytup, NULL);
    Py_XDECREF(res);
    Py_DECREF(pytup);
  }
  Py_XDECREF(pyexc);
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
  if (pv->putevt_cb && PyCallable_Check(pv->putevt_cb)) {
    PyObject* pytup = pyca_new_cbtuple(pyexc);
    PyObject* res = PyObject_Call(pv->putevt_cb, pytup, NULL);
    Py_XDECREF(res);
    Py_DECREF(pytup);
  }
  Py_XDECREF(pyexc);
  PyGILState_Release(gstate);
}

