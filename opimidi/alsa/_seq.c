#include <Python.h>
#include <structmember.h>

#include <asoundlib.h>

typedef struct {
	PyObject_HEAD
	snd_seq_t *handle;
	int client_id;
} SeqClient;

typedef struct {
	PyObject_HEAD
        snd_seq_event_t ev;
} SeqEvent;
static PyTypeObject SeqEventType;

static void
SeqClient_dealloc(SeqClient* self)
{
    if (self->handle) {
         snd_seq_close(self->handle);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
SeqClient_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    SeqClient *self;

    self = (SeqClient *)type->tp_alloc(type, 0);
    self->handle = 0;
    return (PyObject *)self;
}

static int
SeqClient_init(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"client_name", "streams", "mode", "sequencer_name", NULL};
    char *client_name = NULL;
    char *sequencer_name = "default";
    int streams = SND_SEQ_OPEN_DUPLEX, mode = 0;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s|iis", kwlist,
                                      &client_name, &streams, &mode,
                                      &sequencer_name))
        return -1;

    if (self->handle)
        return 0; /* initiaize just once */

    int err;
    err = snd_seq_open(&self->handle, sequencer_name, streams, mode);
    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return -1;
    }

    snd_seq_set_client_name(self->handle, client_name);
    self->client_id = snd_seq_client_id(self->handle);

    return 0;
}

PyObject *
SeqClient_create_port(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"name", "caps", "type", NULL};
    char *name = NULL;
    int caps = 0, type = SND_SEQ_PORT_TYPE_MIDI_GENERIC;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "si|i", kwlist,
                                      &name, &caps, &type))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int port = snd_seq_create_simple_port(self->handle, name, caps, type);
    if (port < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-port));
        return NULL;
    }

    return PyLong_FromLong(port);
}

PyObject *
SeqClient_delete_port(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"port", NULL};
    int port = 0;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &port))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int err = snd_seq_delete_simple_port(self->handle, port);
    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_create_queue(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"name", NULL};
    char *name = NULL;
    int queue = -1;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &name))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    if (name) {
        queue = snd_seq_alloc_named_queue(self->handle, name);
    }
    else {
        queue = snd_seq_alloc_queue(self->handle);
    }

    if (queue < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-queue));
        return NULL;
    }

    return PyLong_FromLong(queue);
}

PyObject *
SeqClient_delete_queue(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"queue", NULL};
    int queue = 0;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &queue))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int err = snd_seq_free_queue(self->handle, queue);
    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_set_queue_tempo(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"queue", "tempo", "ppq", NULL};
    int queue = 0;
    unsigned int tempo = 500000;
    int ppq = 96;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "i|Ii", kwlist,
                                      &queue, &tempo, &ppq))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    snd_seq_queue_tempo_t *q_tempo;
    snd_seq_queue_tempo_alloca(&q_tempo);
    snd_seq_queue_tempo_set_tempo(q_tempo, tempo);
    snd_seq_queue_tempo_set_ppq(q_tempo, ppq);
    int err = snd_seq_set_queue_tempo(self->handle, queue, q_tempo);

    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_control_queue(SeqClient *self, PyObject *args, PyObject *kwds,
                        enum snd_seq_event_type ev_type)
{
    static char *kwlist[] = {"queue", "event", NULL};
    int queue = 0;
    SeqEvent* event = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "i|O!", kwlist,
                                      &queue, &SeqEventType, &event))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    snd_seq_event_t* ev = NULL;
    if (event) {
        ev = &event->ev;
    }

    int err = snd_seq_control_queue(self->handle, queue, ev_type, 0, ev);
    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_start_queue(SeqClient *self, PyObject *args, PyObject *kwds)
{
    return SeqClient_control_queue(self, args, kwds, SND_SEQ_EVENT_START);
}
PyObject *
SeqClient_stop_queue(SeqClient *self, PyObject *args, PyObject *kwds)
{
    return SeqClient_control_queue(self, args, kwds, SND_SEQ_EVENT_STOP);
}
PyObject *
SeqClient_continue_queue(SeqClient *self, PyObject *args, PyObject *kwds)
{
    return SeqClient_control_queue(self, args, kwds, SND_SEQ_EVENT_CONTINUE);
}

