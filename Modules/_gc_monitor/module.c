

#ifndef Py_BUILD_CORE_BUILTIN
#  ifndef Py_BUILD_CORE_MODULE
#    define Py_BUILD_CORE_MODULE 1
#  endif
#endif

#include "Python.h"
#include "internal/pycore_debug_offsets.h"  // _Py_DebugOffsets
#include "internal/pycore_frame.h"          // FRAME_SUSPENDED_YIELD_FROM
#include "internal/pycore_interpframe.h"    // FRAME_OWNED_BY_INTERPRETER
#include "internal/pycore_interp_structs.h" // gc_stats
#include "internal/pycore_llist.h"          // struct llist_node
#include "internal/pycore_long.h"           // _PyLong_GetZero
#include "internal/pycore_pyerrors.h"       // _PyErr_FormatFromCause
#include "internal/pycore_stackref.h"       // Py_TAG_BITS
#include "../../Python/remote_debug.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exception cause macro */
#define set_exception_cause(exc_type, message)                                        \
    do {                                                                              \
        assert(PyErr_Occurred() && "function returned -1 without setting exception"); \
        _set_debug_exception_cause(exc_type, message);                                \
    } while (0)


typedef struct {
    int i;
    PyTypeObject *GCMonitorHandler_Type;
    PyTypeObject *GCMonitorStatsItem_Type;
} GCMonitorState;

typedef struct {
    PyObject_HEAD
    proc_handle_t handle;
    uintptr_t runtime_start_address;
    struct _Py_DebugOffsets debug_offsets;

} GCMonitorHandler;


typedef struct {
    PyObject_HEAD
    PyTime_t ts;
    int gen;
    Py_ssize_t collections;
    Py_ssize_t collected;
    Py_ssize_t uncollectable;
    Py_ssize_t candidates;
    Py_ssize_t object_visits;
    Py_ssize_t objects_transitively_reachable;
    Py_ssize_t objects_not_transitively_reachable;
    Py_ssize_t heap_size;
    Py_ssize_t work_to_do;
    double duration;
    double total_duration;

} GCMonitorStatsItem;

#define GCMonitorHandler_CAST(op) ((GCMonitorHandler *)(op))
#define GCMonitorStatsItem_CAST(op) ((GCMonitorStatsItem *)(op))



GCMonitorState *
GCMonitor_GetState(PyObject *module)
{
    void *state = _PyModule_GetState(module);
    assert(state != NULL);
    return (GCMonitorState *)state;
}

GCMonitorState *
GCMonitor_GetStateFromType(PyTypeObject *type)
{
    PyObject *module = PyType_GetModule(type);
    assert(module != NULL);
    return GCMonitor_GetState(module);
}

// GCMonitorState *
// GCMonitor_GetStateFromObject(PyObject *obj)
// {
//     RemoteUnwinderObject *unwinder = (RemoteUnwinderObject *)obj;
//     if (unwinder->cached_state == NULL) {
//         unwinder->cached_state = GCMonitor_GetStateFromType(Py_TYPE(obj));
//     }
//     return unwinder->cached_state;
// }

// -- GC Monitor Handler --

