#include <pybind11/pybind11.h>
#include "MMW.h"

namespace py = pybind11;

// This will hold the Python callback globally per subscriber (simplest approach)
static py::function g_python_callback;

extern "C" void subscriber_trampoline(const char* msg) {
    py::gil_scoped_acquire acquire;  // Acquire GIL before calling Python
    if (g_python_callback) {
        g_python_callback(std::string(msg)); // Convert C string to Python str
    }
}

PYBIND11_MODULE(mmw_python, m) {
    m.doc() = "Python bindings for Minimal Middleware";

    py::enum_<MmwResult>(m, "MmwResult")
        .value("MMW_OK", MMW_OK)
        .value("MMW_ERROR", MMW_ERROR)
        .export_values();

    m.def("initialize", &mmw_initialize, py::arg("configPath") = nullptr);
    m.def("create_publisher", &mmw_create_publisher, py::arg("topic"));
    m.def("create_subscriber", &mmw_create_subscriber, py::arg("topic"), py::arg("callback"));
    m.def("create_subscriber_raw", &mmw_create_subscriber_raw, py::arg("topic"), py::arg("callback"));
    m.def("publish", &mmw_publish, py::arg("topic"), py::arg("message"));
    m.def("publish_raw", &mmw_publish_raw, py::arg("topic"), py::arg("message"), py::arg("size"));
    m.def("cleanup", &mmw_cleanup);

    m.def("create_subscriber", [](const char* topic, py::function callback) {
        g_python_callback = callback;
        mmw_create_subscriber(topic, &subscriber_trampoline);
    });
}