PyObject *
SeqClient_event_output(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"event", "port", "queue", "dest", "direct",
                             "tick", "realtime", "relative", NULL};
    SeqEvent* event = NULL;
    PyObject* port_o = NULL;
    PyObject* queue_o = NULL;
    PyObject* dest_client_o = NULL, * dest_port_o = NULL;
    PyObject* direct_o = NULL;
    PyObject* tick_o = NULL, * rt_sec_o = NULL, * rt_nsec_o = NULL;
    int relative = 1;

    snd_seq_event_t ev;
    snd_seq_event_t *ev_p;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O!|OO(OO)OO(OO)p", kwlist,
                                      &SeqEventType, &event,
                                      &port_o, &queue_o,
                                      &dest_client_o, &dest_port_o,
                                      &direct_o,
                                      &tick_o, &rt_sec_o, &rt_nsec_o,
                                      &relative))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    if (tick_o && (rt_sec_o || rt_nsec_o)) {
        PyErr_SetString(PyExc_ValueError, "'tick' and 'realtime' schedule cannot be used at the same time");
        return NULL;
    }
    if (queue_o && direct_o && PyObject_IsTrue(direct_o)) {
        PyErr_SetString(PyExc_ValueError, "'queue' and 'direct' cannot be used at the same time");
        return NULL;
    }

    if (port_o || queue_o || dest_client_o || dest_port_o || direct_o || tick_o || rt_sec_o || rt_nsec_o) {
        // make a copy, as modifying event here would be unexpected
        ev = event->ev;
        ev_p = &ev;
    }
    else {
        ev_p = &event->ev;
    }

    if (port_o) {
        long port = PyLong_AsLong(port_o);
        if (PyErr_Occurred()) return NULL;
        if (port < 0 || port > 255) {
            PyErr_SetString(PyExc_ValueError, "Invalid port number");
            return NULL;
        }
        snd_seq_ev_set_source(&ev, port);
    }

    if (dest_client_o && dest_port_o) {
        long client = PyLong_AsLong(dest_client_o);
        if (PyErr_Occurred()) return NULL;
        if (client < 0 || client > 255) {
            PyErr_SetString(PyExc_ValueError, "Invalid client number");
            return NULL;
        }
        long port = PyLong_AsLong(dest_port_o);
        if (PyErr_Occurred()) return NULL;
        if (port < 0 || port > 255) {
            PyErr_SetString(PyExc_ValueError, "Invalid port number");
            return NULL;
        }
        ev.dest.client = client;
        ev.dest.port = port;
    }

    if (queue_o) {
        long queue = PyLong_AsLong(queue_o);
        if (PyErr_Occurred()) return NULL;
        if (queue < 0 || queue > 255) {
            PyErr_SetString(PyExc_ValueError, "Invalid queue number");
            return NULL;
        }
        ev.queue = queue;
    }

    if (direct_o && PyObject_IsTrue(direct_o)) {
        ev.queue = SND_SEQ_QUEUE_DIRECT;
    }

    if (tick_o) {
        unsigned long tick = PyLong_AsUnsignedLong(tick_o);
        if (PyErr_Occurred()) return NULL;
        ev.flags &= ~(SND_SEQ_TIME_STAMP_MASK | SND_SEQ_TIME_MODE_MASK);
        ev.flags |= SND_SEQ_TIME_STAMP_TICK;
        ev.flags |= relative ? SND_SEQ_TIME_MODE_REL : SND_SEQ_TIME_MODE_ABS;
        ev.time.tick = tick;
    }

    if (rt_sec_o && rt_nsec_o) {
        unsigned long sec = PyLong_AsUnsignedLong(rt_sec_o);
        if (PyErr_Occurred()) return NULL;
        unsigned long nsec = PyLong_AsUnsignedLong(rt_nsec_o);
        if (PyErr_Occurred()) return NULL;
        ev.flags &= ~(SND_SEQ_TIME_STAMP_MASK | SND_SEQ_TIME_MODE_MASK);
        ev.flags |= SND_SEQ_TIME_STAMP_REAL;
        ev.flags |= relative ? SND_SEQ_TIME_MODE_REL : SND_SEQ_TIME_MODE_ABS;
        ev.time.time.tv_sec = sec;
        ev.time.time.tv_nsec = nsec;
    }

    if (event->ev.type == SND_SEQ_EVENT_NOTE && ev_p->queue == SND_SEQ_QUEUE_DIRECT) {
        PyErr_SetString(PyExc_ValueError, "Note events must be enqueued");
        return NULL;
    }

    int err = snd_seq_event_output(self->handle, ev_p);
    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


