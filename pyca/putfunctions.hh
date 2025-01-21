#include "p3compat.h"
// Channel access PUT template functions
static inline void _pyca_put(PyObject* pyvalue, dbr_string_t* buf)
{
  char *result = PyString_AsString(pyvalue);
  if (result)
      memcpy(buf, result, sizeof(dbr_string_t));
  else
      (*buf)[0] = 0;
}

static inline void _pyca_put(PyObject* pyvalue, dbr_enum_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

static inline void _pyca_put(PyObject* pyvalue, dbr_char_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

static inline void _pyca_put(PyObject* pyvalue, dbr_short_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

static inline void _pyca_put(PyObject* pyvalue, dbr_long_t* buf)
{
  *buf = PyLong_AsLong(pyvalue);
}

static inline void _pyca_put(PyObject* pyvalue, dbr_float_t* buf)
{
  *buf = PyFloat_AsDouble(pyvalue);
}

static inline void _pyca_put(PyObject* pyvalue, dbr_double_t* buf)
{
  *buf = PyFloat_AsDouble(pyvalue);
}

// If using correct numpy type, there is nothing special to do.
// Note we still risk corrupt data if we mismatch numpy data types, but this
// should never happen unless the python user is malicious.
template<class T> static inline
void _pyca_put_np(void* npdata, T* buf)
{
  memcpy(buf, npdata, sizeof(T));
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
  T* buffer = reinterpret_cast<T*>(pv->putbuffer);
  if (count == 1) {
      // if we only want to put the first element
      if (PyTuple_Check(pyvalue)) {
        PyObject* pyval = PyTuple_GetItem(pyvalue, 0);
        _pyca_put(pyval, buffer);
      } else if (PyArray_Check(pyvalue)) {
        // Convert to array
        PyArrayObject *arr = (PyArrayObject *)PyArray_FROM_O(pyvalue); 
        char* npdata = static_cast<char*>(PyArray_GETPTR1(arr, 0));
        if (PyArray_IsPythonScalar(pyvalue)) {
          PyObject* pyval = PyArray_GETITEM(arr, npdata);
          _pyca_put(pyval, buffer);
        } else {
          _pyca_put_np(npdata, buffer);
        }
      } else {
        _pyca_put(pyvalue, buffer);
      }
  } else {
//     Py_ssize_t len = PyTuple_Size(pyvalue);
//     if (len != count) {
//       pyca_raise_pyexc_pv("put_data", "value doesn't match pv length", pv);
//     }
    if (PyTuple_Check(pyvalue)) {
      for (long i=0; i<count; i++) {
        PyObject* pyval = PyTuple_GetItem(pyvalue, i);
        _pyca_put(pyval, buffer+i);
      }
    } else if (PyArray_Check(pyvalue)) {
      PyArrayObject *arr2 = (PyArrayObject *)PyArray_FROM_O(pyvalue); 
      bool py_type = PyArray_IsPythonScalar(arr2);
      for (long i=0; i<count; i++) {
        char* npdata = static_cast<char*>(PyArray_GETPTR1(arr2, i));
        if (py_type) {
          PyObject* pyval = PyArray_GETITEM(arr2, npdata);
          _pyca_put(pyval, buffer+i);
        } else {
          _pyca_put_np(npdata, buffer+i);
        }
      }
    }
  }
  if (PyErr_ExceptionMatches(PyExc_TypeError)) {
      PyErr_Clear();
      *buf = NULL;
  } else
      *buf = buffer;
}

static const void* _pyca_put_buffer(capv* pv,
                                    PyObject* pyvalue,
                                    short &dbr_type, // We may change DBF_ENUM to DBF_STRING
                                    long count)
{
  const void* buffer;
  switch (dbr_type) {
  case DBR_ENUM:
    {
      if (PyString_Check(pyvalue)) {
	dbr_type = DBR_STRING;
	// no break: pass into string block, below
	// Note: We don't check if the caller passed
	//       an integer cast as a string... Although
	//       this seems to be handled correctly.
      } else {
	dbr_enum_t* buf;
	_pyca_put_value(pv, pyvalue, &buf, count);
	buffer = buf;
	break;
      }
    }
  case DBR_STRING:
    {
      dbr_string_t* buf;
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
