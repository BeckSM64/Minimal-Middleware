
#include "MMW.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

struct Options
{
    std::string host = "127.0.0.1";
    unsigned short port = 5000;
    unsigned int clients = 1000;
    unsigned int batchSize = 50;
    unsigned int publishers = 1;
    unsigned int subscribers = 1;
    unsigned int delayMs = 100;
    unsigned int duration = 10;
};

static void OnMessage(const char*, const char*)
{
}

static void PrintUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --host IP          Broker host (default 127.0.0.1)\n"
        << "  --port PORT        Broker TCP port (default 5000)\n"
        << "  --clients N        Logical clients to create (default 1000)\n"
        << "  --batch-size N     Clients added per batch (default 50)\n"
        << "  --publishers N     Publishers per client (default 1)\n"
        << "  --subscribers N    Subscribers per client (default 1)\n"
        << "  --delay-ms N       Delay between batches (default 100)\n"
        << "  --duration N       Seconds to hold connections (default 10)\n";
}

static bool ParseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            return false;
        }

        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for " << arg << '\n';
            return false;
        }

        const std::string value = argv[++i];

        try
        {
            if (arg == "--host")
                options.host = value;
            else if (arg == "--port")
            {
                const unsigned long n = std::stoul(value);
                if (n == 0 || n > 65535)
                    throw std::out_of_range("port");
                options.port = static_cast<unsigned short>(n);
            }
            else if (arg == "--clients")
                options.clients = std::stoul(value);
            else if (arg == "--batch-size")
                options.batchSize = std::stoul(value);
            else if (arg == "--publishers")
                options.publishers = std::stoul(value);
            else if (arg == "--subscribers")
                options.subscribers = std::stoul(value);
            else if (arg == "--delay-ms")
                options.delayMs = std::stoul(value);
            else if (arg == "--duration")
                options.duration = std::stoul(value);
            else
            {
                std::cerr << "Unknown option: " << arg << '\n';
                return false;
            }
        }
        catch (const std::exception&)
        {
            std::cerr << "Invalid value for " << arg << ": "
                      << value << '\n';
            return false;
        }
    }

    if (options.clients == 0 || options.batchSize == 0 ||
        (options.publishers == 0 && options.subscribers == 0))
    {
        std::cerr << "Clients and batch size must be positive, and at "
                     "least one publisher or subscriber is required.\n";
        return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    Options options;

    if (!ParseOptions(argc, argv, options))
        return argc > 1 &&
               (std::string(argv[1]) == "--help" ||
                std::string(argv[1]) == "-h") ? 0 : 1;

    mmw_set_log_level(MMW_LOG_LEVEL_ERROR);

    if (mmw_initialize(options.host.c_str(), options.port, MMW_TRANSPORT_TCP) != MMW_OK)
    {
        std::cerr << "Failed to initialize MMW.\n";
        return 1;
    }

    const unsigned long long connectionsPerClient =
        static_cast<unsigned long long>(options.publishers) +
        options.subscribers;

    std::cout << "MMW scaling test\n"
              << "Broker: " << options.host << ':' << options.port << '\n'
              << "Target clients: " << options.clients << '\n'
              << "Connections per client: " << connectionsPerClient << '\n'
              << "Target broker connections: "
              << connectionsPerClient * options.clients << "\n\n";

    unsigned int successfulClients = 0;
    unsigned int failedClients = 0;
    unsigned long long successfulConnections = 0;
    unsigned long long failedConnections = 0;

    auto testStart = std::chrono::steady_clock::now();

    for (unsigned int base = 0; base < options.clients;)
    {
        const unsigned int batchEnd =
            base + (options.clients - base < options.batchSize
                ? options.clients - base
                : options.batchSize);

        auto batchStart = std::chrono::steady_clock::now();
        unsigned long long batchSuccess = 0;
        unsigned long long batchFailure = 0;

        for (unsigned int client = base; client < batchEnd; ++client)
        {
            bool clientSucceeded = true;

            for (unsigned int p = 0; p < options.publishers; ++p)
            {
                const std::string topic =
                    "scale/client/" + std::to_string(client) +
                    "/publisher/" + std::to_string(p);

                if (mmw_create_publisher(topic.c_str()) == MMW_OK)
                {
                    ++successfulConnections;
                    ++batchSuccess;
                }
                else
                {
                    ++failedConnections;
                    ++batchFailure;
                    clientSucceeded = false;
                }
            }

            for (unsigned int s = 0; s < options.subscribers; ++s)
            {
                const std::string topic =
                    "scale/client/" + std::to_string(client) +
                    "/subscriber/" + std::to_string(s);

                if (mmw_create_subscriber(topic.c_str(), OnMessage) == MMW_OK)
                {
                    ++successfulConnections;
                    ++batchSuccess;
                }
                else
                {
                    ++failedConnections;
                    ++batchFailure;
                    clientSucceeded = false;
                }
            }

            if (clientSucceeded)
                ++successfulClients;
            else
                ++failedClients;
        }

        const auto batchEndTime = std::chrono::steady_clock::now();
        const double batchSeconds =
            std::chrono::duration<double>(batchEndTime - batchStart).count();

        std::cout << "Clients: " << batchEnd << '/' << options.clients
                  << " | Connections: " << successfulConnections
                  << " succeeded, " << failedConnections << " failed"
                  << " | Batch: +" << batchSuccess << " connected, "
                  << batchFailure << " failed"
                  << " | Time: " << batchSeconds << " s\n"
                  << std::flush;

        base = batchEnd;

        if (base < options.clients && options.delayMs > 0)
            std::this_thread::sleep_for(
                std::chrono::milliseconds(options.delayMs));
    }

    const auto connectedAt = std::chrono::steady_clock::now();
    const double connectSeconds =
        std::chrono::duration<double>(connectedAt - testStart).count();

    std::cout << "\nConnection phase complete\n"
              << "Successful clients: " << successfulClients << '\n'
              << "Clients with failures: " << failedClients << '\n'
              << "Successful connections: " << successfulConnections << '\n'
              << "Failed connections: " << failedConnections << '\n'
              << "Connection phase time: " << connectSeconds << " s\n"
              << "Holding connections for " << options.duration
              << " seconds...\n" << std::flush;

    std::this_thread::sleep_for(std::chrono::seconds(options.duration));

    std::cout << "Cleaning up MMW connections...\n" << std::flush;
    mmw_cleanup();

    std::cout << "Test finished.\n";
    return failedConnections == 0 ? 0 : 1;
}
