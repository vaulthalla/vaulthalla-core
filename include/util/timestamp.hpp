#pragma once

#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace vh::util {
    inline std::time_t parsePostgresTimestamp(const std::string& timestampStr) {
        std::tm tm = {};
        std::istringstream ss(timestampStr.substr(0, 19));  // truncate to "YYYY-MM-DD HH:MM:SS"
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (ss.fail()) {
            throw std::runtime_error("Failed to parse timestamp: " + timestampStr);
        }
        return timegm(&tm);  // returns UTC-based time_t
    }
}
