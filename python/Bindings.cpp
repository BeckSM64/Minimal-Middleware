#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <string>
#include "MMW.h"

namespace py = pybind11;

// Forward declaration
class PySubscriber;

// Map of topic -> PySubscriber instance
static std::map<std::string, PySubscriber*> g_instanceMap;

// Subscriber trampoline called by C++ library
extern "C" void subscriber_trampoline(const char* topic, const char* message);

// Python-facing subscriber class
class PySubscriber {
public:
    PySubscriber(const std::string& topic_, py::function callback_)
        : topic(topic_), callback(callback_), running(true)
    {
        // Register in global map
        g_instanceMap[topic] = this;

        // Start Python worker thread
        workerThread = std::thread(&PySubscriber::processQueue, this);

        // Create C++ subscriber
        mmw_create_subscriber(topic.c_str(), &subscriber_trampoline);
    }

    ~PySubscriber() {
        running = false;
        cv.notify_all();
        if (workerThread.joinable())
            workerThread.join();

        // Remove from global map
        g_instanceMap.erase(topic);

        // Optional: clean up library if desired
        // mmw_cleanup();  // Don't do this if you have multiple subscribers
    }

    // Enqueue message from C++ subscriber thread
    void enqueueMessage(const std::string& msg) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            msgQueue.push(msg);
        }
        cv.notify_one();
    }

private:
    std::string topic;
    py::function callback;
    std::thread workerThread;
    std::atomic<bool> running;

    std::mutex queueMutex;
    std::condition_variable cv;
    std::queue<std::string> msgQueue;

    void processQueue() {
        while (running) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this] { return !msgQueue.empty() || !running; });

            while (!msgQueue.empty()) {
                std::string msg = std::move(msgQueue.front());
                msgQueue.pop();
                lock.unlock();

                try {
                    py::gil_scoped_acquire gil;
                    callback(topic, msg);
                } catch (const std::exception &e) {
                    py::print("[PySubscriber] Exception in callback:", e.what());
                }

                lock.lock();
            }
        }
    }

    friend void subscriber_trampoline(const char* topic, const char* message);
};

// Trampoline called by C++ library
extern "C" void subscriber_trampoline(const char* topic, const char* message) {
    auto it = g_instanceMap.find(topic);
    if (it != g_instanceMap.end()) {
        it->second->enqueueMessage(message ? message : "");
    }
}

// PYBIND11 MODULE
PYBIND11_MODULE(mmw, m) {
    m.doc() = "Python bindings for Minimal Middleware";

    // Enums
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
        .value("MMW_LOG_LEVEL_TRACE", MMW_LOG_LEVEL_TRACE)
        .export_values();

    // Core API
    m.def("initialize", &mmw_initialize, py::arg("broker_ip"), py::arg("port"));
    m.def("create_publisher", &mmw_create_publisher, py::arg("topic"));
    m.def("publish", &mmw_publish, py::arg("topic"), py::arg("message"), py::arg("reliability"));
    m.def("set_log_level", &mmw_set_log_level, py::arg("level"));
    m.def("delete_publisher", &mmw_delete_publisher, py::arg("topic"));
    m.def("delete_subscriber", &mmw_delete_subscriber, py::arg("topic"));
    m.def("cleanup", &mmw_cleanup);

    // Subscriber wrapper
    py::class_<PySubscriber>(m, "create_subscriber")
        .def(py::init<const std::string&, py::function>(),
             py::arg("topic"), py::arg("callback"));
}
