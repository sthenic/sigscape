#pragma once

#define SPDLOG_FMT_EXTERNAL
#include "spdlog/spdlog.h"
#include "spdlog/sinks/ringbuffer_sink.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <memory>

/* Global logging objects. */
namespace Log
{
extern std::shared_ptr<spdlog::logger> log;
extern std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> buffer;
}
