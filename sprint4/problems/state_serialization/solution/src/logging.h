#pragma once

#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <string_view>

namespace server_logging {

namespace json = boost::json;
namespace logging = boost::log;

void InitFormatter();
void LogJson(logging::trivial::severity_level level, json::value data, std::string_view message);

inline void LogInfo(json::value data, std::string_view message) {
    LogJson(logging::trivial::info, std::move(data), message);
}

}  // namespace server_logging
