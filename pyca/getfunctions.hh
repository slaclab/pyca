#include "p3compat.h"
// #include "npy_2_compat.h"
// Channel access GET template functions
static inline PyObject* _pyca_get(const dbr_string_t value)
{
  return PyString_FromString(value);
}

static inline PyObject* _pyca_get(const dbr_enum_t value)
{
  return PyLong_FromLong(value);
}

static inline PyObject* _pyca_get(const dbr_char_t value)
{
  return PyLong_FromLong(value);
}

static inline PyObject* _pyca_get(const dbr_short_t value)
{
  return PyLong_FromLong(value);
}

static inline PyObject* _pyca_get(const dbr_ulong_t value)
{
  return PyLong_FromLong(value);
}

static inline PyObject* _pyca_get(const dbr_long_t value)
{
  return PyLong_FromLong(value);
}

static inline PyObject* _pyca_get(const dbr_float_t value)
{
  return PyFloat_FromDouble(value);
}

static inline PyObject* _pyca_get(const dbr_double_t value)
{
  return PyFloat_FromDouble(value);
}

// Copy the value of a channel access object into a python
// object. Note that for array objects (count > 1), this function will
// check for the presence of a user process method ('processor') and,
// if present, that method is invoked. This mechanism may avoid
// unncessary data copies and it may be useful for large arrays.

typedef void (*processptr)(const void* cadata, long count, size_t size, void* descr);

// EPICS      Description                Numpy
// DBR_STRING 40 character string`       NPY_STRING
// DBR_ENUM   16-bit unsigned integer    NPY_UINT16
// DBR_CHAR   8-bit character            NPY_UINT8
// DBR_SHORT  16-bit integer             NPY_INT16
// DBR_LONG   32-bit signed integer      NPY_INT32
// DBR_FLOAT  32-bit IEEE floating point NPY_FLOAT32
// DBR_DOUBLE 64-bit IEEE floating pint  NPY_FLOAT64
int _numpy_array_type(const dbr_string_t*)
{
  return NPY_STRING;
}

int _numpy_array_type(const dbr_enum_t*)
{
  return NPY_UINT16;
}

int _numpy_array_type(const dbr_char_t*)
{
  return NPY_UINT8;
}

int _numpy_array_type(const dbr_short_t*)
{
  return NPY_INT16;
}

int _numpy_array_type(const dbr_ulong_t*)
{
  return NPY_UINT32;
}

int _numpy_array_type(const dbr_long_t*)
{
  return NPY_INT32;
}

int _numpy_array_type(const dbr_float_t*)
{
  return NPY_FLOAT32;
}

int _numpy_array_type(const dbr_double_t*)
{
  return NPY_FLOAT64;
}

template<class T> static inline
PyObject* _pyca_get_value(capv* pv, const T* dbrv, long count)
{
  if (count == 1) {
    return _pyca_get(dbrv->value);
  } else {
    if (!pv->processor) {
      if (PyObject_IsTrue(pv->use_numpy)) {
        npy_intp dims[1] = {count};
        int typenum = _numpy_array_type(&(dbrv->value));
        PyObject* nparray = PyArray_EMPTY(1, dims, typenum, 0);
        PyArrayObject *arr = (PyArrayObject *)PyArray_FROM_O(nparray); 
        memcpy(PyArray_DATA(arr), &(dbrv->value), count*sizeof(dbrv->value));
        return nparray;
      } else {
        PyObject* pytup = PyTuple_New(count);
        for (long i=0; i<count; i++) {
          // Following function steals reference, no DECREF needed for each item
          PyTuple_SetItem(pytup, i, _pyca_get(*(&(dbrv->value)+i)));
        }
        return pytup;
      }
    } else {
#ifdef IS_PY3K
      const char* name = PyCapsule_GetName(pv->processor);
      processptr process = (processptr)PyCapsule_GetPointer(pv->processor, name);
      void* descr = PyCapsule_GetContext(pv->processor);
#else
      processptr process = (processptr)PyCObject_AsVoidPtr(pv->processor);
      void* descr = PyCObject_GetDesc(pv->processor);
#endif
      process(&(dbrv->value), count, sizeof(dbrv->value), descr);
      return NULL;
    }
  }
}

// Copy channel access status objects into python
template<class T> static inline
void _pyca_get_sts(capv* pv, const T* dbrv, long count)
{
  PyObject* pydata = pv->data;
  _pyca_setitem(pydata, "status",      _pyca_get(dbrv->status));
  _pyca_setitem(pydata, "severity",    _pyca_get(dbrv->severity));
  _pyca_setitem(pydata, "value",       _pyca_get_value(pv, dbrv, count));
}

