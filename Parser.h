#pragma once
#include "Utils.h"

#include <filesystem>

namespace SagaStats
{
class Parser
{
public:
    explicit Parser(const Config& config)
        : config_(config)
    {
    }
    void ParseFile(const std::filesystem::path& pathStr);
    void ParseDir(const std::filesystem::path& pathStr);

    const Stats& GetStats() const;

private:
    void processFileData(const std::vector<char>& v, const std::wstring& setName);
    void processFileLegacy(const std::vector<char>& v);

    Stats stats_;
    const Config& config_;
};
} // namespace SagaStats
