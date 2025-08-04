#pragma once

#include <string>
#include <chrono>
#include <sstream>

namespace vh::util {

inline std::chrono::seconds parsePostgresInterval(const std::string& s) {
    std::istringstream ss(s);
    int days = 0, hours = 0, minutes = 0, seconds = 0;
    char colon;

    if (s.find("day") != std::string::npos) {
        ss >> days;
        std::string tmp;
        ss >> tmp; // discard "day"
    }

    ss >> hours >> colon >> minutes >> colon >> seconds;

    return std::chrono::seconds(
        days * 86400 +
        hours * 3600 +
        minutes * 60 +
        seconds
    );
}

inline std::string intervalToString(const std::chrono::seconds& interval) {
    auto total_seconds = interval.count();
    const int days = static_cast<int>(total_seconds) / 86400;
    total_seconds %= 86400;
    const int hours = static_cast<int>(total_seconds) / 3600;
    total_seconds %= 3600;
    const int minutes = static_cast<int>(total_seconds) / 60;
    const int seconds = static_cast<int>(total_seconds) % 60;

    std::ostringstream oss;
    if (days > 0) oss << days << " day ";
    oss << hours << ":" << minutes << ":" << seconds;
    return oss.str();
}

}