PyObject *
SeqClient_drain_output(SeqClient *self)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int err = snd_seq_drain_output(self->handle);

    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_drop_output(SeqClient *self)
{
    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int err = snd_seq_drop_output(self->handle);

    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_connect_to(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"port", "dest_client", "dest_port", NULL};
    int port=-1, dest_client=-1, dest_port=-1;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "III", kwlist, &port,&dest_client, &dest_port))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int err = snd_seq_connect_to(self->handle, port, dest_client, dest_port);

    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_disconnect_to(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"port", "dest_client", "dest_port", NULL};
    int port=-1, dest_client=-1, dest_port=-1;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "III", kwlist, &port,&dest_client, &dest_port))
        return NULL;

    if (!self->handle) {
        PyErr_SetString(PyExc_RuntimeError, "already closed");
        return NULL;
    }

    int err = snd_seq_disconnect_to(self->handle, port, dest_client, dest_port);

    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
SeqClient_parse_address(SeqClient *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"name", NULL};
    const char *name;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &name))
        return NULL;

    snd_seq_addr_t addr;
    int err =  snd_seq_parse_address(self->handle, &addr, name);

    if (err < 0) {
        PyErr_SetString(PyExc_RuntimeError, snd_strerror(-err));
        return NULL;
    }

    return Py_BuildValue("bb", addr.client, addr.port);
}


static PyMethodDef SeqClient_methods[] = {
    {"create_port", (PyCFunction)SeqClient_create_port, METH_VARARGS | METH_KEYWORDS,
             "Create port",
    },
    {"delete_port", (PyCFunction)SeqClient_delete_port, METH_VARARGS | METH_KEYWORDS,
             "Delete port",
    },
    {"create_queue", (PyCFunction)SeqClient_create_queue, METH_VARARGS | METH_KEYWORDS,
             "Create queue",
    },
    {"delete_queue", (PyCFunction)SeqClient_delete_queue, METH_VARARGS | METH_KEYWORDS,
             "Delete queue",
    },
    {"set_queue_tempo", (PyCFunction)SeqClient_set_queue_tempo, METH_VARARGS | METH_KEYWORDS,
             "Set queue tempo",
    },
    {"start_queue", (PyCFunction)SeqClient_start_queue, METH_VARARGS | METH_KEYWORDS,
             "Start queue",
    },
    {"stop_queue", (PyCFunction)SeqClient_stop_queue, METH_VARARGS | METH_KEYWORDS,
             "Stop queue",
    },
    {"continue_queue", (PyCFunction)SeqClient_continue_queue, METH_VARARGS | METH_KEYWORDS,
             "Continue queue",
    },
    {"event_output", (PyCFunction)SeqClient_event_output, METH_VARARGS | METH_KEYWORDS,
             "Output an event",
    },
    {"drain_output", (PyCFunction)SeqClient_drain_output, METH_NOARGS,
             "Drop pending output events",
    },
    {"drop_output", (PyCFunction)SeqClient_drop_output, METH_NOARGS,
             "Drop pending output events",
    },
    {"connect_to", (PyCFunction)SeqClient_connect_to, METH_VARARGS | METH_KEYWORDS,
             "Connect sending port to external receiver",
    },
    {"disconnect_to", (PyCFunction)SeqClient_disconnect_to, METH_VARARGS | METH_KEYWORDS,
             "Disconnect sending port from external receiver",
    },
    {"parse_address", (PyCFunction)SeqClient_parse_address, METH_VARARGS | METH_KEYWORDS,
             "Convert 'client:port' string or client name to (client,port) tuple",
    },
    {NULL}  /* Sentinel */
};

static PyMemberDef SeqClient_members[] = {
    {"client_id", T_INT, offsetof(SeqClient, client_id), 0, "client id"},
    {NULL}  /* Sentinel */
};

static PyTypeObject SeqClientType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "opimidi.alsa._seq.SeqClient",
    .tp_basicsize = sizeof(SeqClient),
    .tp_dealloc = (destructor)SeqClient_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ALSA sequencer client",
    .tp_methods = SeqClient_methods,
    .tp_members = SeqClient_members,
    .tp_init = (initproc)SeqClient_init,
    .tp_new = SeqClient_new,
};

static void
SeqEvent_dealloc(SeqEvent* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
SeqEvent_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    SeqEvent *self;

    self = (SeqEvent *)type->tp_alloc(type, 0);

    return (PyObject *)self;
}

