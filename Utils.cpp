#include "Utils.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace SagaStats
{
Data::Data(const std::vector<char>& data)
    : data_(data)
{
}

void Data::skip(int n)
{
    ensureEnough(n);
    itr_ += n;
}

void Data::seek(int n)
{
    itr_ = 0;
    ensureEnough(n);
    itr_ = n;
}

std::string Data::getString(size_t len)
{
    ensureEnough(len);
    std::string res{data_.data() + itr_, len};
    skip(len);
    return res;
}
void Data::ensureEnough(size_t n)
{
    if (n >= data_.size() || itr_ + n >= data_.size()) {
        throw std::runtime_error("Out of bounds");
    }
}
bool Config::Load(const std::string& fileName)
{
    std::ifstream f("config.json");
    if (f.bad()) {
        return false;
    }

    nlohmann::json json = nlohmann::json::parse(f);
    if (json.contains("use_console")) {
        UseConsole = json["use_console"];
    }

    if (json.contains("log_level")) {
        LogLevel = spdlog::level::from_str(json["log_level"]);
    }
    return true;
}
} // namespace SagaStats