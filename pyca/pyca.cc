// #define PYCA_PLAYBACK  // Uncomment to allow playback of recordings!
#include <Python.h>
#include <stdio.h>
#include <structmember.h>

#include <cadef.h>
#include <alarm.h>

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

#include "pyca.hh"
#include "getfunctions.hh"
#include "putfunctions.hh"
#include "handlers.hh"

#ifdef PYCA_PLAYBACK
// Let's do our time arithmetic so as not to confuse fiducials!
#define ONE_SEC         (1000000000)
#define ONE_FID_SEC     (7629 << 17)
#define HALF_FID_SEC    (3814 << 17)
#define QUARTER_FID_SEC (1907 << 17)
#define TENTH_SEC       (762  << 17)
#define FID_MASK        (0x1ffff)

//
// Process connect callbacks for playback PVs.
//
static void change_connect_status(capv *pv, int status)
{
    if (pv->playback && pv->connect_cb && PyCallable_Check(pv->connect_cb)) {
        PyObject* pyisconn = PyBool_FromLong(status);
        PyObject* pytup = pyca_new_cbtuple(pyisconn);
        PyObject* res = PyObject_Call(pv->connect_cb, pytup, NULL);
        Py_XDECREF(res);
        Py_DECREF(pytup);
    }
}

//
// This calculates a timestamp that will put the nxtbuffer slightly in the future,
// without touching the fiducial bits.
//
static void set_next_time(capv *pv)
{
    epicsTimeGetCurrent(&pv->stamp);
    pv->stamp.nsec &= ~FID_MASK;
    pv->stamp.nsec |= pv->actual.nsec & FID_MASK;
    pv->stamp.nsec += QUARTER_FID_SEC;
    if (pv->stamp.nsec > ONE_FID_SEC) {
        pv->stamp.nsec -= ONE_FID_SEC;
        pv->stamp.secPastEpoch++;
    }

    if (pv->adjusttime) {
        struct dbr_time_double *pb = (struct dbr_time_double *)pv->nxtbuffer;
        pb->stamp = pv->stamp;     // If adjusting, set the timestamp!
    }
}

