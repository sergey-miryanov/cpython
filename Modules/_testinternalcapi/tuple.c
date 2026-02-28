#include "parts.h"

#include "pycore_tuple.h"


static PyObject *
_tuple_from_pair(PyObject *Py_UNUSED(module), PyObject *args)
{
    PyObject *one, *two;
    if (!PyArg_ParseTuple(args, "OO", &one, &two)) {
        return NULL;
    }

    return _PyTuple_FromPair(one, two);
}

static PyObject *
_tuple_from_pair_steal(PyObject *Py_UNUSED(module), PyObject *args)
{
    PyObject *one, *two;
    if (!PyArg_ParseTuple(args, "OO", &one, &two)) {
        return NULL;
    }

    return _PyTuple_FromPairSteal(Py_NewRef(one), Py_NewRef(two));
}


static PyObject *
bench_tuple_new_pair(PyObject *Py_UNUSED(module), PyObject *obj)
{
    Py_ssize_t loops = PyLong_AsSsize_t(obj);
    PyTime_t t1, t2;
    PyObject *result = NULL;
    PyObject *preuse = PyList_New(PyTuple_MAXSAVESIZE+1);
    if (preuse == NULL) {
        return NULL;
    }
    for(Py_ssize_t i = 0; i < PyTuple_MAXSAVESIZE+1; i++)
    {
        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            goto error;
        }
        PyList_SET_ITEM(preuse, i, tuple);
    }

    PyTime_PerfCounterRaw(&t1);
    for (Py_ssize_t i=0; i < loops; i++)
    {
        PyObject *one = PyLong_FromLong(100001);
        if (one == NULL) {
            goto error;
        }
        PyObject *two = PyLong_FromLong(100002);
        if (two == NULL) {
            Py_DECREF(one);
            goto error;
        }

        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            Py_DECREF(one);
            Py_DECREF(two);
            goto error;
        }

        PyTuple_SET_ITEM(tuple, 0, one);
        PyTuple_SET_ITEM(tuple, 1, two);

        Py_DECREF(tuple);
    }
    PyTime_PerfCounterRaw(&t2);
    result = PyFloat_FromDouble(PyTime_AsSecondsDouble(t2 - t1));

error:
    Py_DECREF(preuse);
    return result;
}

static PyObject *
bench_tuple_pack_pair(PyObject *Py_UNUSED(module), PyObject *obj)
{
    Py_ssize_t loops = PyLong_AsSsize_t(obj);
    PyTime_t t1, t2;
    PyObject *result = NULL;
    PyObject *preuse = PyList_New(PyTuple_MAXSAVESIZE+1);
    if (preuse == NULL) {
        return NULL;
    }
    for(Py_ssize_t i = 0; i < PyTuple_MAXSAVESIZE+1; i++)
    {
        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            goto error;
        }
        PyList_SET_ITEM(preuse, i, tuple);
    }

    PyTime_PerfCounterRaw(&t1);
    for (Py_ssize_t i=0; i < loops; i++)
    {
        PyObject *one = PyLong_FromLong(100001);
        if (one == NULL) {
            goto error;
        }
        PyObject *two = PyLong_FromLong(100002);
        if (two == NULL) {
            Py_DECREF(one);
            goto error;
        }

        PyObject *tuple = PyTuple_Pack(2, one, two);
        Py_DECREF(one);
        Py_DECREF(two);
        if (tuple == NULL) {
            goto error;
        }

        Py_DECREF(tuple);
    }
    PyTime_PerfCounterRaw(&t2);
    result = PyFloat_FromDouble(PyTime_AsSecondsDouble(t2 - t1));

error:
    Py_DECREF(preuse);
    return result;
}

static PyObject *
bench_tuple_from_array_pair(PyObject *Py_UNUSED(module), PyObject *obj)
{
    Py_ssize_t loops = PyLong_AsSsize_t(obj);
    PyTime_t t1, t2;
    PyObject *result = NULL;
    PyObject *preuse = PyList_New(PyTuple_MAXSAVESIZE+1);
    if (preuse == NULL) {
        return NULL;
    }
    for(Py_ssize_t i = 0; i < PyTuple_MAXSAVESIZE+1; i++)
    {
        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            goto error;
        }
        PyList_SET_ITEM(preuse, i, tuple);
    }

    PyTime_PerfCounterRaw(&t1);
    for (Py_ssize_t i=0; i < loops; i++)
    {
        PyObject *one = PyLong_FromLong(100001);
        if (one == NULL) {
            goto error;
        }
        PyObject *two = PyLong_FromLong(100002);
        if (two == NULL) {
            Py_DECREF(one);
            goto error;
        }

        PyObject *tuple = PyTuple_FromArray((PyObject*[]){one, two}, 2);
        Py_DECREF(one);
        Py_DECREF(two);
        if (tuple == NULL) {
            goto error;
        }

        Py_DECREF(tuple);
    }
    PyTime_PerfCounterRaw(&t2);
    result = PyFloat_FromDouble(PyTime_AsSecondsDouble(t2 - t1));

error:
    Py_DECREF(preuse);
    return result;
}

