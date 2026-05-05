#pragma once

// Set: identifier of a printing run shared by both supported games.
// JSON shape matches the original Rust Set struct exactly:
//   { "id": "...", "name": "...", "releaseDate": "YYYY/MM/DD" }

#include <nlohmann/json.hpp>

#include <string>

namespace ccm {

struct Set {
    std::string id;
    std::string name;
    std::string releaseDate;  // formatted as "YYYY/MM/DD"

    friend bool operator==(const Set&, const Set&) = default;
};

void to_json(nlohmann::json& j, const Set& s);
void from_json(const nlohmann::json& j, Set& s);

}  // namespace ccm
