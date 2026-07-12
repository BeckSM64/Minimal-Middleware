#pragma once
#include <memory>
#include <cstdint>
#include "ITransport.h"

class IListener {
public:
    virtual ~IListener() = default;

    virtual bool Listen(uint16_t port) = 0;

    virtual std::shared_ptr<ITransport> Accept() = 0;

    virtual void Close() = 0;
};
