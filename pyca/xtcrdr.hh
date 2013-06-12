// Structure to define an xtc reader for python
struct xtcrdr {
  PyObject_HEAD
  PyObject* name;      // XTC name
  PyObject* data;      // PV dictionary: "string" -> capv.
  PyObject* atoms;     // Tuple of IDs in the file.
  int   chunk;         // The current chunk number
  int   fd;            // The open chunk.
  XtcFileIterator *iter;
  Dgram *dg;
  int   count;         // How many data items in the file.
  char* buffer;        // buffer for received data
  unsigned bufsiz;     // received data buffer size

  epicsTimeStamp ts;   // Used when traversing to process.
  struct event_handler_args ev;
  dbr_time_short *dts;
  int tcnt;
};
