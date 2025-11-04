#include <pybind11/pybind11.h>
#include "MMW.h"

namespace py = pybind11;

static py::function g_python_callback;

extern "C" void subscriber_trampoline(const char* msg) {
    py::gil_scoped_acquire acquire;
    if (g_python_callback) {
        g_python_callback(std::string(msg));
    }
}

class PySubscriber {
public:
    PySubscriber(const std::string& topic, py::function callback)
        : callback(callback)
    {
        g_python_callback = callback;
        mmw_create_subscriber(topic.c_str(), &subscriber_trampoline);
    }

    ~PySubscriber() {
        mmw_cleanup();
    }

private:
    py::function callback;
};

PYBIND11_MODULE(mmw_python, m) {
    m.doc() = "Python bindings for Minimal Middleware";

    // MmwResult enum
    py::enum_<MmwResult>(m, "MmwResult")
        .value("MMW_OK", MMW_OK)
        .value("MMW_ERROR", MMW_ERROR)
        .export_values();

    // MmwReliability enum
    py::enum_<MmwReliability>(m, "MmwReliability")
        .value("MMW_BEST_EFFORT", MMW_BEST_EFFORT)
        .value("MMW_RELIABLE", MMW_RELIABLE)
        .export_values();

    // Functions
    m.def("initialize", &mmw_initialize, py::arg("brokerIp"), py::arg("port"));
    m.def("create_publisher", &mmw_create_publisher, py::arg("topic"));
    m.def("create_subscriber", &mmw_create_subscriber, py::arg("topic"), py::arg("callback"));
    m.def("create_subscriber_raw", &mmw_create_subscriber_raw, py::arg("topic"), py::arg("callback"));
    m.def("publish", &mmw_publish, py::arg("topic"), py::arg("message"), py::arg("reliability"));
    m.def("publish_raw", &mmw_publish_raw, py::arg("topic"), py::arg("message"), py::arg("size"), py::arg("reliability"));
    m.def("cleanup", &mmw_cleanup);

    // Python-friendly Subscriber wrapper
    py::class_<PySubscriber>(m, "Subscriber")
        .def(py::init<const std::string&, py::function>());
}