static PyObject *
_gc_monitor_handler_read(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    GCMonitorState *st = GCMonitor_GetStateFromType(Py_TYPE(op));
    GCMonitorHandler *h = GCMonitorHandler_CAST(op);

    PyThreadState *tstate = _PyThreadState_GET();
    struct _gc_runtime_state *gcstate = &tstate->interp->gc;

    uintptr_t interpreter_state_list_head =
        (uintptr_t)h->debug_offsets.runtime_state.interpreters_head;

    uintptr_t address_of_interpreter_state;
    if (_Py_RemoteDebug_ReadRemoteMemory(
            &h->handle,
            h->runtime_start_address + interpreter_state_list_head,
            sizeof(void*),
            &address_of_interpreter_state) < 0) {
        set_exception_cause(PyExc_RuntimeError, "Failed to read interpreter state address");
        return NULL;
    }

    if (address_of_interpreter_state == 0) {
        PyErr_SetString(PyExc_RuntimeError, "No interpreter state found");
        return NULL;
    }

    struct gc_stats stats;
    uintptr_t address = address_of_interpreter_state
        + h->debug_offsets.interpreter_state.gc
        + h->debug_offsets.gc.generation_stats;
    if (_Py_RemoteDebug_ReadRemoteMemory(&h->handle,
                                         address,
                                         h->debug_offsets.gc.generation_stats_size,
                                         &stats) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to read GC state");
        return NULL;
    }

    PyObject *tuple = PyTuple_New(NUM_GENERATIONS * 11);
    if (tuple == NULL) {
        return NULL;
    }

    int index = 0;
    for(int gen = 0; gen < NUM_GENERATIONS; gen++) {
        struct gc_generation_stats_buffer *buffer = &stats.gen[gen];
        for(int i = 0; i < 11; i++, index++) {
            struct gc_generation_stats *stats_item = &buffer->items[i];
            GCMonitorStatsItem *item = PyObject_New(GCMonitorStatsItem, st->GCMonitorStatsItem_Type);
            if (item == NULL) {
                Py_DECREF(tuple);
                return NULL;
            }

            item->ts = stats_item->ts;
            item->gen = gen;
            item->collections = stats_item->collections;
            item->collected = stats_item->collected;
            item->uncollectable = stats_item->uncollectable;
            item->candidates = stats_item->candidates;
            item->object_visits = stats_item->object_visits;
            item->objects_transitively_reachable = stats_item->objects_transitively_reachable;
            item->objects_not_transitively_reachable = stats_item->objects_not_transitively_reachable;
            item->heap_size = stats_item->heap_size;
            item->work_to_do = stats_item->work_to_do;
            item->duration = stats_item->duration;
            item->total_duration = stats_item->total_duration;

            PyTuple_SET_ITEM(tuple, index, item);
        }
    }

    return tuple;
}

static PyObject *
_gc_monitor_handler_close(PyObject *op, PyObject *Py_UNUSED(ignored))
{
    GCMonitorHandler *self = GCMonitorHandler_CAST(op);
    if (self->handle.pid != 0) {
        _Py_RemoteDebug_ClearCache(&self->handle);
        _Py_RemoteDebug_CleanupProcHandle(&self->handle);
    }
    Py_RETURN_NONE;
}

static void
GCMonitorHandler_dealloc(PyObject *op)
{
    GCMonitorHandler *self = GCMonitorHandler_CAST(op);
    PyTypeObject *tp = Py_TYPE(self);

    PyObject *result = _gc_monitor_handler_close(op, NULL);
    assert(Py_IsNone(result));

    PyObject_Del(self);
    Py_DECREF(tp);
}

static PyMethodDef GCMonitorHandler_methods[] = {
    {"read", (PyCFunction)_gc_monitor_handler_read, METH_NOARGS, "read GC stats"},
    {"close", (PyCFunction)_gc_monitor_handler_close, METH_NOARGS, "close GC monitor handler"},
    {NULL, NULL}
};

static PyType_Slot GCMonitorHandler_slots[] = {
    {Py_tp_doc, (void *)"GC Monitor"},
    {Py_tp_methods, GCMonitorHandler_methods},
    {Py_tp_dealloc, GCMonitorHandler_dealloc},
    {0, NULL}
};

static PyType_Spec GCMonitorHandler_spec = {
    .name = "_gc_monitor.GCMonitorHandler",
    .basicsize = sizeof(GCMonitorHandler),
    .flags = (
        Py_TPFLAGS_DEFAULT
        | Py_TPFLAGS_IMMUTABLETYPE
    ),
    .slots = GCMonitorHandler_slots,
};

// -- GC Monitor Stats Item --

