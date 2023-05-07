#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/sinks/ringbuffer_sink.h"

#include <memory>

/* Global logging objects. */
namespace Log
{
extern std::shared_ptr<spdlog::logger> log;
extern std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> buffer;
}
