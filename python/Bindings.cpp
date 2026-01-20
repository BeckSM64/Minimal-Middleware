#include <pybind11/pybind11.h>
#include "MMW.h"

#include <unordered_map>
#include <string>
#include <mutex>

namespace py = pybind11;

// Map topic -> Python callback
static std::unordered_map<std::string, py::function> g_callbacks;
static std::mutex g_callbacks_mutex;

/*
 * C trampoline — EXACT signature expected by mmw_create_subscriber
 */
extern "C" void subscriber_trampoline(const char* topic, const char* message) {
    py::gil_scoped_acquire gil;

    std::lock_guard<std::mutex> lock(g_callbacks_mutex);
    auto it = g_callbacks.find(topic);
    if (it != g_callbacks.end()) {
        it->second(topic, message);
    }
}

/*
 * Python-facing Subscriber object
 */
class PySubscriber {
public:
    PySubscriber(const std::string& topic, py::function callback)
        : topic_(topic)
    {
        {
            std::lock_guard<std::mutex> lock(g_callbacks_mutex);
            g_callbacks[topic_] = std::move(callback);
        }

        mmw_create_subscriber(topic_.c_str(), &subscriber_trampoline);
    }

    ~PySubscriber() {
        std::lock_guard<std::mutex> lock(g_callbacks_mutex);
        g_callbacks.erase(topic_);
        // NOTE: mmw_cleanup() should usually be global, not per-subscriber
    }

private:
    std::string topic_;
};

PYBIND11_MODULE(mmw_python, m) {
    m.doc() = "Python bindings for Minimal Middleware";

    py::enum_<MmwResult>(m, "MmwResult")
        .value("MMW_OK", MMW_OK)
        .value("MMW_ERROR", MMW_ERROR)
        .export_values();

    py::enum_<MmwReliability>(m, "MmwReliability")
        .value("MMW_BEST_EFFORT", MMW_BEST_EFFORT)
        .value("MMW_RELIABLE", MMW_RELIABLE)
        .export_values();

    py::enum_<MmwLogLevel>(m, "MmwLogLevel")
        .value("MMW_LOG_LEVEL_OFF", MMW_LOG_LEVEL_OFF)
        .value("MMW_LOG_LEVEL_ERROR", MMW_LOG_LEVEL_ERROR)
        .value("MMW_LOG_LEVEL_WARN", MMW_LOG_LEVEL_WARN)
        .value("MMW_LOG_LEVEL_INFO", MMW_LOG_LEVEL_INFO)
        .value("MMW_LOG_LEVEL_DEBUG", MMW_LOG_LEVEL_DEBUG)
        .export_values();

    m.def("initialize", &mmw_initialize,
          py::arg("broker_ip"), py::arg("port"));

    m.def("create_publisher", &mmw_create_publisher,
          py::arg("topic"));

    m.def("publish", &mmw_publish,
          py::arg("topic"), py::arg("message"), py::arg("reliability"));

    m.def("publish_raw", &mmw_publish_raw,
          py::arg("topic"), py::arg("payload"),
          py::arg("size"), py::arg("reliability"));

    m.def("set_log_level", &mmw_set_log_level,
          py::arg("level"));

    m.def("cleanup", &mmw_cleanup);

    py::class_<PySubscriber>(m, "Subscriber")
        .def(py::init<const std::string&, py::function>(),
             py::arg("topic"), py::arg("callback"));
}