static PyMemberDef GCMonitorStatsItem_members[] = {
    {"ts", Py_T_LONGLONG, offsetof(GCMonitorStatsItem, ts), Py_READONLY},
    {"gen", Py_T_LONG, offsetof(GCMonitorStatsItem, gen), Py_READONLY},
    {"collections", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, collections), Py_READONLY},
    {"collected", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, collected), Py_READONLY},
    {"uncollectable", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, uncollectable), Py_READONLY},
    {"candidates", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, candidates), Py_READONLY},
    {"object_visits", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, object_visits), Py_READONLY},
    {"objects_transitively_reachable", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, objects_transitively_reachable), Py_READONLY},
    {"objects_not_transitively_reachable", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, objects_not_transitively_reachable), Py_READONLY},
    {"heap_size", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, heap_size), Py_READONLY},
    {"work_to_do", Py_T_PYSSIZET, offsetof(GCMonitorStatsItem, work_to_do), Py_READONLY},
    {"duration", Py_T_DOUBLE, offsetof(GCMonitorStatsItem, duration), Py_READONLY},
    {"total_duration", Py_T_DOUBLE, offsetof(GCMonitorStatsItem, total_duration), Py_READONLY},
    {NULL}
};

static PyType_Slot GCMonitorStatsItem_slots[] = {
    {Py_tp_doc, (void *)"GC Monitor Stats"},
    {Py_tp_members, GCMonitorStatsItem_members},
    {0, NULL}
};

static PyType_Spec GCMonitorStatsItem_spec = {
    .name = "_gc_monitor.GCMonitorStatsItem",
    .basicsize = sizeof(GCMonitorStatsItem),
    .flags = (
        Py_TPFLAGS_DEFAULT
        | Py_TPFLAGS_IMMUTABLETYPE
    ),
    .slots = GCMonitorStatsItem_slots,
};

// -- TOP LEVEL --

int
is_prerelease_version(uint64_t version)
{
    return (version & 0xF0) != 0xF0;
}

int
validate_debug_offsets(struct _Py_DebugOffsets *debug_offsets)
{
    if (memcmp(debug_offsets->cookie, _Py_Debug_Cookie, sizeof(debug_offsets->cookie)) != 0) {
        // The remote is probably running a Python version predating debug offsets.
        PyErr_SetString(
            PyExc_RuntimeError,
            "Can't determine the Python version of the remote process");
        return -1;
    }

    // Assume debug offsets could change from one pre-release version to another,
    // or one minor version to another, but are stable across patch versions.
    if (is_prerelease_version(Py_Version) && Py_Version != debug_offsets->version) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Can't attach from a pre-release Python interpreter"
            " to a process running a different Python version");
        return -1;
    }

    if (is_prerelease_version(debug_offsets->version) && Py_Version != debug_offsets->version) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Can't attach to a pre-release Python interpreter"
            " from a process running a different Python version");
        return -1;
    }

    unsigned int remote_major = (debug_offsets->version >> 24) & 0xFF;
    unsigned int remote_minor = (debug_offsets->version >> 16) & 0xFF;

    if (PY_MAJOR_VERSION != remote_major || PY_MINOR_VERSION != remote_minor) {
        PyErr_Format(
            PyExc_RuntimeError,
            "Can't attach from a Python %d.%d process to a Python %d.%d process",
            PY_MAJOR_VERSION, PY_MINOR_VERSION, remote_major, remote_minor);
        return -1;
    }

    // The debug offsets differ between free threaded and non-free threaded builds.
    if (_Py_Debug_Free_Threaded && !debug_offsets->free_threaded) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Cannot attach from a free-threaded Python process"
            " to a process running a non-free-threaded version");
        return -1;
    }

    if (!_Py_Debug_Free_Threaded && debug_offsets->free_threaded) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Cannot attach to a free-threaded Python process"
            " from a process running a non-free-threaded version");
        return -1;
    }

    return 0;
}


