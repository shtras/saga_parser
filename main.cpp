#include "Utils.h"
#include "Parser.h"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <wx/wx.h>
#include <wx/treectrl.h>

#include <fstream>
#include <format>

namespace fs = std::filesystem;
namespace SagaStats
{
void scanDir(const std::string& pathStr)
{
    Parser parser;

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
            parser.ParseFile(path);
        } catch (std::runtime_error& e) {
            spdlog::error("Failed: {}", e.what());
            continue;
        }
    }
}

class MyFrame : public wxFrame
{
public:
    enum { ID_ScanFolder = 1, ID_ScanFile };

    MyFrame()
        : wxFrame(NULL, wxID_ANY, "Cross Stitch Saga Parser")
    {
        auto menuBar = new wxMenuBar();

        auto menuFile = new wxMenu();
        menuFile->Append(ID_ScanFolder, "&Scan folder...\tCtrl-O", "Select a folder with .sp files to scan");
        menuFile->Append(ID_ScanFile, "&Scan a single file...\tCtrl-O", "Select a single .sp files to scan");
        menuFile->AppendSeparator();
        menuFile->Append(wxID_EXIT);

        auto menuHelp = new wxMenu();
        menuHelp->Append(wxID_ABOUT);
        menuBar->Append(menuFile, "&File");
        menuBar->Append(menuHelp, "&Help");

        SetMenuBar(menuBar);

        Bind(
            wxEVT_MENU,
            [](wxCommandEvent&) {
                wxMessageBox(
                    "Cross Stitch Saga parser v1.0\nThat's all, folks!", "SagaParser", wxOK | wxICON_INFORMATION);
            },
            wxID_ABOUT);

        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent&) {
                wxDirDialog dirOpenDialog(
                    this, _("Select folder to scan"), "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
                if (dirOpenDialog.ShowModal() == wxID_CANCEL) {
                    spdlog::debug("Cancel on select folder");
                    return;
                }
                SetStatusText("Scanning. Please wait...");
                Parser parser;
                parser.ParseDir(dirOpenDialog.GetPath().ToStdString());
                fillStats(parser.GetStats());
                SetStatusText("Scan completed.");
            },
            ID_ScanFolder);

        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent&) {
                wxFileDialog fileOpenDialog(this, _("Open SP file"), "", "", "SP files (*.sp)|*.sp",
                    wxFD_DEFAULT_STYLE | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
                if (fileOpenDialog.ShowModal() == wxID_CANCEL) {
                    spdlog::debug("Cancel on select file");
                    return;
                }
                SetStatusText("Scanning. Please wait...");
                Parser parser;
                wxArrayString paths;
                fileOpenDialog.GetPaths(paths);
                for (auto& path : paths) {
                    parser.ParseFile(path.ToStdWstring());
                }
                fillStats(parser.GetStats());
                SetStatusText("Scan completed.");
            },
            ID_ScanFile);

        Bind(
            wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);

        auto panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxRAISED_BORDER);

        auto panelSizer = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(panelSizer);

        statsTree_ = new wxTreeCtrl(panel, wxID_ANY, wxDefaultPosition, {600, 600});
        panelSizer->Add(statsTree_, 1, wxEXPAND | wxSHRINK);

        stats_ = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, {600, 100}, wxTE_MULTILINE | wxTE_READONLY);
        panelSizer->Add(stats_, 0, wxEXPAND | wxSHRINK);

        auto clearBtn = new wxButton(panel, wxID_ANY, "Clear", wxDefaultPosition, {600, 30});
        panelSizer->Add(clearBtn);
        clearBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            stats_->Clear();
            statsTree_->DeleteAllItems();
        });

        auto statusBar = CreateStatusBar();
        SetStatusText("I'm a status bar");

        auto topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(panel, 1, wxEXPAND | wxSHRINK);

        SetSizerAndFit(topSizer);

        auto icon = new wxIcon(wxIconLocation("saga_parser.exe", 0));
        SetIcon(*icon);
    }

private:
    static std::string_view getMonthName(int num)
    {
        if (num < 1 || num > 12) {
            return "Unknown";
        }
        std::array<std::string_view, 12> months = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        return months[num - 1];
    }

    void fillStats(const Stats& stats)
    {
        statsTree_->DeleteAllItems();
        auto rootId = statsTree_->AddRoot(std::format("Total: {}", stats.stat.numStitches));
        for (const auto& year : stats.years) {
            auto yearId =
                statsTree_->AppendItem(rootId, std::format("Year {}: {}", year.first, year.second.stat.numStitches));
            for (const auto& set : year.second.stat.sets) {
                auto text = std::format("{}: {} ({:.2f}%)", set.first, set.second,
                    static_cast<float>(set.second) / static_cast<float>(year.second.stat.numStitches) * 100.0f);
                statsTree_->AppendItem(yearId, text);
            }

            for (const auto& month : year.second.months) {
                auto monthId = statsTree_->AppendItem(
                    yearId, std::format("{}: {}", getMonthName(month.first), month.second.stat.numStitches));
                for (const auto& set : month.second.stat.sets) {
                    statsTree_->AppendItem(
                        monthId, std::format("{}: {} ({:.2f}%)", set.first, set.second,
                                     static_cast<float>(set.second) /
                                         static_cast<float>(month.second.stat.numStitches) * 100.0f));
                }
            }
            statsTree_->Expand(yearId);
        }
        statsTree_->Expand(rootId);

        stats_->AppendText(std::format("Errors: {}\n", stats.errors.size()));
        for (const auto& error : stats.errors) {
            stats_->AppendText(std::format("{}\n", error));
        }
    }
    wxTextCtrl* stats_;
    wxTreeCtrl* statsTree_;
};

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        auto res = config_.Load("config.json");
        if (!res) {
            spdlog::warn("Failed to load config");
        }
        if (config_.UseConsole) {
            AllocConsole();
            FILE* stream = nullptr;
            _wfreopen_s(&stream, L"CON", L"w", stdout);
        }
        spdlog::set_level(config_.LogLevel);
        auto frame = new MyFrame();
        frame->Show(true);
        spdlog::info("Application started");
        return true;
    }

    int OnExit() override
    {
        if (config_.UseConsole) {
            FreeConsole();
        }
        return 0;
    }

private:
    Config config_;
};

} // namespace SagaStats
wxIMPLEMENT_APP(SagaStats::MyApp);

/*
int main()
{
    //processFileNew("C:\\Projects\\crossstitchsaga\\data\\08789_Santa_Snowman_Ornaments20230924_24.09.2023_21-15.sp");
    //processFile("C:\\Projects\\crossstitchsaga\\data\\honey_20240301_04.03.2024_22-52.sp");
    //processFileNew("C:\\Projects\\crossstitchsaga\\data\\honey_20240301_04.03.2024_22-52.sp");

    //processFile("C:\\Projects\\crossstitchsaga\\data\\honey_20240302_05.03.2024_22-53.sp");
    //processFileNew("C:\\Projects\\crossstitchsaga\\data\\honey_20240302_05.03.2024_22-53.sp");

    scanDir("C:\\Projects\\crossstitchsaga\\data");
    //return 0;

    return 0;
}
*/