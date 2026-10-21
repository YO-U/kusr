#include "logging.h"

#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

namespace server_logging {

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

void InitFormatter() {
    namespace keywords = boost::log::keywords;
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        keywords::auto_flush = true,
        keywords::format = [](logging::record_view const& rec, logging::formatting_ostream& strm) {
            json::object obj;
            if (auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec)) {
                obj["timestamp"] = to_iso_extended_string(*ts);
            }
            if (auto data = logging::extract<json::value>("AdditionalData", rec)) {
                obj["data"] = *data;
            } else {
                obj["data"] = json::object{};
            }
            obj["message"] = rec[logging::expressions::smessage].get();
            strm << json::serialize(obj);
        });
}

void LogJson(logging::trivial::severity_level level, json::value data, std::string_view message) {
    BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),
                                 (::boost::log::keywords::severity = level))
        << logging::add_value(additional_data, std::move(data)) << message;
}

}  // namespace server_logging