static int
SeqEvent_init_generic(SeqEvent *self, PyObject *args, PyObject *kwds,
                      snd_seq_event_type_t type,
                      const char *argformat,
                      char *kwlist[],
                      ...)
{
    va_list arglist;

    int res;

    // clearing before parsing arguments as parsing will fill the event
    // ugly, but works
    snd_seq_ev_clear(&self->ev);
    self->ev.type = type;
    // reasonable defaults
    self->ev.queue = SND_SEQ_QUEUE_DIRECT;
    self->ev.dest.client = SND_SEQ_ADDRESS_SUBSCRIBERS;
    self->ev.dest.port = SND_SEQ_ADDRESS_UNKNOWN;

    va_start(arglist, kwlist);
    res = PyArg_VaParseTupleAndKeywords(args, kwds, argformat, kwlist, arglist);
    va_end(arglist);
    if (!res)
        return -1;

    return 0;
}

#define EVENT_ARGS ""
#define EVENT_OARGS "bbbIIIbbbb"
#define EVENT_KWARGS \
    "flags", "tag", "queue", \
    "tick", "tv_sec", "tv_nsec", \
    "source_client", "source_port", \
    "dest_client", "dest_port"

#define EVENT_ARGPTRS \
    &self->ev.flags, \
    &self->ev.tag, \
    &self->ev.queue, \
    &self->ev.time.tick, \
    &self->ev.time.time.tv_sec, \
    &self->ev.time.time.tv_nsec, \
    &self->ev.source.client, \
    &self->ev.source.port, \
    &self->ev.dest.client, \
    &self->ev.dest.port \


static int
SeqEvent_init(SeqEvent *self, PyObject *args, PyObject *kwds)
{
    static char *argformat = "b" EVENT_ARGS "|" EVENT_OARGS;
    static char *kwlist[] = {"type", EVENT_KWARGS, NULL};
    return SeqEvent_init_generic(self, args, kwds, SND_SEQ_EVENT_NONE,
                                 argformat, kwlist, &self->ev.type, EVENT_ARGPTRS);
}

static PyMethodDef SeqEvent_methods[] = {
/*    {"create_port", (PyCFunction)SeqEvent_create_port, METH_VARARGS | METH_KEYWORDS,
             "Create port",
    },*/
    {NULL}  /* Sentinel */
};

static PyMemberDef SeqEvent_members[] = {
    {"type", T_UBYTE, offsetof(SeqEvent, ev.type), 0, "event type"},
    {"flags", T_UBYTE, offsetof(SeqEvent, ev.flags), 0, "event flags"},
    {"tag", T_UBYTE, offsetof(SeqEvent, ev.tag), 0, "event tag"},
    {"queue", T_UBYTE, offsetof(SeqEvent, ev.queue), 0, "event queue"},
    {"tick", T_UINT, offsetof(SeqEvent, ev.time.tick), 0, "event tick time"},
    {"tv_sec", T_UINT, offsetof(SeqEvent, ev.time.time.tv_sec), 0, "event time seconds"},
    {"tv_nsec", T_UINT, offsetof(SeqEvent, ev.time.time.tv_nsec), 0, "event time nanoseconds"},
    {"source_client", T_UBYTE, offsetof(SeqEvent, ev.source.client), 0, "event source client"},
    {"source_port", T_UBYTE, offsetof(SeqEvent, ev.source.port), 0, "event source port"},
    {"dest_client", T_UBYTE, offsetof(SeqEvent, ev.dest.client), 0, "event dest client"},
    {"dest_port", T_UBYTE, offsetof(SeqEvent, ev.dest.port), 0, "event dest port"},
    {NULL}  /* Sentinel */
};

static PyTypeObject SeqEventType = {
    PyVarObject_HEAD_INIT(NULL, 0)

    .tp_name = "opimidi.alsa._seq.SeqEvent",
    .tp_basicsize = sizeof(SeqEvent),
    .tp_dealloc = (destructor)SeqEvent_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ALSA sequencer event base type",
    .tp_methods = SeqEvent_methods,
    .tp_members = SeqEvent_members,
    .tp_init = (initproc)SeqEvent_init,
    .tp_new = SeqEvent_new,
};

static int
SeqNoteEvent_init(SeqEvent *self, PyObject *args, PyObject *kwds)
{
    static char *argformat = EVENT_ARGS "|bbbbI" EVENT_OARGS;
    static char *kwlist[] = {"channel", "note", "velocity",
                             "off_velocity", "duration",
                             EVENT_KWARGS, NULL};
    return SeqEvent_init_generic(self, args, kwds, SND_SEQ_EVENT_NOTE,
                                 argformat, kwlist,
                                 &self->ev.data.note.channel,
                                 &self->ev.data.note.note,
                                 &self->ev.data.note.velocity,
                                 &self->ev.data.note.off_velocity,
                                 &self->ev.data.note.duration,
                                 EVENT_ARGPTRS);
}