static PyObject *
_gc_monitor_connect(PyObject *module, PyObject *obj)
{
    int pid = PyLong_AsInt(obj);
    if (pid == -1 && PyErr_Occurred()) {
        return NULL;
    }

    GCMonitorState *st = GCMonitor_GetState(module);
    GCMonitorHandler *h = PyObject_New(GCMonitorHandler, st->GCMonitorHandler_Type);
    if (h == NULL) {
        return NULL;
    }

    if (_Py_RemoteDebug_InitProcHandle(&h->handle, pid) < 0) {
        set_exception_cause(PyExc_RuntimeError, "Failed to initialize process handle");
        Py_DECREF(h);
        return NULL;
    }

    h->runtime_start_address = _Py_RemoteDebug_GetPyRuntimeAddress(&h->handle);
    if (h->runtime_start_address == 0) {
        set_exception_cause(PyExc_RuntimeError, "Failed to get Python runtime address");
        Py_DECREF(h);
        return NULL;
    }

    if (_Py_RemoteDebug_ReadDebugOffsets(&h->handle,
                                         &h->runtime_start_address,
                                         &h->debug_offsets) < 0)
    {
        set_exception_cause(PyExc_RuntimeError, "Failed to read debug offsets");
        Py_DECREF(h);
        return NULL;
    }

    // Validate that the debug offsets are valid
    if (validate_debug_offsets(&h->debug_offsets) == -1) {
        set_exception_cause(PyExc_RuntimeError, "Invalid debug offsets found");
        Py_DECREF(h);
        return NULL;
    }

    return (PyObject *)h;
}

static PyObject *
_gc_monitor_disconnect(PyObject *module, PyObject *obj)
{
    GCMonitorState *st = GCMonitor_GetState(module);
    if (Py_IS_TYPE(obj, st->GCMonitorHandler_Type)) {
        PyObject *result = _gc_monitor_handler_close(obj, NULL);
        assert(Py_IsNone(result));
    }

    Py_RETURN_NONE;
}

// -- MODULE --

int
GCMonitor_InitState(GCMonitorState *st)
{
    return 0;
}

static int
_gc_monitor_exec(PyObject *m)
{
    GCMonitorState *st = GCMonitor_GetState(m);
#define CREATE_TYPE(mod, type, spec)                                        \
    do {                                                                    \
        type = (PyTypeObject *)PyType_FromMetaclass(NULL, mod, spec, NULL); \
        if (type == NULL) {                                                 \
            return -1;                                                      \
        }                                                                   \
    } while (0)

    CREATE_TYPE(m, st->GCMonitorHandler_Type, &GCMonitorHandler_spec);
    if (PyModule_AddType(m, st->GCMonitorHandler_Type) < 0) {
        return -1;
    }

    CREATE_TYPE(m, st->GCMonitorStatsItem_Type, &GCMonitorStatsItem_spec);
    if (PyModule_AddType(m, st->GCMonitorStatsItem_Type) < 0) {
        return -1;
    }

    if (GCMonitor_InitState(st) < 0) {
        return -1;
    }
    return 0;
}

static int
gc_monitor_traverse(PyObject *mod, visitproc visit, void *arg)
{
    GCMonitorState *st = GCMonitor_GetState(mod);
    Py_VISIT(st->GCMonitorHandler_Type);
    Py_VISIT(st->GCMonitorStatsItem_Type);
    return 0;
}

static int
gc_monitor_clear(PyObject *mod)
{
    GCMonitorState *st = GCMonitor_GetState(mod);
    Py_CLEAR(st->GCMonitorHandler_Type);
    Py_CLEAR(st->GCMonitorStatsItem_Type);
    return 0;
}

static void
gc_monitor_free(void *mod)
{
    (void)gc_monitor_clear((PyObject *)mod);
}

static PyMethodDef gc_monitor_methods[] = {
    {"connect", (PyCFunction)_gc_monitor_connect, METH_O, "gc_monitor.connect"},
    {"disconnect", (PyCFunction)_gc_monitor_disconnect, METH_O, "gc_monitor.disconnect"},
    {NULL, NULL, 0, NULL},
};

static PyModuleDef_Slot gc_monitor_slots[] = {
    {Py_mod_exec, _gc_monitor_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL},
};

static struct PyModuleDef gc_monitor_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_gc_monitor",
    .m_size = sizeof(GCMonitorState),
    .m_methods = gc_monitor_methods,
    .m_slots = gc_monitor_slots,
    .m_traverse = gc_monitor_traverse,
    .m_clear = gc_monitor_clear,
    .m_free = gc_monitor_free,
};

PyMODINIT_FUNC
PyInit__gc_monitor(void)
{
    return PyModuleDef_Init(&gc_monitor_module);
}
