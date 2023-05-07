#include "log.h"
#include "spdlog/sinks/ostream_sink.h"

/* The ringbyffer sink and the global logging object. */
static constexpr size_t LOG_SIZE = 1024;
std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> Log::buffer =
    std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(LOG_SIZE);

std::shared_ptr<spdlog::logger> Log::log = std::make_shared<spdlog::logger>(
    "global", Log::buffer
);
