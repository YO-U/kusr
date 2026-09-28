#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <ctime>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t_c);
#else
        localtime_r(&t_c, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%F %T");
        return ss.str();
    }

    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t_c);
#else
        localtime_r(&t_c, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y_%m_%d");
        return ss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template <class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard lock(mutex_);

        const auto timestamp = GetTimeStamp();
        const auto file_timestamp = GetFileTimeStamp();
        const std::string filename = "/var/log/sample_log_" + file_timestamp + ".log";

        if (!log_file_.is_open() || current_file_ != filename) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_file_ = filename;
            log_file_.open(filename, std::ios::app);
        }

        if (log_file_.is_open()) {
            log_file_ << timestamp << ": ";
            (log_file_ << ... << args);
            log_file_ << std::endl;
        }
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    mutable std::mutex mutex_;
    std::ofstream log_file_;
    std::string current_file_;
};
