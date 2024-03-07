#pragma once
#include "Utils.h"

#include <filesystem>

namespace SagaStats
{
class Parser
{
public:
    explicit Parser(Stats& stats);
    void Parse(const std::filesystem::path& path);

    const Stats& GetStats() const;

private:
    void processFileData(const std::vector<char>& v);
    void processFileLegacy(const std::vector<char>& v);

    Stats& stats_;
};
} // namespace SagaStats