// 1 = OK, 0 = failed
static int move_to_next(capv *pv, int disc_ok)
{
    FILE *fp   = pv->playback;
    char *buf  = pv->nxtbuffer;
    int   size = pv->nxtbufsiz;
    int   dir  = pv->play_forward;
    int result;
    struct dbr_time_double *pb;

    // This is probably needlessly complicated because I don't want to call
    // fread while holding the GIL, but if the GIL is released, the PV may
    // go away.  Therefore, we take a reference to the pv to keep it around,
    // release the GIL and do our read, and then grab the GIL again and release
    // the PV reference.  So when this returns, the pv *could* be gone, so we
    // need to check this.  Whew!
    Py_INCREF(pv);
    Py_BEGIN_ALLOW_THREADS
        if (dir || !fseek(fp, -2 * size, SEEK_CUR)) { // Forward, or backup succeeds
            result = fread(buf, size, 1, fp);
        } else {
            // Backup failed.  Return failure, and reposition after the last record.
            result = 0;
            fseek(fp, size + sizeof(pv->dbftype) + sizeof(pv->dbrtype) + sizeof(pv->nelem), SEEK_SET);
        }
    Py_END_ALLOW_THREADS

    if (result) {
        // Got an event, setup a time to deliver it.
        // pv->stamp is the "current" time of pv->getbuffer.
        // We want to add in the delta between pv->getbuffer and pv->nxtbuffer.
        pb = (struct dbr_time_double *)pv->nxtbuffer;
        if (pb->stamp.secPastEpoch > pv->actual.secPastEpoch ||
            (pb->stamp.secPastEpoch == pv->actual.secPastEpoch &&
             pb->stamp.nsec >= pv->actual.nsec)) {
            // Moving forward!
            pv->stamp.secPastEpoch += pb->stamp.secPastEpoch - pv->actual.secPastEpoch;
            if (pb->stamp.nsec >= pv->actual.nsec) {
                // pb->stamp.nsec - pv->actual.nsec is positive
                pv->stamp.nsec += pb->stamp.nsec - pv->actual.nsec;
                if (pv->stamp.nsec > ONE_SEC) {
                    pv->stamp.nsec -= ONE_SEC;
                    pv->stamp.secPastEpoch++;
                }
            } else if (pv->stamp.nsec >= pv->actual.nsec - pb->stamp.nsec) {
                // pb->stamp.nsec - pv->actual.nsec is negative, but
                // pv->stamp.nsec + pb->stamp.nsec - pv->actual.nsec is positive
                pv->stamp.nsec -= pv->actual.nsec - pb->stamp.nsec;
            } else {
                // pb->stamp.nsec - pv->actual.nsec is negative, and so is
                // pv->stamp.nsec + pb->stamp.nsec - pv->actual.nsec.  We need
                // to borrow!
                pv->stamp.nsec += ONE_SEC + pb->stamp.nsec - pv->actual.nsec;
                pv->stamp.secPastEpoch--;
            }
        } else {
            // Moving backwards!
            pv->stamp.secPastEpoch += pv->actual.secPastEpoch - pb->stamp.secPastEpoch;
            if (pv->actual.nsec >= pb->stamp.nsec) {
                // pv->actual.nsec - pb->stamp.nsec is positive
                pv->stamp.nsec += pv->actual.nsec - pb->stamp.nsec;
                if (pv->stamp.nsec > ONE_SEC) {
                    pv->stamp.nsec -= ONE_SEC;
                    pv->stamp.secPastEpoch++;
                }
            } else if (pv->stamp.nsec >= pb->stamp.nsec - pv->actual.nsec) {
                // pv->actual.nsec - pb->stamp.nsec is negative, but
                // pv->stamp.nsec + pv->actual.nsec - pb->stamp.nsec is positive
                pv->stamp.nsec -= pb->stamp.nsec - pv->actual.nsec;
            } else {
                // pv->actual.nsec - pb->stamp.nsec is negative, and so is
                // pv->stamp.nsec + pv->actual.nsec - pb->stamp.nsec.  We need
                // to borrow!
                pv->stamp.nsec += ONE_SEC + pv->actual.nsec - pb->stamp.nsec;
                pv->stamp.secPastEpoch--;
            }
        }
        pv->actual = pb->stamp;       // Save, just in case
        if (pv->adjusttime) {
            pb->stamp = pv->stamp;    // If adjusting, set the timestamp!
        }
        pv->status = capv::PB_OK;
    } else if (disc_ok) {
        // End of file, signal a disconnect.
        pv->status = capv::PB_EOF;
        change_connect_status(pv, 0);
    }
    Py_DECREF(pv);
    return result;
}

#ifdef PYCA_PLAYBACK
static PyObject *process_get(capv *pv, char *buffer, double timeout)
{
    if (timeout < 0) {
        //
        // Like pyca_getevent_handler callback.
        //
        PyObject* pyexc = NULL;
        if (!_pyca_event_process(pv, buffer, pv->dbrtype, pv->nelem))
            pyexc = pyca_data_status_msg(ECA_BADTYPE, pv);
        if (pv->getevt_cb && PyCallable_Check(pv->getevt_cb)) {
            PyObject* pytup = pyca_new_cbtuple(pyexc);
            PyObject* res = PyObject_Call(pv->getevt_cb, pytup, NULL);
            Py_XDECREF(res);
            Py_DECREF(pytup);
        }
        Py_XDECREF(pyexc);
    } else {
        if (!_pyca_event_process(pv, buffer, pv->dbrtype, pv->nelem))
            pyca_raise_pyexc_pv("get_data", "un-handled type", pv);
    }
    return ok();
}
#endif

enum state { stop_req, stopped, run_req, running, dead };

class playback_thread: public epicsThreadRunable
{
public:
    epicsThread thread;
    //
    // When the main thread wants us to do something, it changes the state and signals
    // req.  When this thread gets around to doing it, it changes the state again and
    // signals ack.
    // 
    epicsEvent  req, ack;
    capv       *pv;
    enum state  state;         // Our current state.
    int         orphan;        // The PV had been deleted.
    int         monitor;       // We are actively monitoring.

    playback_thread(capv *_pv) : thread(*this, "PB thread", 
                                         epicsThreadGetStackSize(epicsThreadStackSmall), 50),
                                 pv(_pv), state(stop_req), orphan(0), monitor(0)
    {
        thread.start();
    }
    
