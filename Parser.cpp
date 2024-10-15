#include "Parser.h"
#include "Utils.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>
#include <chrono>
#include <map>
#include <array>
#include <algorithm>
#include <numeric>

namespace SagaStats
{

namespace
{
struct Header
{
    explicit Header(Data& data)
    {
        payloadOffset = data.get<int>();
        data.skip(10);
        width = data.get<short>();
        height = data.get<short>();
        data.skip(89);
        sizeExtra = data.get<short>();
    }

    int payloadOffset = -1;
    short width = -1;
    short height = -1;
    short sizeExtra = -1;
};

struct Cross
{
    explicit Cross(Data& data)
    {
        x = data.get<short>();
        y = data.get<short>();
        d = data.get<char>();
    }
    short x = -1;
    short y = -1;
    char d = '?';
};

struct Date
{
    explicit Date(Data& data)
    {
        auto dateStr = data.getString(data.get<short>());
        data.skip(1);
        auto day = data.getString(data.get<short>());
        auto time1 = data.getString(data.get<short>());
        auto time2 = data.getString(data.get<short>());
        for (int i = 0; i < 11; ++i) {
            stitches.stitches[i] = -data.get<long long>() * StitchCount::crossMultiplier[i];
        }
        auto backStitch = data.get<float>();
        auto longStitch = data.get<float>();
        stitches.stitches[11] = -backStitch * StitchCount::crossMultiplier[11];
        stitches.stitches[12] = -longStitch * StitchCount::crossMultiplier[12];
        auto numDecorative = data.get<int>();
        for (int i = 0; i < numDecorative; ++i) {
            auto coeff = data.get<float>();
            auto numDec = data.get<long long>();
            stitches.adjustedDecorative -= numDec * coeff;
            stitches.decorative -= numDec;
        }
        //data.skip(extraDateFields * 12);

        std::istringstream iss(dateStr);
        iss >> std::chrono::parse(std::string{"%d.%m.%Y"}, date);
    }

    std::string toString() const
    {
        std::stringstream ss;
        ss << date << ": " << stitches.total();
        return ss.str();
    }