static PyMemberDef SeqNoteEvent_members[] = {
    {"type", T_UBYTE, offsetof(SeqEvent, ev.type), READONLY, "event type"},
    {"channel", T_UBYTE, offsetof(SeqEvent, ev.data.note.channel), 0, "note channel"},
    {"note", T_UBYTE, offsetof(SeqEvent, ev.data.note.note), 0, "note number"},
    {"velocity", T_UBYTE, offsetof(SeqEvent, ev.data.note.velocity), 0, "note velocity"},
    {"off_velocity", T_UBYTE, offsetof(SeqEvent, ev.data.note.off_velocity), 0, "note off velocity"},
    {"duration", T_UINT, offsetof(SeqEvent, ev.data.note.duration), 0, "note duration"},
    {NULL}  /* Sentinel */
};

static PyTypeObject SeqNoteEventType = {
    PyVarObject_HEAD_INIT(NULL, 0)

    .tp_name = "opimidi.alsa._seq.SeqNoteEvent",
    .tp_basicsize = sizeof(SeqEvent),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ALSA sequencer note event",
    .tp_members = SeqNoteEvent_members,
    .tp_init = (initproc)SeqNoteEvent_init,
    .tp_base = &SeqEventType,
};

static int
SeqNoteOnEvent_init(SeqEvent *self, PyObject *args, PyObject *kwds)
{
    static char *argformat = EVENT_ARGS "|bbI" EVENT_OARGS;
    static char *kwlist[] = {"channel", "note", "velocity",
                             EVENT_KWARGS, NULL};
    return SeqEvent_init_generic(self, args, kwds, SND_SEQ_EVENT_NOTEON,
                                 argformat, kwlist,
                                 &self->ev.data.note.channel,
                                 &self->ev.data.note.note,
                                 &self->ev.data.note.velocity,
                                 EVENT_ARGPTRS);
}

static PyMemberDef SeqNoteOnEvent_members[] = {
    {"type", T_UBYTE, offsetof(SeqEvent, ev.type), READONLY, "event type"},
    {"channel", T_UBYTE, offsetof(SeqEvent, ev.data.note.channel), 0, "note channel"},
    {"note", T_UBYTE, offsetof(SeqEvent, ev.data.note.note), 0, "note number"},
    {"velocity", T_UBYTE, offsetof(SeqEvent, ev.data.note.velocity), 0, "note velocity"},
    {NULL}  /* Sentinel */
};

static PyTypeObject SeqNoteOnEventType = {
    PyVarObject_HEAD_INIT(NULL, 0)

    .tp_name = "opimidi.alsa._seq.SeqNoteOnEvent",
    .tp_basicsize = sizeof(SeqEvent),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ALSA sequencer note event",
    .tp_members = SeqNoteOnEvent_members,
    .tp_init = (initproc)SeqNoteOnEvent_init,
    .tp_base = &SeqEventType,
};

static int
SeqNoteOffEvent_init(SeqEvent *self, PyObject *args, PyObject *kwds)
{
    static char *argformat = EVENT_ARGS "|bbI" EVENT_OARGS;
    static char *kwlist[] = {"channel", "note", "velocity",
                             EVENT_KWARGS, NULL};
    return SeqEvent_init_generic(self, args, kwds, SND_SEQ_EVENT_NOTEOFF,
                                 argformat, kwlist,
                                 &self->ev.data.note.channel,
                                 &self->ev.data.note.note,
                                 &self->ev.data.note.velocity,
                                 EVENT_ARGPTRS);
}

static PyMemberDef SeqNoteOffEvent_members[] = {
    {"type", T_UBYTE, offsetof(SeqEvent, ev.type), READONLY, "event type"},
    {"channel", T_UBYTE, offsetof(SeqEvent, ev.data.note.channel), 0, "note channel"},
    {"note", T_UBYTE, offsetof(SeqEvent, ev.data.note.note), 0, "note number"},
    {"velocity", T_UBYTE, offsetof(SeqEvent, ev.data.note.velocity), 0, "note velocity"},
    {NULL}  /* Sentinel */
};

static PyTypeObject SeqNoteOffEventType = {
    PyVarObject_HEAD_INIT(NULL, 0)

    .tp_name = "opimidi.alsa._seq.SeqNoteOffEvent",
    .tp_basicsize = sizeof(SeqEvent),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ALSA sequencer note event",
    .tp_members = SeqNoteOffEvent_members,
    .tp_init = (initproc)SeqNoteOffEvent_init,
    .tp_base = &SeqEventType,
};