    ~playback_thread()
    {
        thread.exitWait();
    }
    
    void run()
    {
        epicsTimeStamp ts;
        double diff;
        PyGILState_STATE gstate;

        thread.setOkToBlock(true);
        gstate = PyGILState_Ensure();

        for (;;) {
            while (state == stop_req) {
                state = stopped;
                ack.signal();
                Py_BEGIN_ALLOW_THREADS
                    req.wait();
                Py_END_ALLOW_THREADS
                if (state == run_req) {
                    state = running;
                    ack.signal();
                }
            }
            // Check if the PV has gone away and orphaned us!
            if (orphan)
                break;
            epicsTimeGetCurrent(&ts);
            if (ts.secPastEpoch > pv->stamp.secPastEpoch ||
                (ts.secPastEpoch == pv->stamp.secPastEpoch && ts.nsec >= pv->stamp.nsec)) {
                // An event in the file has passed!

                _pyca_swap_buffers(pv); // Move to the new event.

                if (monitor) {    // If we are monitoring, do a callback!
                    struct event_handler_args args;
                    args.usr = (void *) pv;
                    args.chid = 0;
                    args.type = pv->dbrtype;
                    args.count = pv->nelem;
                    args.dbr = pv->getbuffer;
                    args.status = ECA_NORMAL;
                    pyca_monitor_handler(args);
                }

                // Get the next data point!
                if (!move_to_next(pv, 1))
                    break;

                // pv could have been deleted during the I/O.
                if (orphan)
                    break;
            } else {
                // Sleep until it's time for the next event.
                diff = pv->stamp.secPastEpoch - ts.secPastEpoch;
                diff += ((double)pv->stamp.nsec - (double)ts.nsec)/1.0e9;
                Py_BEGIN_ALLOW_THREADS
                    req.wait(diff); // Event wait so we can wake up prematurely!
                Py_END_ALLOW_THREADS
            }
        }

        // OK, we're done!
        //
        // If the PV is gone (we're orphaned), just delete ourselves.
        // Otherwise, mark that we are done and the PV will delete us.
        if (orphan) {
            PyGILState_Release(gstate);
            delete this;
        } else {
            state = dead;
            PyGILState_Release(gstate);
        }
    }
};

//
// Close our file and kill our playback thread.
//
static void close_playback(capv *pv)
{
    if (pv->playback)
        fclose(pv->playback);
    pv->playback = NULL;
    if (pv->thid) {
        // We need to kill the thread.  If it's done, just delete it.
        // Otherwise, mark it as an orphan, and make sure it is running.
        if (pv->thid->state == dead)
            delete pv->thid;
        else {
            pv->thid->orphan = 1;
            pv->thid->state = run_req;
            pv->thid->req.signal();
        }
        pv->thid = NULL;
    }
}
#endif // PYCA_PLAYBACK

extern "C" {
    //
    // Python methods for channel access PV types
    //
    // Now that we have introduced the idea of PV playback, all of these
    // routines have the form:
    //
    //  static PyObject* ROUTINE(PyObject* self, PyObject* args)
    //  {
    //      capv* pv = reinterpret_cast<capv*>(self);
    //      // Parse arguments
    //  #ifdef PYCA_PLAYBACK
    //      if (pv->playback) {
    //          // Do file playback operations.
    //      }
    //  #endif PYCA_PLAYBACK
    //      // Do regular PV operations.
    //  }
    //
    static PyObject* create_channel(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            if (pv->thid) {
                pyca_raise_pyexc_pv("create_channel", "channel already created", pv);
            }
            pv->thid = new playback_thread(pv);
            Py_BEGIN_ALLOW_THREADS
                pv->thid->ack.wait();
            Py_END_ALLOW_THREADS
                change_connect_status(pv, 1);
            return ok();
        }
#endif // PYCA_PLAYBACK
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

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            close_playback(pv);
            return ok();
        }
