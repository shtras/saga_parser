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
        throw std::runtime_error("Unimplemented");
    }

    template <>
    long long get<long long>()
    {
        ensureEnough(sizeof(long long));
        int res = *(long long*)(data_.data() + itr_);
        skip(sizeof(long long));
        return res;
    }

    template <>
    int get<int>()
    {
        ensureEnough(sizeof(int));
        int res = *(int*)(data_.data() + itr_);
        skip(sizeof(int));
        return res;
    }

    template <>
    short get<short>()
    {
        ensureEnough(sizeof(short));
        int res = *(short*)(data_.data() + itr_);
        skip(sizeof(short));
        return res;
    }

    template <>
    char get<char>()
    {
        ensureEnough(1);
        int res = data_[itr_];
        skip(1);
        return res;
    }

private:
    void ensureEnough(size_t n);

    const std::vector<char>& data_;
    size_t itr_ = 0;
};

struct Stats
{
    struct StatPeriod
    {
        int numStitches = 0;
        std::map<std::wstring, int> sets;
    };
    struct Month
    {
        StatPeriod stat;
    };
    struct Year
    {
        std::map<int, Month> months;
        StatPeriod stat;
    };

    void Add(int year, int month, int numStitches, const std::wstring& setName)
    {
        if (numStitches == 0) {
            return;
        }
        stat.numStitches += numStitches;
        years[year].stat.numStitches += numStitches;
        years[year].months[month].stat.numStitches += numStitches;

        if (!setName.empty()) {
            stat.sets[setName] += numStitches;
            years[year].stat.sets[setName] += numStitches;
            years[year].months[month].stat.sets[setName] += numStitches;
        }
    }

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
};


} // namespace SagaStats
