#include "Utils.h"
#include "Parser.h"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <fstream>

namespace fs = std::filesystem;

void scanDir(const std::string& pathStr)
{
    SagaStats::Stats stats;
    SagaStats::Parser parser(stats);

    fs::path dir(pathStr);
    fs::directory_iterator itr(dir);
    for (auto& e : itr) {
        auto& path = e.path();
        if (!fs::exists(path)) {
            spdlog::error("File not found");
            continue;
        }
        if (path.extension() != ".sp") {
            continue;
        }
        try {
            parser.Parse(path);
        } catch (std::runtime_error& e) {
            spdlog::error("Failed: {}", e.what());
            continue;
        }
    }

    for (auto& year : parser.GetStats().years) {
        spdlog::info("Stats for year: {}", year.first);
        for (auto& month : year.second.months) {
            spdlog::info("Month: {} Crosses: {}", month.first, month.second);
        }
    }
}

int main()
{
    SagaStats::Config config;
    auto res = config.Load("config.json");
    if (!res) {
        spdlog::warn("Failed to load config");
    }

    spdlog::set_level(config.LogLevel);

    //processFileNew("C:\\Projects\\crossstitchsaga\\data\\08789_Santa_Snowman_Ornaments20230924_24.09.2023_21-15.sp");
    //processFile("C:\\Projects\\crossstitchsaga\\data\\honey_20240301_04.03.2024_22-52.sp");
    //processFileNew("C:\\Projects\\crossstitchsaga\\data\\honey_20240301_04.03.2024_22-52.sp");

    //processFile("C:\\Projects\\crossstitchsaga\\data\\honey_20240302_05.03.2024_22-53.sp");
    //processFileNew("C:\\Projects\\crossstitchsaga\\data\\honey_20240302_05.03.2024_22-53.sp");

    scanDir("C:\\Projects\\crossstitchsaga\\data");
    //return 0;

    return 0;
}