#endif // PYCA_PLAYBACK
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
        PyObject* pycnt = NULL;

        if (!PyArg_ParseTuple(args, "OO|O:subscribe", &pymsk, &pyctrl, &pycnt) ||
            !PyLong_Check(pymsk) ||
            !PyBool_Check(pyctrl) ||
            (pycnt && pycnt != Py_None && !PyInt_Check(pycnt))) {
            pyca_raise_pyexc_pv("subscribe_channel", "error parsing arguments", pv);
        }

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            if (!pv->thid)
                pyca_raise_pyexc_pv("subscribe_channel", "channel is null", pv);
            if (pv->thid->state == dead)
                pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
            pv->thid->monitor = 1;
            if (pv->realtime && pv->thid->state == stopped) {
                set_next_time(pv);
                pv->thid->state = run_req;
                pv->thid->req.signal();
                Py_BEGIN_ALLOW_THREADS
                    pv->thid->ack.wait();
                Py_END_ALLOW_THREADS
                    }
            return ok();
        }
#endif // PYCA_PLAYBACK
        chid cid = pv->cid;
        if (!cid) {
            pyca_raise_pyexc_pv("subscribe_channel", "channel is null", pv);
        }
        int count = ca_element_count(cid);
        if (pycnt && pycnt != Py_None) {
            int limit = PyInt_AsLong(pycnt);
            if (limit < count)
                count = limit;
        }
        short type = ca_field_type(cid);
        if (count == 0 || type == TYPENOTCONN) {
            pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
        }
        short dbr_type = (Py_True == pyctrl) ?
            dbf_type_to_DBR_CTRL(type) : // Asks IOC to send status+time+limits+value
            dbf_type_to_DBR_TIME(type);  // Asks IOC to send status+time+value
        if (dbr_type_is_ENUM(dbr_type) && pv->string_enum)
            dbr_type = (Py_True == pyctrl) ? DBR_CTRL_STRING : DBR_TIME_STRING;

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

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            if (pv->thid)
                pv->thid->monitor = 0;
            return ok();
        }
#endif // PYCA_PLAYBACK
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
        PyObject* pycnt = NULL;

        if (!PyArg_ParseTuple(args, "OO|O:get", &pyctrl, &pytmo, &pycnt) ||
            !PyBool_Check(pyctrl) ||
            !PyFloat_Check(pytmo) ||
            (pycnt && pycnt != Py_None && !PyInt_Check(pycnt))) {
            pyca_raise_pyexc_pv("get_data", "error parsing arguments", pv);
        }

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            double timeout = PyFloat_AsDouble(pytmo);
            PyObject *result;
            if (!pv->thid)
                pyca_raise_pyexc_pv("get_data", "channel is null", pv);
            if (pv->thid->state == dead)
                pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
            if (pv->getbuffer != NULL)
                result = process_get(pv, pv->getbuffer, timeout);
            else
                result = process_get(pv, pv->nxtbuffer, timeout);
            if (pv->realtime && pv->thid->state == stopped) {
                // We haven't started the playback yet, so start it!
                pv->thid->state = run_req;
                pv->thid->req.signal();
                Py_BEGIN_ALLOW_THREADS
                    pv->thid->ack.wait();
                Py_END_ALLOW_THREADS
                    }
            return result;
        }
#endif // PYCA_PLAYBACK
        chid cid = pv->cid;
        if (!cid) {
            pyca_raise_pyexc_pv("get_data", "channel is null", pv);
        }
        int count = ca_element_count(cid);
        if (pycnt && pycnt != Py_None) {
            int limit = PyInt_AsLong(pycnt);
            if (limit < count)
                count = limit;
        }
        short type = ca_field_type(cid);
        if (count == 0 || type == TYPENOTCONN) {
            pyca_raise_caexc_pv("ca_field_type", ECA_DISCONNCHID, pv);
        }
        short dbr_type = (Py_True == pyctrl) ?
            dbf_type_to_DBR_CTRL(type) : // Asks IOC to send status+time+limits+value
            dbf_type_to_DBR_TIME(type);  // Asks IOC to send status+time+value
        if (dbr_type_is_ENUM(dbr_type) && pv->string_enum)
            dbr_type = (Py_True == pyctrl) ? DBR_CTRL_STRING : DBR_TIME_STRING;
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
            void* buffer = _pyca_adjust_buffer_size(pv, dbr_type, count, 0);
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
 
#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            // Can't write a playback channel!
            pyca_raise_pyexc_pv("put_data", "channel is doing playback", pv);
        }
