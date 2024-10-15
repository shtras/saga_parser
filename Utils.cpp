#include "Utils.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <numeric>

namespace SagaStats
{
std::array<std::string_view, 13> StitchCount::stitchNames = {
    "Cross-Stitch",    "Half x-stitch", "Petite x-stitch",    "Quarter x-stitches",
    "Unknown3",        "Half x-stitch", "French knot",        "Bead",
    "Oblong x-stitch", "Unknown7",      "Quarter x-stitches", "Back-Sitch",
    "Long stitch"
};

std::array<float, 13> StitchCount::crossMultiplier = {1.f, 0.5f, 1.f, 0.25f, 1.f,  0.5f, 1.f,
                                                      1.f, 1.f,  1.f, 1.f,   0.5f, 0.5f};

float StitchCount::total() const
{
    return std::accumulate(stitches.cbegin(), stitches.cend(), 0.f) + adjustedDecorative;
}

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

void Config::Save(const std::string& fileName)
{
    nlohmann::json json;
    json["log_level"] = fmt::to_string(spdlog::level::to_string_view(LogLevel));
    json["use_console"] = UseConsole;
    json["skip_after"] = SkipAfter;
    std::ofstream f("config.json");
    f << json;
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
    if (json.contains("skip_after")) {
        SkipAfter = json["skip_after"];
    }
    return true;
}
void Stats::Add(
    int year, int month, int day, const StitchCount& stitchCount, const std::wstring& setName
)
{
    auto numStitches = stitchCount.total();
    if (numStitches == 0) {
        return;
    }
    stat.stitches += stitchCount;
    years[year].stat.stitches += stitchCount;
    years[year].months[month].stat.stitches += stitchCount;

    if (!setName.empty()) {
        stat.sets[setName] += numStitches;
        years[year].stat.sets[setName] += numStitches;
        years[year].months[month].stat.sets[setName] += numStitches;
        if (years[year].months[month].setByDay[setName].count(day) > 0) {
            spdlog::error(
                L"Multiple entries for the same day: {} {}-{}-{}", setName, year, month, day
            );
        }
        years[year].months[month].setByDay[setName][day].stitches = stitchCount;
    }
}
} // namespace SagaStats