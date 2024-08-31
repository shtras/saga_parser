#pragma once

#include <spdlog/spdlog.h>

#include <vector>
#include <string>
#include <stdexcept>
#include <map>

namespace SagaStats
{

class Data
{
public:
    explicit Data(const std::vector<char>& data);

    void skip(int n);

    void seek(int n);

    std::string getString(size_t len);

    template <typename T>
    T get()
    {
        ensureEnough(sizeof(T));
        T res = *(T*)(data_.data() + itr_);
        skip(sizeof(T));
        return res;
    }

private:
    void ensureEnough(size_t n);

    const std::vector<char>& data_;
    size_t itr_ = 0;
};

struct StitchCount
{
    static std::array<std::string_view, 13> stitchNames;
    static std::array<float, 13> crossMultiplier;
    std::array<float, 13> stitches = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float adjustedDecorative = 0;
    int decorative = 0;
    float total() const;
    std::string totalStr() const;

    StitchCount& operator+=(const StitchCount& other)
    {
        for (int i = 0; i < stitches.size(); ++i) {
            stitches[i] += other.stitches[i];
        }
        adjustedDecorative += other.adjustedDecorative;
        decorative += other.decorative;
        return *this;
    }
};

struct Stats
{
    struct StatPeriod
    {
        StitchCount stitches;
        std::map<std::wstring, float> sets;
    };
    struct Month
    {
        StatPeriod stat;
        std::map<std::wstring, std::map<int, StatPeriod>> setByDay;
    };
    struct Year
    {
        std::map<int, Month> months;
        StatPeriod stat;
    };

    void Add(int year, int month, int day, const StitchCount& stitchCount, const std::wstring& setName);

    std::map<int, Year> years;
    StatPeriod stat;
    std::vector<std::wstring> errors;
};

struct Config
{
    bool Load(const std::string& fileName);
    void Save(const std::string& fileName);
    bool UseConsole = false;
    spdlog::level::level_enum LogLevel{spdlog::level::info};
    int SkipAfter = 0;
};


} // namespace SagaStats