#endif // PYCA_PLAYBACK
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

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            return PyString_FromString("localhost");
        }
#endif // PYCA_PLAYBACK
        if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
        return PyString_FromString(ca_host_name(pv->cid));
    }

    static PyObject* state(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            switch (pv->status) {
            case capv::PB_NOTCONN:
                return PyInt_FromLong(cs_never_conn);
            case capv::PB_OK:
                return PyInt_FromLong(cs_conn);
            case capv::PB_EOF:
                return PyInt_FromLong(cs_prev_conn);
            default:
                pyca_raise_pyexc_pv("state", "status is corrupt", pv);
            }
        }
#endif // PYCA_PLAYBACK
        if (!pv->cid) pyca_raise_pyexc_pv("state", "channel is null", pv);
        return PyInt_FromLong(ca_state(pv->cid));
    }

    static PyObject* count(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            return PyInt_FromLong(pv->nelem);
        }
#endif // PYCA_PLAYBACK
        if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
        return PyInt_FromLong(ca_element_count(pv->cid));
    }

    static PyObject* type(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            return PyString_FromString(dbf_type_to_text(pv->dbftype));
        }
#endif // PYCA_PLAYBACK
        if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
        return PyString_FromString(dbf_type_to_text(ca_field_type(pv->cid)));
    }

    static PyObject* rwaccess(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);

#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            return PyInt_FromLong(1); // Playback is always read-only!
        }
#endif // PYCA_PLAYBACK
        if (!pv->cid) pyca_raise_pyexc_pv("host", "channel is null", pv);
        int rw = ca_read_access(pv->cid) ? 1 : 0;
        rw |= ca_write_access(pv->cid) ? 2 : 0;
        return PyInt_FromLong(rw);
    }

    static PyObject* is_string_enum(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        return PyBool_FromLong(pv->string_enum);
    }

    static PyObject* set_string_enum(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        PyObject* pyval;

        if (!PyArg_ParseTuple(args, "O:set_string_enum", &pyval) ||
            !PyBool_Check(pyval)) {
            pyca_raise_pyexc_pv("set_string_enum", "error parsing arguments", pv);
        }
        pv->string_enum = (Py_True == pyval);
        return ok();
    }

