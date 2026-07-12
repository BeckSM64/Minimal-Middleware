#pragma once
#include <memory>
#include <cstdint>
#include "ITransport.h"

class IListener {
public:
    virtual ~IListener() = default;

    virtual bool listen(uint16_t port) = 0;

    virtual std::shared_ptr<ITransport> accept() = 0;

    virtual void close() = 0;
};