static PyObject *
bench_tuple_from_array_pair_steal(PyObject *Py_UNUSED(module), PyObject *obj)
{
    Py_ssize_t loops = PyLong_AsSsize_t(obj);
    PyTime_t t1, t2;
    PyObject *result = NULL;
    PyObject *preuse = PyList_New(PyTuple_MAXSAVESIZE+1);
    if (preuse == NULL) {
        return NULL;
    }
    for(Py_ssize_t i = 0; i < PyTuple_MAXSAVESIZE+1; i++)
    {
        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            goto error;
        }
        PyList_SET_ITEM(preuse, i, tuple);
    }

    PyTime_PerfCounterRaw(&t1);
    for (Py_ssize_t i=0; i < loops; i++)
    {
        PyObject *one = PyLong_FromLong(100001);
        if (one == NULL) {
            goto error;
        }
        PyObject *two = PyLong_FromLong(100002);
        if (two == NULL) {
            Py_DECREF(one);
            goto error;
        }

        PyObject *tuple = _PyTuple_FromArraySteal((PyObject*[]){one, two}, 2);
        if (tuple == NULL) {
            goto error;
        }

        Py_DECREF(tuple);
    }
    PyTime_PerfCounterRaw(&t2);
    result = PyFloat_FromDouble(PyTime_AsSecondsDouble(t2 - t1));

error:
    Py_DECREF(preuse);
    return result;
}

static PyObject *
bench_tuple_from_pair(PyObject *Py_UNUSED(module), PyObject *obj)
{
    Py_ssize_t loops = PyLong_AsSsize_t(obj);
    PyTime_t t1, t2;
    PyObject *result = NULL;
    PyObject *preuse = PyList_New(PyTuple_MAXSAVESIZE+1);
    if (preuse == NULL) {
        return NULL;
    }
    for(Py_ssize_t i = 0; i < PyTuple_MAXSAVESIZE+1; i++)
    {
        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            goto error;
        }
        PyList_SET_ITEM(preuse, i, tuple);
    }

    PyTime_PerfCounterRaw(&t1);
    for (Py_ssize_t i=0; i < loops; i++)
    {
        PyObject *one = PyLong_FromLong(100001);
        if (one == NULL) {
            goto error;
        }
        PyObject *two = PyLong_FromLong(100002);
        if (two == NULL) {
            Py_DECREF(one);
            goto error;
        }

        PyObject *tuple = _PyTuple_FromPair(one, two);
        Py_DECREF(one);
        Py_DECREF(two);
        if (tuple == NULL) {
            goto error;
        }

        Py_DECREF(tuple);
    }
    PyTime_PerfCounterRaw(&t2);
    result = PyFloat_FromDouble(PyTime_AsSecondsDouble(t2 - t1));

error:
    Py_DECREF(preuse);
    return result;
}

static PyObject *
bench_tuple_from_pair_steal(PyObject *Py_UNUSED(module), PyObject *obj)
{
    Py_ssize_t loops = PyLong_AsSsize_t(obj);
    PyTime_t t1, t2;
    PyObject *result = NULL;
    PyObject *preuse = PyList_New(PyTuple_MAXSAVESIZE+1);
    if (preuse == NULL) {
        return NULL;
    }
    for(Py_ssize_t i = 0; i < PyTuple_MAXSAVESIZE+1; i++)
    {
        PyObject *tuple = PyTuple_New(2);
        if (tuple == NULL) {
            goto error;
        }
        PyList_SET_ITEM(preuse, i, tuple);
    }

    PyTime_PerfCounterRaw(&t1);
    for (Py_ssize_t i=0; i < loops; i++)
    {
        PyObject *one = PyLong_FromLong(100001);
        if (one == NULL) {
            goto error;
        }
        PyObject *two = PyLong_FromLong(100002);
        if (two == NULL) {
            Py_DECREF(one);
            goto error;
        }

        PyObject *tuple = _PyTuple_FromPairSteal(one, two);
        if (tuple == NULL) {
            goto error;
        }

        Py_DECREF(tuple);
    }
    PyTime_PerfCounterRaw(&t2);
    result = PyFloat_FromDouble(PyTime_AsSecondsDouble(t2 - t1));

error:
    Py_DECREF(preuse);
    return result;
}

static PyMethodDef test_methods[] = {
    {"_tuple_from_pair", _tuple_from_pair, METH_VARARGS},
    {"_tuple_from_pair_steal", _tuple_from_pair_steal, METH_VARARGS},

    {"bench_tuple_new_pair", bench_tuple_new_pair, METH_O},
    {"bench_tuple_pack_pair", bench_tuple_pack_pair, METH_O},
    {"bench_tuple_from_array_pair", bench_tuple_from_array_pair, METH_O},
    {"bench_tuple_from_array_pair_steal", bench_tuple_from_array_pair_steal, METH_O},
    {"bench_tuple_from_pair", bench_tuple_from_pair, METH_O},
    {"bench_tuple_from_pair_steal", bench_tuple_from_pair_steal, METH_O},

    {NULL},
};

int
_PyTestInternalCapi_Init_Tuple(PyObject *m)
{
    if (PyModule_AddFunctions(m, test_methods) < 0) {
        return -1;
    }

    return 0;
}