static int
SeqControlChangeEvent_init(SeqEvent *self, PyObject *args, PyObject *kwds)
{
    static char *argformat = EVENT_ARGS "|bII" EVENT_OARGS;
    static char *kwlist[] = {"channel", "param", "value",
                             EVENT_KWARGS, NULL};
    int res = SeqEvent_init_generic(self, args, kwds, SND_SEQ_EVENT_CONTROLLER,
                                 argformat, kwlist,
                                 &self->ev.data.control.channel,
                                 &self->ev.data.control.param,
                                 &self->ev.data.control.value,
                                 EVENT_ARGPTRS);
    if (!res) {
        // SeqEvent_init_generic succeeded
        if (self->ev.data.control.channel < 0 || self->ev.data.control.channel > 127) {
            PyErr_SetString(PyExc_ValueError, "'channel' must be 0-127");
            return -1;
        }
        if (self->ev.data.control.param < 0 || self->ev.data.control.param > 127) {
            PyErr_SetString(PyExc_ValueError, "'param' must be 0-127");
            return -1;
        }
        if (self->ev.data.control.value < 0 || self->ev.data.control.value > 127) {
            PyErr_SetString(PyExc_ValueError, "'value' must be 0-127");
            return -1;
        }
    }
    return res;
}

static PyMemberDef SeqControlChangeEvent_members[] = {
    {"type", T_UBYTE, offsetof(SeqEvent, ev.type), READONLY, "event type"},
    {"channel", T_UBYTE, offsetof(SeqEvent, ev.data.control.channel), 0, "controller channel"},
    {"param", T_UBYTE, offsetof(SeqEvent, ev.data.control.param), 0, "controller number"},
    {"value", T_UBYTE, offsetof(SeqEvent, ev.data.control.value), 0, "controller value"},
    {NULL}  /* Sentinel */
};

static PyTypeObject SeqControlChangeEventType = {
    PyVarObject_HEAD_INIT(NULL, 0)

    .tp_name = "opimidi.alsa._seq.SeqControlChangeEvent",
    .tp_basicsize = sizeof(SeqEvent),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ALSA sequencer control change event",
    .tp_members = SeqControlChangeEvent_members,
    .tp_init = (initproc)SeqControlChangeEvent_init,
    .tp_base = &SeqEventType,
};


