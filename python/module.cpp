#include <nanobind/nanobind.h>

namespace nb = nanobind;

NB_MODULE(_native, m) {
    m.doc() = "Simucube API native bindings";
    m.attr("__version__") = SC_API_VERSION;
}
