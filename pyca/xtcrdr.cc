#include <Python.h>
#include <stdio.h>
#include <structmember.h>

#include <cadef.h>
#include <alarm.h>

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include"pdsdata/xtc/DetInfo.hh"
#include"pdsdata/xtc/Xtc.hh"
#include"pdsdata/xtc/TypeId.hh"
#include"pdsdata/xtc/TimeStamp.hh"
#include"pdsdata/xtc/Dgram.hh"
#include"pdsdata/xtc/XtcFileIterator.hh"
#include"pdsdata/epics/EpicsPvData.hh"
#include"pdsdata/camera/FrameV1.hh"

/*
 * Sigh.  We want to include the DAQ code directly, but Sequence.cc and 
 * TimeStamp.cc don't play nice together.
 */
#include"pdsdata/xtc/src/Sequence.cc"

using namespace std;
using namespace Pds;

#include "pyca.hh"
#include "xtcrdr.hh"

extern "C" {
    static PyObject* pyca = NULL;
    static PyObject* capv_type = NULL;
    static void (*pyca_getevent_handler)(struct event_handler_args args) = NULL;
    static void (*pyca_monitor_handler)(struct event_handler_args args) = NULL;

    static void chunk_open(xtcrdr *rdr, const char *name)
    {
        char buf[1024];
        if (rdr->fd >= 0)
            close(rdr->fd);
        sprintf(buf, "%s-c%02d.xtc", name, ++rdr->chunk);
        rdr->fd = open(buf, O_RDONLY | O_LARGEFILE, 0644);
    }

    static long long next_dg(xtcrdr *rdr)
    {
        long long off = lseek64(rdr->fd, 0, SEEK_CUR);
        while ((rdr->dg = rdr->iter->next()) == NULL) {
            delete rdr->iter;
            rdr->iter = NULL;
            rdr->dg = NULL;
            const char* name = PyString_AsString(rdr->name);
            chunk_open(rdr, name);
            if (rdr->fd == -1)
                return -1;
            off = 0;
            rdr->iter = new XtcFileIterator(rdr->fd, 0x40000000);
        }
        return off;
    }

    static PyObject* xtcrdr_next(PyObject* self, PyObject* args)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        long long off;
        if (rdr->fd < 0)
            return ok();
        do {
            off = next_dg(rdr);
        } while (rdr->dg && rdr->dg->seq.service() != TransitionId::L1Accept);
        if (!rdr->dg)
            return ok();
        PyObject* s = PyTuple_New(2);
        PyTuple_SET_ITEM(s, 0, PyLong_FromLong(rdr->chunk));
        PyTuple_SET_ITEM(s, 1, PyLong_FromLong(off));
        return s;
    }

    static PyObject* xtcrdr_open(PyObject* self, PyObject* args)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        PyObject *pyname;
        if (!PyArg_ParseTuple(args, "O:open", &pyname) ||
            !PyString_Check(pyname)) {
            pyca_raise_pyexc_pv("open", "cannot get XTC name", rdr);
        }
        Py_XDECREF(rdr->name);
        rdr->name = pyname;
        Py_INCREF(rdr->name);
        rdr->chunk = -1;
        const char* name = PyString_AsString(rdr->name);
        chunk_open(rdr, name);
        if (rdr->fd < 0) {
            pyca_raise_pyexc_pv("open", "cannot open XTC file", rdr);
        }
        rdr->iter = new XtcFileIterator(rdr->fd, 0x40000000);
        rdr->dg = rdr->iter->next();
        // This better be a Configuration!
        if (rdr->dg->seq.service() != TransitionId::Configure) {
            pyca_raise_pyexc_pv("open", "XTC file didn't start with Configure", rdr);
        }
        Py_XDECREF(rdr->atoms);
        rdr->count = 0;
        Xtc *xtc = (Xtc *)(rdr->dg->xtc.payload());
        int size = xtc->sizeofPayload();
        xtc = (Xtc *)(xtc->payload());
        while (size > 0) {
            if (xtc->contains.id() == TypeId::Id_Epics ||
                xtc->contains.id() == TypeId::Id_FrameFexConfig) {
                rdr->count++;
            }
            size -= xtc->extent;
            xtc = xtc->next();
        }
        rdr->atoms = PyTuple_New(rdr->count);

        xtc = (Xtc *)(rdr->dg->xtc.payload());
        size = xtc->sizeofPayload();
        xtc = (Xtc *)(xtc->payload());
        int i = 0;
        while (size > 0) {
            switch (xtc->contains.id()) {
            case TypeId::Id_Epics: {
                EpicsPvCtrlHeader *pvc = (EpicsPvCtrlHeader *)xtc->payload();
                PyTuple_SET_ITEM(rdr->atoms,i,
                                 PyString_FromString(pvc->sPvName));
                i++;
                break;
            }
            case TypeId::Id_FrameFexConfig: {
                PyTuple_SET_ITEM(rdr->atoms,i,
                                 PyString_FromString(DetInfo::name(*(DetInfo *)&xtc->src)));
                i++;
                break;
            }
            default:
                break;
            }
            size -= xtc->extent;
            xtc = xtc->next();
        }
        
        return xtcrdr_next((PyObject *)rdr, NULL);
    }

    static PyObject* xtcrdr_unassociate(PyObject* self, PyObject* args)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);

        PyObject *pyatom, *pycapv;
        if (!PyArg_ParseTuple(args, "OO:unassociate", &pyatom, &pycapv) ||
            !PyString_Check(pyatom) ||
            !PyObject_IsInstance(pycapv, capv_type)) {
            pyca_raise_pyexc_pv("associate", "cannot get associate parameters", rdr);
        }

        char *atom = PyString_AsString(pyatom);
        int i;
        for (i = 0; i < rdr->count; i++) {
            char *word = PyString_AS_STRING(PyTuple_GET_ITEM(rdr->atoms, i));
            if (!strcmp(atom, word))
                break;
        }

        if (i == rdr->count) {
            pyca_raise_pyexc_pv("associate", "Bad XTC element", rdr);
        }

        PyObject *l = PyDict_GetItem(rdr->data, pyatom);

        int cnt = PyList_GET_SIZE(l);
        for (int j = 0; j < cnt; j++) {
            PyObject *pyval = PyList_GET_ITEM(l, j);
            if (pyval == pycapv) {
                PyObject *pylast = PyList_GET_ITEM(l, cnt - 1);
                if (j != cnt - 1) {
                    PyList_SET_ITEM(l, j, pylast);
                    PyList_SET_ITEM(l, cnt - 1, pycapv);
                }
                PyObject *l2 = PyList_GetSlice(l, 0, cnt - 1);
                PyDict_SetItem(rdr->data, pyatom, l2);
                return ok();
            }
        }
        return ok();
    }

    static PyObject* xtcrdr_associate(PyObject* self, PyObject* args)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);

        PyObject *pyatom, *pycapv;
        if (!PyArg_ParseTuple(args, "OO:associate", &pyatom, &pycapv) ||
            !PyString_Check(pyatom) ||
            !PyObject_IsInstance(pycapv, capv_type)) {
            pyca_raise_pyexc_pv("associate", "cannot get associate parameters", rdr);
        }

        char *atom = PyString_AsString(pyatom);
        int i;
        for (i = 0; i < rdr->count; i++) {
            char *word = PyString_AS_STRING(PyTuple_GET_ITEM(rdr->atoms, i));
            if (!strcmp(atom, word))
                break;
        }

        if (i == rdr->count) {
            pyca_raise_pyexc_pv("associate", "Bad XTC element", rdr);
        }

        PyObject *l = PyDict_GetItem(rdr->data, pyatom);
        if (!l)
            l = PyList_New(0);
        PyList_Append(l, pycapv);
        Py_INCREF(pycapv);
        PyDict_SetItem(rdr->data, pyatom, l);

        return ok();
    }

    static PyObject* xtcrdr_process(PyObject* self, PyObject* args)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        epicsTimeStamp *ts = NULL;
        struct event_handler_args ev;
        dbr_time_short *buf = NULL;

        Xtc *xtc = (Xtc *)(rdr->dg->xtc.payload());
        int size = xtc->sizeofPayload();
        xtc = (Xtc *)(xtc->payload());
        /* Find a timestamp for this event! */
        while (size > 0) {
            if (xtc->contains.id() == TypeId::Id_Epics) {
                ts = &((EpicsPvTime<DBR_LONG> *)(xtc->payload()))->stamp;
                break;
            }
            size -= xtc->extent;
            xtc = xtc->next();
        }

        ev.chid = 0;
        ev.status = ECA_NORMAL;

        xtc = (Xtc *)(rdr->dg->xtc.payload());
        xtc = (Xtc *)(xtc->payload());
        for (int i = 0; i < rdr->count; i++, xtc = xtc->next()) {
            PyObject *pyatom = PyTuple_GET_ITEM(rdr->atoms, i);
            PyObject *pylist = PyDict_GetItem(rdr->data, pyatom);

            if (!pylist)
                continue;

            int maxcnt;

            switch (xtc->contains.id()) {
            case TypeId::Id_Epics: {
                EpicsPvTime<DBR_LONG> *t = (EpicsPvTime<DBR_LONG> *)(xtc->payload());
                ev.type = t->iDbrType;
                maxcnt = t->iNumElements;
                ev.dbr = (void *)&t->status; /* First field of the dbr_time_* type */
                break;
            }
            case TypeId::Id_Frame: {
                Camera::FrameV1 *f = (Camera::FrameV1 *)(xtc->payload());
                ev.type = DBR_TIME_SHORT;
                maxcnt = f->width() * f->height();
                if (buf == NULL) {
                    buf = (dbr_time_short *)malloc(sizeof(dbr_time_short) + f->data_size());
                    buf->status = 0;
                    buf->severity = 0;
                    if (ts) {
                        buf->stamp.secPastEpoch = ts->secPastEpoch;
                        buf->stamp.nsec = ts->nsec;
                    }
                }
                memcpy(&buf->value, f->data(), maxcnt * sizeof(short));
                ev.dbr = buf;
                break;
            }
            default:
                pyca_raise_pyexc_pv("process", "Unknown TypeID in XTC file", rdr);
                break;
            }

            int cnt = PyList_GET_SIZE(pylist);
            for (int j = 0; j < cnt; j++) {
                PyObject *pyval = PyList_GET_ITEM(pylist, j);
                capv *pv = reinterpret_cast<capv*>(pyval);

                if (!pv->didmon && !pv->didget)
                    continue;

                ev.usr = pv;
                if (pv->count && pv->count < maxcnt)
                    ev.count = pv->count;
                else
                    ev.count = maxcnt;

                if (pv->didmon) {
                    Py_BEGIN_ALLOW_THREADS
                        pyca_monitor_handler(ev);
                    Py_END_ALLOW_THREADS
                }
                if (pv->didget) {
                    pv->didget = 0;
                    Py_BEGIN_ALLOW_THREADS
                        pyca_getevent_handler(ev);
                    Py_END_ALLOW_THREADS
                }
            }
        }
        if (buf)
            free(buf);

        return ok();
    }

    static PyObject* xtcrdr_moveto(PyObject* self, PyObject* args)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        PyObject *pytuple;
        if (!PyArg_ParseTuple(args, "OO:moveto", &pytuple) ||
            !PyTuple_Check(pytuple) ||
            PyTuple_Size(pytuple) != 2) {
            pyca_raise_pyexc_pv("moveto", "Invalid position", rdr);
        }
        PyObject *pychunk  = PyTuple_GET_ITEM(pytuple, 0);
        PyObject *pyoffset = PyTuple_GET_ITEM(pytuple, 1);
        if (!PyLong_Check(pychunk) || !PyLong_Check(pychunk)) {
            pyca_raise_pyexc_pv("moveto", "Invalid position", rdr);
        }
        long chunk       = PyLong_AsLong(pychunk);
        long long offset = PyLong_AsLongLong(pyoffset);
        if (chunk != rdr->chunk) {
            delete rdr->iter;
            rdr->iter = NULL;
            rdr->dg = NULL;
            rdr->chunk = chunk - 1;
            const char* name = PyString_AsString(rdr->name);
            chunk_open(rdr, name);
            if (rdr->fd == -1)
                return ok();
            rdr->iter = new XtcFileIterator(rdr->fd, 0x40000000);
        }
        lseek64(rdr->fd, offset, SEEK_SET);
        return xtcrdr_next((PyObject *)rdr, NULL);
    }

    static int xtcrdr_init(PyObject* self, PyObject* args, PyObject* kwds)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        rdr->name = Py_None;
        Py_INCREF(rdr->name);
        rdr->atoms = Py_None;
        Py_INCREF(rdr->atoms);
        rdr->chunk = -1;
        rdr->fd = -1;
        rdr->count = 0;
        rdr->buffer = 0;
        rdr->bufsiz = 0;
        return 0;
    }

    static void xtcrdr_dealloc(PyObject* self)
    {
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        Py_XDECREF(rdr->atoms);
        Py_XDECREF(rdr->data);
        Py_XDECREF(rdr->name);
        if (rdr->fd >= 0) {
            close(rdr->fd);
            rdr->fd = -1;
        }
        if (rdr->buffer) {
            delete [] rdr->buffer;
            rdr->buffer = 0;
            rdr->bufsiz = 0;
        }
        if (rdr->iter) {
            delete rdr->iter;
            rdr->iter = 0;
        }
        if (rdr->dg) {
            delete rdr->dg;
            rdr->dg = 0;
        }
        self->ob_type->tp_free(self);
    }

    static PyObject* xtcrdr_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
    {
        PyObject* self = type->tp_alloc(type, 0);
        xtcrdr* rdr = reinterpret_cast<xtcrdr*>(self);
        if (!rdr) {
            pyca_raise_pyexc("xtcrdr_new", "cannot allocate new XTC reader");
        }
        rdr->data = PyDict_New();
        if (!rdr->data) {
            pyca_raise_pyexc("xtcrdr_new", "cannot allocate dictionary for new XTC reader");
        }
        Py_INCREF(rdr);
        return self;
    }

    // Register xtcrdr methods
    static PyMethodDef xtcrdr_methods[] = {
        {"open", xtcrdr_open, METH_VARARGS},
        {"next", xtcrdr_next, METH_VARARGS},
        {"associate", xtcrdr_associate, METH_VARARGS},
        {"unassociate", xtcrdr_unassociate, METH_VARARGS},
        {"process", xtcrdr_process, METH_VARARGS},
        {"moveto", xtcrdr_moveto, METH_VARARGS},
        {NULL,  NULL},
    };

    // Register xtcrdr members
    static PyMemberDef xtcrdr_members[] = {
        {"name", T_OBJECT_EX, offsetof(xtcrdr, name), 0, "name"},
        {"data", T_OBJECT_EX, offsetof(xtcrdr, data), 0, "data"},
        {"atoms", T_OBJECT_EX, offsetof(xtcrdr, atoms), 0, "atoms"},
        {NULL}
    };

    static PyTypeObject xtcrdr_type = {
        PyObject_HEAD_INIT(0)
        0,
        "pyca.xtcrdr",
        sizeof(xtcrdr),
        0,
        xtcrdr_dealloc,                         /* tp_dealloc */
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
        xtcrdr_methods,                         /* tp_methods */
        xtcrdr_members,                         /* tp_members */
        0,                                      /* tp_getset */
        0,                                      /* tp_base */
        0,                                      /* tp_dict */
        0,                                      /* tp_descr_get */
        0,                                      /* tp_descr_set */
        0,                                      /* tp_dictoffset */
        xtcrdr_init,                            /* tp_init */
        0,                                      /* tp_alloc */
        xtcrdr_new,                             /* tp_new */
    };


    // Register module methods
    static PyMethodDef module_methods[] = {
        {NULL, NULL}
    };

    // Initialize python module
    void initxtcrdr()
    {
        if (PyType_Ready(&xtcrdr_type) < 0) {
            return;
        }

        PyObject* module = Py_InitModule("xtcrdr", module_methods);
        if (!module) {
            return;
        }

        pyca = PyImport_ImportModule("pyca");
        if (!pyca) {
            printf("Cannot import pyca!!!\n");
            return;
        }

        PyObject *v = PyObject_GetAttrString(pyca, "get_handler");
        if (!v) {
            printf("Cannot find pyca.get_handler?!?\n");
            return;
        }
        pyca_getevent_handler = (void (*)(struct event_handler_args))PyCObject_AsVoidPtr(v);

        v = PyObject_GetAttrString(pyca, "monitor_handler");
        if (!v) {
            printf("Cannot find pyca.monitor_handler?!?\n");
            return;
        }
        pyca_monitor_handler = (void (*)(struct event_handler_args))PyCObject_AsVoidPtr(v);

        capv_type = PyObject_GetAttrString(pyca, "capv");
        if (!capv_type) {
            printf("Cannot find pyca.capv?!?\n");
            return;
        }

        // Add xtcrdr type to this module
        Py_INCREF(&xtcrdr_type);
	PyModule_AddObject(module, "xtcrdr", (PyObject*)&xtcrdr_type);

        // Add custom exceptions to this module
        pyca_pyexc = PyObject_GetAttrString(pyca, "pyexc");
        if (!pyca_pyexc) {
            printf("Cannot find pyca.pyexc?!?\n");
            return;
        }
        pyca_caexc = PyObject_GetAttrString(pyca, "caexc");
        if (!pyca_caexc) {
            printf("Cannot find pyca.caexc?!?\n");
            return;
        }

        PyEval_InitThreads();
    }
}
