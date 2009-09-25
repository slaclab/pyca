// Channel access PUT template functions
template<class T> static inline void _pyca_put(PyObject* pyvalue, T buf);

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_string_t* buf)
{
  memcpy(buf, PyString_AsString(pyvalue), sizeof(dbr_string_t));
}

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_enum_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_char_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_short_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_long_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_float_t* buf)
{
  *buf = PyFloat_AsDouble(pyvalue);
}

template<> static inline void _pyca_put(PyObject* pyvalue, dbr_double_t* buf)
{
  *buf = PyFloat_AsDouble(pyvalue);
}

// Copy python objects into channel access void* buffer
template<class T> static inline 
void _pyca_put_value(capv* pv, PyObject* pyvalue, T** buf, long count)
{
  unsigned size = count*sizeof(T);
  if (size != pv->putbufsiz) {
    delete [] pv->putbuffer;
    pv->putbuffer = new char[size];
    pv->putbufsiz = size;
  }
  void* buffer = pv->putbuffer;
  *buf = reinterpret_cast<T*>(buffer);
  if (count == 1) {
    _pyca_put(pyvalue, *buf);
  } else {
    for (long i=0; i<count; i++) {
      _pyca_put(PyTuple_GetItem(pyvalue, i), buf[i]);
    }
  }
}

static const void* _pyca_put_buffer(capv* pv, 
                                    PyObject* pyvalue, 
                                    short dbr_type, 
                                    long count)
{
  const void* buffer;
  switch (dbr_type) {
  case DBR_STRING:
    {
      dbr_string_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  case DBR_ENUM:
    {
      dbr_enum_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  case DBR_CHAR:
    {
      dbr_char_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  case DBR_SHORT:
    {
      dbr_short_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  case DBR_LONG:
    {
      dbr_long_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  case DBR_FLOAT:
    {
      dbr_float_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  case DBR_DOUBLE:
    {
      dbr_double_t* buf;
      _pyca_put_value(pv, pyvalue, &buf, count);
      buffer = buf;
    }
    break;
  default:
    return NULL;
  }
  return buffer;
}