// Copy channel access time objects into python
template<class T> static inline
void _pyca_get_time(capv* pv, const T* dbrv, long count)
{
  PyObject* pydata = pv->data;
  _pyca_setitem(pydata, "status",      _pyca_get(dbrv->status));
  _pyca_setitem(pydata, "severity",    _pyca_get(dbrv->severity));
  _pyca_setitem(pydata, "secs",        _pyca_get(dbrv->stamp.secPastEpoch));
  _pyca_setitem(pydata, "nsec",        _pyca_get(dbrv->stamp.nsec));
  _pyca_setitem(pydata, "value",       _pyca_get_value(pv, dbrv, count));

}

// Copy channel access control objects into python
template<class T> static inline
void _pyca_get_ctrl_long(capv* pv, const T* dbrv, long count)
{
  PyObject* pydata = pv->data;
  _pyca_setitem(pydata, "status",      _pyca_get(dbrv->status));
  _pyca_setitem(pydata, "severity",    _pyca_get(dbrv->severity));
  _pyca_setitem(pydata, "units",       _pyca_get(dbrv->units));
  _pyca_setitem(pydata, "display_llim",_pyca_get(dbrv->lower_disp_limit));
  _pyca_setitem(pydata, "display_hlim",_pyca_get(dbrv->upper_disp_limit));
  _pyca_setitem(pydata, "warn_llim",   _pyca_get(dbrv->lower_warning_limit));
  _pyca_setitem(pydata, "warn_hlim",   _pyca_get(dbrv->upper_warning_limit));
  _pyca_setitem(pydata, "alarm_llim",  _pyca_get(dbrv->lower_alarm_limit));
  _pyca_setitem(pydata, "alarm_hlim",  _pyca_get(dbrv->upper_alarm_limit));
  _pyca_setitem(pydata, "ctrl_llim",   _pyca_get(dbrv->lower_ctrl_limit));
  _pyca_setitem(pydata, "ctrl_hlim",   _pyca_get(dbrv->upper_ctrl_limit));
  _pyca_setitem(pydata, "value",       _pyca_get_value(pv, dbrv, count));
}

template<class T> static inline
void _pyca_get_ctrl_enum(capv* pv, const T* dbrv, long count)
{
  PyObject* pydata = pv->data;
  _pyca_setitem(pydata, "status",      _pyca_get(dbrv->status));
  _pyca_setitem(pydata, "severity",    _pyca_get(dbrv->severity));
  _pyca_setitem(pydata, "no_str",      _pyca_get(dbrv->no_str));
  _pyca_setitem(pydata, "value",       _pyca_get_value(pv, dbrv, count));
}

template<class T> static inline
void _pyca_get_ctrl_double(capv* pv, const T* dbrv, long count)
{
  PyObject* pydata = pv->data;
  _pyca_setitem(pydata, "status",      _pyca_get(dbrv->status));
  _pyca_setitem(pydata, "severity",    _pyca_get(dbrv->severity));
  _pyca_setitem(pydata, "precision",   _pyca_get(dbrv->precision));
  _pyca_setitem(pydata, "units",       _pyca_get(dbrv->units));
  _pyca_setitem(pydata, "display_llim",_pyca_get(dbrv->lower_disp_limit));
  _pyca_setitem(pydata, "display_hlim",_pyca_get(dbrv->upper_disp_limit));
  _pyca_setitem(pydata, "warn_llim",   _pyca_get(dbrv->lower_warning_limit));
  _pyca_setitem(pydata, "warn_hlim",   _pyca_get(dbrv->upper_warning_limit));
  _pyca_setitem(pydata, "alarm_llim",  _pyca_get(dbrv->lower_alarm_limit));
  _pyca_setitem(pydata, "alarm_hlim",  _pyca_get(dbrv->upper_alarm_limit));
  _pyca_setitem(pydata, "ctrl_llim",   _pyca_get(dbrv->lower_ctrl_limit));
  _pyca_setitem(pydata, "ctrl_hlim",   _pyca_get(dbrv->upper_ctrl_limit));
  _pyca_setitem(pydata, "value",       _pyca_get_value(pv, dbrv, count));
}

static inline
void _pyca_get_gr_enum(capv* pv, const struct dbr_gr_enum* dbrv, long count)
{
  PyObject* pydata = pv->data;
  PyObject* enstrs = PyTuple_New(dbrv->no_str);
  for (int i=0; i < dbrv->no_str; i++) {
    // TODO:  This is no bueno.  We need a new accessor above for
    //        char arrays...
    PyTuple_SET_ITEM(enstrs, i, _pyca_get(dbrv->strs[i]));
  }
  _pyca_setitem(pydata, "enum_set", enstrs);
}