#ifdef PYCA_PLAYBACK
    // This must be called before the channel is created.
    static PyObject* start_playback(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        PyObject *file;
        if (!PyArg_ParseTuple(args, "O:start_playback", &file) ||
            !PyString_Check(file)) {
            pyca_raise_pyexc_pv("start_playback", "cannot get filename", pv);
        }

        // If we are already playing back, we need to close everything down,
        // but restore our state for the new playback.  (If we were connected,
        // we want to be connected, and if we were monitoring, we want to be
        // monitoring.)
        if (pv->playback) {
            int have_thid;
            int monitor;
            if (pv->thid) {
                have_thid = 1;
                monitor = pv->thid->monitor;
            } else {
                have_thid = 0;
                monitor = 0;
            }
            close_playback(pv);
            if (have_thid) {
                pv->thid = new playback_thread(pv);
                pv->thid->monitor = monitor;
                Py_BEGIN_ALLOW_THREADS
                    pv->thid->ack.wait();
                Py_END_ALLOW_THREADS
                    }
        }
        if (pv->cid) {
            pyca_raise_pyexc_pv("start_playback", "channel already created", pv);
        }

        const char* filename = PyString_AsString(file);
        if (!filename || !filename[0]) { // Null string == turn off playback!
            pv->playback = NULL;
            return PyBool_FromLong(1);
        } else {
            pv->playback = fopen(filename, "r");
            if (!pv->playback)
                return PyBool_FromLong(0);
            // Read our header!
            if (!fread(&pv->dbftype, sizeof(pv->dbftype), 1, pv->playback) ||
                !fread(&pv->dbrtype, sizeof(pv->dbrtype), 1, pv->playback) ||
                !fread(&pv->nelem, sizeof(pv->nelem), 1, pv->playback)) {
                fclose(pv->playback);
                pv->playback = NULL;
                return PyBool_FromLong(0);
            }

            if (pv->getbuffer) {
                delete [] pv->getbuffer;
                pv->getbuffer = 0;
                pv->getbufsiz = 0;
            }
            _pyca_adjust_buffer_size(pv, pv->dbrtype, pv->nelem, 1);

            // Read the next data record.
            if (!pv->play_forward)        // Skip to the end if playing backwards!
                fseek(pv->playback, -(long)pv->nxtbufsiz, SEEK_END);
            if (!fread(pv->nxtbuffer, pv->nxtbufsiz, 1, pv->playback)) {
                fclose(pv->playback);
                pv->playback = NULL;
            } else
                pv->status = capv::PB_NOTCONN;
            pv->actual = ((struct dbr_time_double *)pv->nxtbuffer)->stamp; // Save, just in case
            if (pv->realtime && pv->thid && pv->thid->monitor) { // If we're already monitoring, start the thread!
                change_connect_status(pv, 1);
                set_next_time(pv);
                if (pv->thid->state == stopped) {
                    pv->thid->state = run_req;
                    pv->thid->req.signal();
                    Py_BEGIN_ALLOW_THREADS
                        pv->thid->ack.wait();
                    Py_END_ALLOW_THREADS
                        }
            }
            return PyBool_FromLong(pv->playback != NULL);
        }
    }

    static PyObject* adjust_time(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        int mode;
        if (!PyArg_ParseTuple(args, "i:adjust_time", &mode)) {
            pyca_raise_pyexc_pv("adjust_time", "cannot get mode", pv);
        }
        pv->adjusttime = mode ? 1 : 0;
        if (pv->playback) {
            struct dbr_time_double *pb = (struct dbr_time_double *)pv->nxtbuffer;
            pb->stamp = pv->adjusttime ? pv->stamp : pv->actual; // Set the correct time!
        }
        return ok();
    }

    static PyObject* realtime(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        int mode;
        if (!PyArg_ParseTuple(args, "i:realtime", &mode)) {
            pyca_raise_pyexc_pv("realtime", "cannot get mode", pv);
        }
        pv->realtime = mode ? 1 : 0;
        if (pv->playback) {
            if (pv->thid && pv->thid->state == (pv->realtime ? stopped : running)) {
                pv->thid->state = (pv->realtime ? run_req : stop_req);
                pv->thid->req.signal();
                Py_BEGIN_ALLOW_THREADS
                    pv->thid->ack.wait();
                Py_END_ALLOW_THREADS
                    }
        }
        return ok();
    }

    static PyObject* single_step(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);

        if (pv->playback) {
            if (pv->thid && pv->thid->state == stopped) {
                // Single step.
                set_next_time(pv);

                _pyca_swap_buffers(pv); // Move to the new event.

                if (pv->thid->monitor) {    // If we are monitoring, do a callback!
                    struct event_handler_args args;
                    args.usr = (void *) pv;
                    args.chid = 0;
                    args.type = pv->dbrtype;
                    args.count = pv->nelem;
                    args.dbr = pv->getbuffer;
                    args.status = ECA_NORMAL;
                    pyca_monitor_handler(args);
                }

                move_to_next(pv, 1);      // Get the next event.
            } else {
                // Error, needs to be stopped!
                pyca_raise_pyexc_pv("single_step", "playback must be stopped before single stepping", pv);
            }
            return ok();
        } else {
            pyca_raise_pyexc_pv("single_step", "not playing back, cannot single step", pv);
        }
    }

    static PyObject* play_forward(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        int dir;
        if (!PyArg_ParseTuple(args, "i:play_forward", &dir)) {
            pyca_raise_pyexc_pv("play_forward", "cannot get direction", pv);
        }
        pv->play_forward = dir;
        // MCB - This will play the next data point forward, and
        // then start moving back.  Is that enough?
        return ok();
    }

    static PyObject* jump_forward(PyObject* self, PyObject* args)
    {
        capv* pv = reinterpret_cast<capv*>(self);
        double delta;
        if (!PyArg_ParseTuple(args, "d:jump_forward", &delta)) {
            pyca_raise_pyexc_pv("jump_forward", "cannot get time delta", pv);
        }

        if (pv->playback) {
            if (pv->thid && pv->thid->state == stopped) {
                epicsTimeStamp ts;
                long nsecdelta;
                struct dbr_time_double *pb = (struct dbr_time_double *)pv->nxtbuffer;
                int dir = pv->play_forward;
                pv->play_forward = (delta > 0);
                ts.secPastEpoch = pv->actual.secPastEpoch + (int) delta;
                nsecdelta = (int)((delta - (int) delta) * 1e9);
                if (nsecdelta < 0 && pv->actual.nsec < -nsecdelta) {
                    // Borrow!
                    ts.secPastEpoch--;
                    ts.nsec = ONE_SEC + pv->actual.nsec + nsecdelta;
                } else
                    ts.nsec = pv->actual.nsec + nsecdelta;
                if (ts.nsec > ONE_SEC) {
                    // Carry!
                    ts.nsec -= ONE_SEC;
                    ts.secPastEpoch++;
                }
                while (move_to_next(pv, 0)) {
                    pb = (struct dbr_time_double *)pv->nxtbuffer;
                    if (delta < 0) {
                        if (ts.secPastEpoch > pv->actual.secPastEpoch ||
                            (ts.secPastEpoch == pv->actual.secPastEpoch &&
                             ts.nsec >= pv->actual.nsec))
                            break;
                    } else {
                        if (ts.secPastEpoch < pv->actual.secPastEpoch ||
                            (ts.secPastEpoch == pv->actual.secPastEpoch &&
                             ts.nsec <= pv->actual.nsec))
                            break;
                    }
                }
                set_next_time(pv);
                pv->play_forward = dir;
                return ok();
            } else {
                pyca_raise_pyexc_pv("jump_forward","playback must be stopped before jumping", pv);
            }
        } else {
            pyca_raise_pyexc_pv("jump_forward", "not playing back, cannot jump", pv);
        }
    }