    std::chrono::year_month_day date;
    StitchCount stitches;
};
} // namespace

namespace fs = std::filesystem;

void Parser::ParseFile(const std::filesystem::path& path)
{
    if (!fs::exists(path)) {
        spdlog::error("File not found");
        stats_.errors.push_back(path.wstring());
        return;
    }
    if (path.extension() != ".sp") {
        stats_.errors.push_back(path.wstring());
        return;
    }
    std::ifstream f(path, std::ios_base::in | std::ios_base::binary);
    auto fileSize = fs::file_size(path);
    spdlog::trace("Size: {}", fileSize);

    std::vector<char> data(fileSize);
    f.read(data.data(), fileSize);
    std::wstring fileName;
    try {
        fileName = path.filename().wstring();
    } catch (std::system_error& e) {
    }
    try {
        processFileData(data, fileName);
    } catch (std::runtime_error& e) {
        spdlog::error("Failed: {}", e.what());
        stats_.errors.push_back(path.wstring());
        return;
    }
    spdlog::debug("Success");
}

void Parser::ParseDir(const std::filesystem::path& path)
{
    fs::path dir(path);
    fs::directory_iterator itr(dir);
    for (auto& e : itr) {
        auto& subPath = e.path();
        ParseFile(subPath);
    }
}

const Stats& Parser::GetStats() const
{
    return stats_;
}

void Parser::processFileData(const std::vector<char>& v, const std::wstring& setName)
{
    Data data(v);
    Header header(data);
    data.skip(header.sizeExtra + 14);

    auto numThreads = data.get<int>();
    for (int i = 0; i < numThreads; ++i) {
        auto nameLen = data.get<short>();
        auto name = data.getString(nameLen);

        auto idLen = data.get<short>();
        auto id = data.getString(idLen);
        auto val = data.get<short>();
        spdlog::debug("Thread: {} {} {}", name, id, val);
    }
    data.seek(header.payloadOffset);
    data.skip(0xc);
    auto numCrosses = data.get<int>();

    std::vector<std::vector<std::unique_ptr<Cross>>> field(header.height);
    for (auto& r : field) {
        r.resize(header.width);
        for (auto& p : r) {
            p = nullptr;
        }
    }
    for (int i = 0; i < numCrosses; ++i) {
        auto cross = std::make_unique<Cross>(data);
        if (cross->x < 0 || cross->y < 0 || cross->x >= header.width || cross->y >= header.height) {
            throw std::runtime_error("Cross coordinates out of bounds");
        }
        field[cross->y][cross->x] = std::move(cross);
    }
    auto secondSectionLen = data.get<int>();
    data.skip(secondSectionLen);
    auto thirdSectionLen = data.get<int>(); // TODO: Is it really a length?
    data.skip(thirdSectionLen + 6);
    auto numDates = data.get<int>();
    for (int i = 0; i < numDates; ++i) {
        Date date(data);
        spdlog::debug("{}", date.toString());
        auto year = static_cast<int>(date.date.year());
        auto month = static_cast<unsigned int>(date.date.month());
        auto day = static_cast<unsigned int>(date.date.day());
        auto numStitches = date.stitches.total();
        if (config_.SkipAfter > 0 && numStitches > config_.SkipAfter) {
            spdlog::info(L"Skipping {}: {} for {}-{}-{}", setName, numStitches, year, month, day);
            continue;
        }
        stats_.Add(year, month, day, date.stitches, setName);
        setStats_[setName].Add(year, month, day, date.stitches, setName);
    }
}

void Parser::processFileLegacy(const std::vector<char>& v)
{
    const char* dataPtr = v.data();

    __pragma(pack(push, 1)) struct Header
    {
        int payloadOffset;
        // 0x4
        union {
            unsigned char uc1[10];
            short s1[5];
        } tmp1;
        // 0xe
        short width;
        // 0x10
        short height;
        // 0x12
        unsigned char tmp2[89];
        // 0x6b
        short sizeExtra;
        // 0x6d
    };

    Header* h = (Header*)dataPtr;
    spdlog::debug("Header: payloadOffset {} width {} height {} extra info {}", h->payloadOffset, h->width, h->height,
        h->sizeExtra);
    // 0x7b: short num threads
    // 0x7f: xxxx - strlen -- str

    /*
    parusnik DMC: 0x7b
    Fish3    DMC: 0x7b
    riolis0923 
        0x6b: 07 00 xx xx xx xx xx xx xx
        DMC: 0x82 0x86
    */
    if (h->sizeExtra > 0) {
        std::string extra{dataPtr + sizeof(Header), (size_t)h->sizeExtra};
        spdlog::info("Extra info: {}", extra);
    }
    const char* threadsPtr = dataPtr + sizeof(Header) + h->sizeExtra + 14;
    int numThreads = *(int*)threadsPtr;
    threadsPtr += 4;
    for (int i = 0; i < numThreads; ++i) {
        short nameLen = *(short*)threadsPtr;
        threadsPtr += 2;
        std::string name{threadsPtr, (size_t)nameLen};
        threadsPtr += nameLen;
        short idLen = *(short*)threadsPtr;
        threadsPtr += 2;
        std::string id{threadsPtr, (size_t)idLen};
        threadsPtr += idLen;
        short val = *(short*)threadsPtr;
        threadsPtr += 2;
        spdlog::debug("Thread: {} {} {}", name, id, val);
    }

    const char* payloadPtr = dataPtr + h->payloadOffset;
    spdlog::info("Gap: {}", (long long)payloadPtr - (long long)threadsPtr);
    int numCrosses = *(int*)(payloadPtr + 0xc);

    /*
    0 0 1
    0 1 1
    0 2 1
    ...
    0 10 1
    1 0 1
    1 1 1
    ...
    10 0 1
    10 1 1
    ...
    10 10 1

    */

    // Fish0: 0x31f -- 0x57d4
    // 54b5 or 10f1
    // Fish2: 0x31f -- 0x57a2 -- 50 (5x10) fewer than fish0
    // 10e7. offset 0x31b (start - 4) 030f c
    // Fish3: 0x31f -- 0x5770 -- another 50
    // Winter: 0x435 -- 0x5f976
    // 1310d. offset 0x431. 0x0425 c
    int startOffset = h->payloadOffset + 0x10;
    int endOffset = startOffset + numCrosses * 5;

    int remainder = v.size() - endOffset;
    spdlog::info("remainder: {} payload: {} start: {} end: {}", remainder, h->payloadOffset, startOffset, endOffset);

    /*
    * 132 per
    Footer: 0x5d (93)
    753 - 5 660
    225 - 1 132
    6165 - 46 6072
    753 - 5
    2469 - 18
    */
    /*
    remainder: 7089 payload: 1031 start: 1047 end: 158267
    */

    struct Pixel
    {
        union {
            unsigned short x;
            unsigned char xd[2];
        };
        union {
            unsigned short y;
            unsigned char yd[2];
        };
        unsigned char data3;
    };

    std::vector<std::vector<Pixel*>> field(h->height);
    for (auto& r : field) {
        r.resize(h->width);
        for (auto& p : r) {
            p = nullptr;
        }
    }

    for (int i = startOffset; i < endOffset; i += 5) {
        Pixel* p = (Pixel*)(dataPtr + i);
        //spdlog::info("{:x}: Pixel: {} {} {} {} {}", i, p->xd[0], p->xd[1], p->yd[0], p->yd[1], p->data3);
        if (p->x >= h->width || p->y >= h->height) {
            continue;
        }
        field[p->y][p->x] = new Pixel(*p);
    }

    // Fish2
    // 0x5770
    // 09 00 00 00 02 00 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 AC 00 00 00 0C 02
    // 0x578b
    // 0x1b

    // Fish3
    // 0x5748
    // 09 00 00 00 02 00 00 00 00 00 00 00 00 19 00 00 00 01 00 00 00 01 04 00 00 00 99 01 00 00 5a 11
    // 00 00 98 01 00 00 59 11 00 00 30 01 00 00 0c 02
    // 0x5778
    // 0x30 -- 0x15 extra (21)
    int secondSectionLen = *(int*)(dataPtr + endOffset);

    int extraOffset = *(int*)(dataPtr + endOffset + secondSectionLen + 4);
    int dateOffset = endOffset + secondSectionLen + 14 + extraOffset;
    const char* datePtr = dataPtr + dateOffset;
    int numDates = *(int*)datePtr;
    constexpr int dateSize = 88;
    if (datePtr + dateSize * numDates > dataPtr + v.size()) {
        spdlog::error("Exceeding file size!");
    }
    datePtr += 4;
    for (int i = 0; i < numDates; ++i) {
        short dateLen = *(short*)datePtr;
        datePtr += 2;
        std::string date{datePtr, (size_t)dateLen};
        datePtr += dateLen;
        unsigned char c = *datePtr;
        ++datePtr;
        short dayLen = *(short*)datePtr;
        datePtr += 2;
        std::string day{datePtr, (size_t)dayLen};
        datePtr += dayLen;

        short time1Len = *(short*)datePtr;
        datePtr += 2;
        std::string time1{datePtr, (size_t)time1Len};
        datePtr += time1Len;

        short time2Len = *(short*)datePtr;
        datePtr += 2;
        std::string time2{datePtr, (size_t)time2Len};
        datePtr += time2Len;
        long long numStitches = 0;
        for (int i = 0; i < 11; ++i) {
            long long numStitchesHere = -*(long long*)datePtr;
            spdlog::debug("{}", numStitchesHere);
            datePtr += 8;
            numStitches += numStitchesHere;
        }
        datePtr += 8;
        int extraDateFields = *(int*)datePtr;
        datePtr += 4;
        datePtr += extraDateFields * 12;
        spdlog::info("Date entry: {} {} {} {} {} {}", date, (int)c, day, time1, time2, numStitches);
        //datePtr += 0x64;
    }
    /*
    std::ofstream of(fileName + "_out.txt");
    for (auto& row : field) {
        for (auto& pixel : row) {
            if (!pixel) {
                //std::cout << " ";
                of << " ";
            } else {
                //std::cout << (int)pixel->data3;
                of << (int)pixel->data3;
            }
        }
        of << "\n";
        //std::cout << "\n";
    }
    */
}
} // namespace SagaStats