static const void* _pyca_event_process(capv* pv,
                                       const void* buffer,
                                       short dbr_type,
                                       long count)
{
  const db_access_val* dbr = reinterpret_cast<const db_access_val*>(buffer);
  switch (dbr_type) {
  case DBR_GR_ENUM:
    _pyca_get_gr_enum(pv, &dbr->genmval, count);
    break;
  case DBR_TIME_STRING:
    _pyca_get_time(pv, &dbr->tstrval, count);
    break;
  case DBR_TIME_ENUM:
    _pyca_get_time(pv, &dbr->tenmval, count);
    break;
  case DBR_TIME_CHAR:
    _pyca_get_time(pv, &dbr->tchrval, count);
    break;
  case DBR_TIME_SHORT:
    _pyca_get_time(pv, &dbr->tshrtval, count);
    break;
  case DBR_TIME_LONG:
    _pyca_get_time(pv, &dbr->tlngval, count);
    break;
  case DBR_TIME_FLOAT:
    _pyca_get_time(pv, &dbr->tfltval, count);
    break;
  case DBR_TIME_DOUBLE:
    _pyca_get_time(pv, &dbr->tdblval, count);
    break;
  case DBR_CTRL_STRING:
    _pyca_get_sts(pv, &dbr->sstrval, count);
    break;
  case DBR_CTRL_ENUM:
    _pyca_get_ctrl_enum(pv, &dbr->cenmval, count);
    break;
  case DBR_CTRL_CHAR:
    _pyca_get_ctrl_long(pv, &dbr->cchrval, count);
    break;
  case DBR_CTRL_SHORT:
    _pyca_get_ctrl_long(pv, &dbr->cshrtval, count);
    break;
  case DBR_CTRL_LONG:
    _pyca_get_ctrl_long(pv, &dbr->clngval, count);
    break;
  case DBR_CTRL_FLOAT:
    _pyca_get_ctrl_double(pv, &dbr->cfltval, count);
    break;
  case DBR_CTRL_DOUBLE:
    _pyca_get_ctrl_double(pv, &dbr->cdblval, count);
    break;
  default:
    return NULL;
  }
  return buffer;
}

static void* _pyca_adjust_buffer_size(capv* pv,
                                      short dbr_type,
                                      long count,
                                      int nxtbuf)
{
  unsigned size = 0;
  switch (dbr_type) {
  case DBR_TIME_STRING:
    size = sizeof(dbr_time_string) + sizeof(dbr_string_t)*(count-1);
    break;
  case DBR_TIME_ENUM:
    size = sizeof(dbr_time_enum) + sizeof(dbr_enum_t)*(count-1);
    break;
  case DBR_TIME_CHAR:
    size = sizeof(dbr_time_char) + sizeof(dbr_char_t)*(count-1);
    break;
  case DBR_TIME_SHORT:
    size = sizeof(dbr_time_short) + sizeof(dbr_short_t)*(count-1);
    break;
  case DBR_TIME_LONG:
    size = sizeof(dbr_time_long) + sizeof(dbr_long_t)*(count-1);
    break;
  case DBR_TIME_FLOAT:
    size = sizeof(dbr_time_float) + sizeof(dbr_float_t)*(count-1);
    break;
  case DBR_TIME_DOUBLE:
    size = sizeof(dbr_time_double) + sizeof(dbr_double_t)*(count-1);
    break;
  case DBR_CTRL_STRING:
    size = sizeof(dbr_sts_string) + sizeof(dbr_string_t)*(count-1);
    break;
  case DBR_CTRL_ENUM:
    size = sizeof(dbr_ctrl_enum) + sizeof(dbr_enum_t)*(count-1);
    break;
  case DBR_CTRL_CHAR:
    size = sizeof(dbr_ctrl_char) + sizeof(dbr_char_t)*(count-1);
    break;
  case DBR_CTRL_SHORT:
    size = sizeof(dbr_ctrl_short) + sizeof(dbr_short_t)*(count-1);
    break;
  case DBR_CTRL_LONG:
    size = sizeof(dbr_ctrl_long) + sizeof(dbr_long_t)*(count-1);
    break;
  case DBR_CTRL_FLOAT:
    size = sizeof(dbr_ctrl_float) + sizeof(dbr_float_t)*(count-1);
    break;
  case DBR_CTRL_DOUBLE:
    size = sizeof(dbr_ctrl_double) + sizeof(dbr_double_t)*(count-1);
    break;
  default:
    return NULL;
  }
  if (size != pv->getbufsiz) {
    delete [] pv->getbuffer;
    pv->getbuffer = new char[size];
    pv->getbufsiz = size;
  }
  return pv->getbuffer;
}
