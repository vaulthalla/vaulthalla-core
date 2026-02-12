#pragma once

#include <ctime>

namespace vh::types {

struct FileActivity {
    enum class Activity { CREATED, MODIFIED, DELETED };

    unsigned int id, file_id, user_id;
    Activity activity = Activity::CREATED;
    std::time_t timestamp;
};

}
