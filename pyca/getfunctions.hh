// Channel access GET template functions
template<class T> static inline PyObject* _pyca_get(T value);

template<> static inline PyObject* _pyca_get(const dbr_string_t value)
{
  return PyString_FromString(value);
}

template<> static inline PyObject* _pyca_get(dbr_enum_t value)
{
  return PyLong_FromLong(value);
}

template<> static inline PyObject* _pyca_get(dbr_char_t value)
{
  return PyLong_FromLong(value);
}

template<> static inline PyObject* _pyca_get(dbr_short_t value)
{
  return PyLong_FromLong(value);
}

template<> static inline PyObject* _pyca_get(dbr_ulong_t value)
{
  return PyLong_FromLong(value);
}

template<> static inline PyObject* _pyca_get(dbr_long_t value)
{
  return PyLong_FromLong(value);
}

template<> static inline PyObject* _pyca_get(dbr_float_t value)
{
  return PyFloat_FromDouble(value);
}

template<> static inline PyObject* _pyca_get(dbr_double_t value)
{
  return PyFloat_FromDouble(value);
}

// Copy the value of a channel access object into a python
// object. Note that for array objects (count > 1), this function will
// check for the presence of a user process method ('processor') and,
// if present, that method is invoked. This mechanism may avoid
// unncessary data copies and it may be useful for large arrays.

typedef void (*processptr)(const void* cadata, long count, size_t size, void* descr);

template<class T> static inline 
PyObject* _pyca_get_value(capv* pv, const T* dbrv, long count)
{
  if (count == 1) {
    return _pyca_get(dbrv->value);
  } else {
    if (!pv->processor) {
      PyObject* pytup = PyTuple_New(count);
      for (long i=0; i<count; i++) {
        // Following function steals reference, no DECREF needed for each item
        PyTuple_SetItem(pytup, i, _pyca_get(*(&(dbrv->value)+i)));
      }
      return pytup;
    } else {
      processptr process = (processptr)PyCObject_AsVoidPtr(pv->processor);
      void* descr = PyCObject_GetDesc(pv->processor);
      process(&(dbrv->value), count, sizeof(dbrv->value), descr);
      return NULL;
    }
  }
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

static const void* _pyca_event_process(capv* pv, 
                                       const void* buffer, 
                                       short dbr_type, 
                                       long count)
{
  const db_access_val* dbr = reinterpret_cast<const db_access_val*>(buffer);
  switch (dbr_type) {
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
                                      long count)
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
