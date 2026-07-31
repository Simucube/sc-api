#include "bind_exceptions.h"

#include <nanobind/nanobind.h>

namespace nb                                  = nanobind;

static PyObject* SimucubeError_type           = nullptr;
static PyObject* StateError_type              = nullptr;
static PyObject* IncompatibleError_type       = nullptr;
static PyObject* BusyError_type               = nullptr;
static PyObject* InternalError_type           = nullptr;
static PyObject* SimucubeConnectionError_type = nullptr;

static PyObject* create_exception(nb::module_& m, const char* name, PyObject* base1, PyObject* base2 = nullptr) {
    PyObject* bases = nullptr;
    if (base2) {
        bases = PyTuple_Pack(2, base1, base2);
    }

    std::string qualname = std::string("simucube_api._native.") + name;
    PyObject*   exc      = PyErr_NewException(qualname.c_str(), base2 ? bases : base1, nullptr);
    if (bases) Py_DECREF(bases);

    m.attr(name) = nb::steal(exc);
    Py_INCREF(exc);
    return exc;
}

void bind_exceptions(nb::module_& m) {
    SimucubeError_type = create_exception(m, "SimucubeError", PyExc_Exception);
    {
        PyObject* doc = PyUnicode_FromString("Base class for all Simucube-specific errors.");
        PyObject_SetAttrString(SimucubeError_type, "__doc__", doc);
        Py_DECREF(doc);
    }

    StateError_type = create_exception(m, "StateError", SimucubeError_type, PyExc_RuntimeError);
    {
        PyObject* doc = PyUnicode_FromString(
            "Operation is invalid in the current session state (e.g. not registered for control).");
        PyObject_SetAttrString(StateError_type, "__doc__", doc);
        Py_DECREF(doc);
    }

    IncompatibleError_type = create_exception(m, "IncompatibleError", SimucubeError_type, PyExc_RuntimeError);
    {
        PyObject* doc = PyUnicode_FromString("Version mismatch between the API and Simucube Tuner.");
        PyObject_SetAttrString(IncompatibleError_type, "__doc__", doc);
        Py_DECREF(doc);
    }

    BusyError_type = create_exception(m, "BusyError", SimucubeError_type, PyExc_RuntimeError);
    {
        PyObject* doc = PyUnicode_FromString("Another controller currently holds the requested resource.");
        PyObject_SetAttrString(BusyError_type, "__doc__", doc);
        Py_DECREF(doc);
    }

    InternalError_type = create_exception(m, "InternalError", SimucubeError_type, PyExc_RuntimeError);
    {
        PyObject* doc = PyUnicode_FromString("Unexpected internal or protocol error.");
        PyObject_SetAttrString(InternalError_type, "__doc__", doc);
        Py_DECREF(doc);
    }

    SimucubeConnectionError_type =
        create_exception(m, "SimucubeConnectionError", SimucubeError_type, PyExc_ConnectionError);
    {
        PyObject* doc = PyUnicode_FromString("Cannot connect to Simucube Tuner (not running or IPC unavailable).");
        PyObject_SetAttrString(SimucubeConnectionError_type, "__doc__", doc);
        Py_DECREF(doc);
    }
}

void throw_internal_error(const char* msg) {
    PyErr_SetString(InternalError_type, msg);
    throw nb::python_error();
}

void throw_on_error(sc_api::ResultCode rc) {
    using RC = sc_api::ResultCode;
    if (rc == RC::ok) return;

    const char* msg      = nullptr;
    PyObject*   exc_type = nullptr;

    switch (rc) {
        case RC::error_invalid_argument:
            exc_type = PyExc_ValueError;
            msg      = "Invalid argument";
            break;
        case RC::error_invalid_format:
            exc_type = PyExc_ValueError;
            msg      = "Invalid format";
            break;
        case RC::error_not_supported:
            exc_type = PyExc_NotImplementedError;
            msg      = "Operation not supported";
            break;
        case RC::error_invalid_state:
            exc_type = StateError_type;
            msg      = "Invalid state";
            break;
        case RC::error_not_registered:
            exc_type = StateError_type;
            msg      = "Not registered";
            break;
        case RC::error_no_control:
            exc_type = StateError_type;
            msg      = "No control";
            break;
        case RC::error_invalid_session_state:
            exc_type = StateError_type;
            msg      = "Invalid session state";
            break;
        case RC::error_incompatible:
            exc_type = IncompatibleError_type;
            msg      = "Incompatible version";
            break;
        case RC::error_busy:
            exc_type = BusyError_type;
            msg      = "Resource busy";
            break;
        case RC::error_timeout:
            exc_type = PyExc_TimeoutError;
            msg      = "Operation timed out";
            break;
        case RC::error_cannot_connect:
            exc_type = SimucubeConnectionError_type;
            msg      = "Cannot connect to Simucube Tuner";
            break;
        case RC::error_internal:
            exc_type = InternalError_type;
            msg      = "Internal error";
            break;
        case RC::error_internal_comm_error:
            exc_type = InternalError_type;
            msg      = "Internal communication error";
            break;
        case RC::error_protocol:
            exc_type = InternalError_type;
            msg      = "Protocol error";
            break;
        default:
            exc_type = SimucubeError_type;
            msg      = "Unknown error";
            break;
    }

    PyErr_SetString(exc_type, msg);
    throw nb::python_error();
}