#endif // PYCA_PLAYBACK

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
#ifdef PYCA_PLAYBACK
        pv->playback = NULL;
        pv->adjusttime = 0;
        pv->realtime = 1;
        pv->dbftype = 0;
        pv->dbrtype = 0;
        pv->nelem = 0;
        pv->nxtbuffer = 0;
        pv->nxtbufsiz = 0;
        pv->thid = NULL;
        pv->status = capv::PB_NOTCONN;
        pv->play_forward = 1;
#endif // PYCA_PLAYBACK
        return 0;
    }

    static void capv_dealloc(PyObject* self)
    {
        capv* pv = reinterpret_cast<capv*>(self);
#ifdef PYCA_PLAYBACK
        if (pv->playback) {
            close_playback(pv);
        }
#endif // PYCA_PLAYBACK
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
#ifdef PYCA_PLAYBACK
        if (pv->nxtbuffer) {
            delete [] pv->nxtbuffer;
            pv->nxtbuffer = 0;
            pv->nxtbufsiz = 0;
        }
#endif // PYCA_PLAYBACK
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
        {"set_string_enum", set_string_enum, METH_VARARGS},
        {"is_string_enum", is_string_enum, METH_VARARGS},
#ifdef PYCA_PLAYBACK
        {"start_playback", start_playback, METH_VARARGS},
        {"adjust_time", adjust_time, METH_VARARGS},
        {"realtime", realtime, METH_VARARGS},
        {"single_step", single_step, METH_VARARGS},
        {"play_forward", play_forward, METH_VARARGS},
        {"jump_forward", jump_forward, METH_VARARGS},
#endif // PYCA_PLAYBACK
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

    static ca_client_context *ca_context = 0;

    static PyObject* attach_context(PyObject* self, PyObject* args) {
        // only failure modes are if it's already attached or single threaded, 
        // so no need to raise an exception
        if (ca_current_context() == NULL) {
            int res = ca_attach_context(ca_context);
            if (res != ECA_NORMAL) {
                pyca_raise_pyexc("attach_context", "attach error");
            }
        }

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
        {"attach_context", attach_context, METH_VARARGS},
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
        if (!ca_context) {
            int result = ca_context_create(ca_enable_preemptive_callback);
            if (result != ECA_NORMAL) {
                fprintf(stderr, 
                        "*** initpyca: ca_context_create failed with status %d\n", 
                        result);
            } else {
	      ca_context = ca_current_context();
                // The following seems to cause a segfault at exit
                // Py_AtExit(ca_context_destroy);
            }
        }
    }
}