static PyModuleDef seqmodule = {
    PyModuleDef_HEAD_INIT,
    "opimidi.alsa._seq",
    "ALSA sequencer interface.",
    -1,
    NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit__seq(void)
{
    PyObject* m;

    if (PyType_Ready(&SeqClientType) < 0)
        return NULL;

    if (PyType_Ready(&SeqEventType) < 0)
        return NULL;

    if (PyType_Ready(&SeqNoteEventType) < 0)
        return NULL;
    if (PyType_Ready(&SeqNoteOnEventType) < 0)
        return NULL;
    if (PyType_Ready(&SeqNoteOffEventType) < 0)
        return NULL;
    if (PyType_Ready(&SeqControlChangeEventType) < 0)
        return NULL;

    m = PyModule_Create(&seqmodule);
    if (m == NULL)
        return NULL;

    Py_INCREF(&SeqClientType);
    PyModule_AddObject(m, "SeqClient", (PyObject *)&SeqClientType);

    Py_INCREF(&SeqEventType);
    PyModule_AddObject(m, "SeqEvent", (PyObject *)&SeqEventType);

    Py_INCREF(&SeqNoteEventType);
    PyModule_AddObject(m, "SeqNoteEvent", (PyObject *)&SeqNoteEventType);
    Py_INCREF(&SeqNoteOnEventType);
    PyModule_AddObject(m, "SeqNoteOnEvent", (PyObject *)&SeqNoteOnEventType);
    Py_INCREF(&SeqNoteOffEventType);
    PyModule_AddObject(m, "SeqNoteOffEvent", (PyObject *)&SeqNoteOffEventType);
    Py_INCREF(&SeqControlChangeEventType);
    PyModule_AddObject(m, "SeqControlChangeEvent", (PyObject *)&SeqControlChangeEventType);

    PyModule_AddIntConstant(m, "OPEN_OUTPUT", SND_SEQ_OPEN_OUTPUT);
    PyModule_AddIntConstant(m, "OPEN_INPUT", SND_SEQ_OPEN_INPUT);
    PyModule_AddIntConstant(m, "OPEN_DUPLEX", SND_SEQ_OPEN_DUPLEX);
    PyModule_AddIntConstant(m, "NONBLOCK", SND_SEQ_NONBLOCK);
    PyModule_AddIntConstant(m, "ADDRESS_UNKNOWN", SND_SEQ_ADDRESS_UNKNOWN);
    PyModule_AddIntConstant(m, "ADDRESS_SUBSCRIBERS", SND_SEQ_ADDRESS_SUBSCRIBERS);
    PyModule_AddIntConstant(m, "ADDRESS_BROADCAST", SND_SEQ_ADDRESS_BROADCAST);
    PyModule_AddIntConstant(m, "CLIENT_SYSTEM", SND_SEQ_CLIENT_SYSTEM);
    PyModule_AddIntConstant(m, "USER_CLIENT", SND_SEQ_USER_CLIENT);
    PyModule_AddIntConstant(m, "KERNEL_CLIENT", SND_SEQ_KERNEL_CLIENT);
    PyModule_AddIntConstant(m, "PORT_SYSTEM_TIMER", SND_SEQ_PORT_SYSTEM_TIMER);
    PyModule_AddIntConstant(m, "PORT_SYSTEM_ANNOUNCE", SND_SEQ_PORT_SYSTEM_ANNOUNCE);
    PyModule_AddIntConstant(m, "PORT_CAP_READ", SND_SEQ_PORT_CAP_READ);
    PyModule_AddIntConstant(m, "PORT_CAP_WRITE", SND_SEQ_PORT_CAP_WRITE);
    PyModule_AddIntConstant(m, "PORT_CAP_SYNC_READ", SND_SEQ_PORT_CAP_SYNC_READ);
    PyModule_AddIntConstant(m, "PORT_CAP_SYNC_WRITE", SND_SEQ_PORT_CAP_SYNC_WRITE);
    PyModule_AddIntConstant(m, "PORT_CAP_DUPLEX", SND_SEQ_PORT_CAP_DUPLEX);
    PyModule_AddIntConstant(m, "PORT_CAP_SUBS_READ", SND_SEQ_PORT_CAP_SUBS_READ);
    PyModule_AddIntConstant(m, "PORT_CAP_SUBS_WRITE", SND_SEQ_PORT_CAP_SUBS_WRITE);
    PyModule_AddIntConstant(m, "PORT_CAP_NO_EXPORT", SND_SEQ_PORT_CAP_NO_EXPORT);
    PyModule_AddIntConstant(m, "PORT_TYPE_SPECIFIC", SND_SEQ_PORT_TYPE_SPECIFIC);
    PyModule_AddIntConstant(m, "PORT_TYPE_MIDI_GENERIC", SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    PyModule_AddIntConstant(m, "PORT_TYPE_MIDI_GM", SND_SEQ_PORT_TYPE_MIDI_GM);
    PyModule_AddIntConstant(m, "PORT_TYPE_MIDI_GS", SND_SEQ_PORT_TYPE_MIDI_GS);
    PyModule_AddIntConstant(m, "PORT_TYPE_MIDI_XG", SND_SEQ_PORT_TYPE_MIDI_XG);
    PyModule_AddIntConstant(m, "PORT_TYPE_MIDI_MT32", SND_SEQ_PORT_TYPE_MIDI_MT32);
    PyModule_AddIntConstant(m, "PORT_TYPE_MIDI_GM2", SND_SEQ_PORT_TYPE_MIDI_GM2);
    PyModule_AddIntConstant(m, "PORT_TYPE_SYNTH", SND_SEQ_PORT_TYPE_SYNTH);
    PyModule_AddIntConstant(m, "PORT_TYPE_DIRECT_SAMPLE", SND_SEQ_PORT_TYPE_DIRECT_SAMPLE);
    PyModule_AddIntConstant(m, "PORT_TYPE_SAMPLE", SND_SEQ_PORT_TYPE_SAMPLE);
    PyModule_AddIntConstant(m, "PORT_TYPE_HARDWARE", SND_SEQ_PORT_TYPE_HARDWARE);
    PyModule_AddIntConstant(m, "PORT_TYPE_SOFTWARE", SND_SEQ_PORT_TYPE_SOFTWARE);
    PyModule_AddIntConstant(m, "PORT_TYPE_SYNTHESIZER", SND_SEQ_PORT_TYPE_SYNTHESIZER);
    PyModule_AddIntConstant(m, "PORT_TYPE_PORT", SND_SEQ_PORT_TYPE_PORT);
    PyModule_AddIntConstant(m, "PORT_TYPE_APPLICATION", SND_SEQ_PORT_TYPE_APPLICATION);
    PyModule_AddIntConstant(m, "TIMER_ALSA", SND_SEQ_TIMER_ALSA);
    PyModule_AddIntConstant(m, "TIMER_MIDI_CLOCK", SND_SEQ_TIMER_MIDI_CLOCK);
    PyModule_AddIntConstant(m, "TIMER_MIDI_TICK", SND_SEQ_TIMER_MIDI_TICK);
    PyModule_AddIntConstant(m, "REMOVE_INPUT", SND_SEQ_REMOVE_INPUT);
    PyModule_AddIntConstant(m, "REMOVE_OUTPUT", SND_SEQ_REMOVE_OUTPUT);
    PyModule_AddIntConstant(m, "REMOVE_DEST", SND_SEQ_REMOVE_DEST);
    PyModule_AddIntConstant(m, "REMOVE_DEST_CHANNEL", SND_SEQ_REMOVE_DEST_CHANNEL);
    PyModule_AddIntConstant(m, "REMOVE_TIME_BEFORE", SND_SEQ_REMOVE_TIME_BEFORE);
    PyModule_AddIntConstant(m, "REMOVE_TIME_AFTER", SND_SEQ_REMOVE_TIME_AFTER);
    PyModule_AddIntConstant(m, "REMOVE_TIME_TICK", SND_SEQ_REMOVE_TIME_TICK);
    PyModule_AddIntConstant(m, "REMOVE_EVENT_TYPE", SND_SEQ_REMOVE_EVENT_TYPE);
    PyModule_AddIntConstant(m, "REMOVE_IGNORE_OFF", SND_SEQ_REMOVE_IGNORE_OFF);
    PyModule_AddIntConstant(m, "REMOVE_TAG_MATCH", SND_SEQ_REMOVE_TAG_MATCH);
    PyModule_AddIntConstant(m, "EVFLG_RESULT", SND_SEQ_EVFLG_RESULT);
    PyModule_AddIntConstant(m, "EVFLG_NOTE", SND_SEQ_EVFLG_NOTE);
    PyModule_AddIntConstant(m, "EVFLG_CONTROL", SND_SEQ_EVFLG_CONTROL);
    PyModule_AddIntConstant(m, "EVFLG_QUEUE", SND_SEQ_EVFLG_QUEUE);
    PyModule_AddIntConstant(m, "EVFLG_SYSTEM", SND_SEQ_EVFLG_SYSTEM);
    PyModule_AddIntConstant(m, "EVFLG_MESSAGE", SND_SEQ_EVFLG_MESSAGE);
    PyModule_AddIntConstant(m, "EVFLG_CONNECTION", SND_SEQ_EVFLG_CONNECTION);
    PyModule_AddIntConstant(m, "EVFLG_SAMPLE", SND_SEQ_EVFLG_SAMPLE);
    PyModule_AddIntConstant(m, "EVFLG_USERS", SND_SEQ_EVFLG_USERS);
    PyModule_AddIntConstant(m, "EVFLG_INSTR", SND_SEQ_EVFLG_INSTR);
    PyModule_AddIntConstant(m, "EVFLG_QUOTE", SND_SEQ_EVFLG_QUOTE);
    PyModule_AddIntConstant(m, "EVFLG_NONE", SND_SEQ_EVFLG_NONE);
    PyModule_AddIntConstant(m, "EVFLG_RAW", SND_SEQ_EVFLG_RAW);
    PyModule_AddIntConstant(m, "EVFLG_FIXED", SND_SEQ_EVFLG_FIXED);
    PyModule_AddIntConstant(m, "EVFLG_VARIABLE", SND_SEQ_EVFLG_VARIABLE);
    PyModule_AddIntConstant(m, "EVFLG_VARUSR", SND_SEQ_EVFLG_VARUSR);
    PyModule_AddIntConstant(m, "EVFLG_NOTE_ONEARG", SND_SEQ_EVFLG_NOTE_ONEARG);
    PyModule_AddIntConstant(m, "EVFLG_NOTE_TWOARG", SND_SEQ_EVFLG_NOTE_TWOARG);
    PyModule_AddIntConstant(m, "EVFLG_QUEUE_NOARG", SND_SEQ_EVFLG_QUEUE_NOARG);
    PyModule_AddIntConstant(m, "EVFLG_QUEUE_TICK", SND_SEQ_EVFLG_QUEUE_TICK);
    PyModule_AddIntConstant(m, "EVFLG_QUEUE_TIME", SND_SEQ_EVFLG_QUEUE_TIME);
    PyModule_AddIntConstant(m, "EVFLG_QUEUE_VALUE", SND_SEQ_EVFLG_QUEUE_VALUE);

    return m;
}

// vi: sts=4 et sw=4
