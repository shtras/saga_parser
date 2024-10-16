﻿#include "Utils.h"
#include "Parser.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>
#include <nlohmann/json.hpp>
#include <wx/wx.h>
#include <wx/treectrl.h>

#include <fstream>
#include <format>

namespace fs = std::filesystem;
namespace SagaStats
{
void createConsole()
{
    AllocConsole();
    FILE* stream = nullptr;
    _wfreopen_s(&stream, L"CON", L"w", stdout);
}

void destroyConsole()
{
    FreeConsole();
}

class OptionsFrame : public wxFrame
{
public:
    OptionsFrame(Config& config)
        : wxFrame(nullptr, wxID_ANY, "Options")
    {
        auto optionsPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
        auto optionsSizer = new wxGridSizer(2);
        optionsPanel->SetSizer(optionsSizer);

        auto logLevelLabel = new wxStaticText(optionsPanel, wxID_ANY, "Log Level");
        optionsSizer->Add(logLevelLabel);

        auto logLevel = new wxComboBox(
            optionsPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, 0,
            wxCB_READONLY
        );
        for (int i = 0; i < spdlog::level::n_levels; ++i) {
            logLevel->Append(fmt::to_string(spdlog::level::to_string_view(
                static_cast<spdlog::level::level_enum>(spdlog::level::trace + i)
            )));
            if (config.LogLevel == i) {
                logLevel->SetSelection(i);
            }
        }
        optionsSizer->Add(logLevel);

        auto useConsoleLabel = new wxStaticText(optionsPanel, wxID_ANY, "Use Console");
        optionsSizer->Add(useConsoleLabel);

        auto useConsole = new wxCheckBox(optionsPanel, wxID_ANY, "");
        optionsSizer->Add(useConsole);
        useConsole->SetValue(config.UseConsole);

        auto skipAfterLabel =
            new wxStaticText(optionsPanel, wxID_ANY, "Skip days with more than (0=disable)");
        optionsSizer->Add(skipAfterLabel);

        auto skipAfter = new wxTextCtrl(optionsPanel, wxID_ANY, std::to_string(config.SkipAfter));
        optionsSizer->Add(skipAfter);

        auto buttonsPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
        auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonsPanel->SetSizer(buttonSizer);

        auto okButton = new wxButton(buttonsPanel, wxID_ANY, "Save");
        okButton->Bind(
            wxEVT_BUTTON,
            [this, logLevel, useConsole, skipAfter, &config](wxCommandEvent&) {
                config.LogLevel = spdlog::level::from_str(
                    logLevel->GetString(logLevel->GetSelection()).ToStdString()
                );
                if (config.UseConsole && !useConsole->GetValue()) {
                    destroyConsole();
                } else if (!config.UseConsole && useConsole->GetValue()) {
                    createConsole();
                }
                config.UseConsole = useConsole->GetValue();
                auto skipAfterStr = skipAfter->GetLineText(0).ToStdString();
                try {
                    config.SkipAfter = std::stoi(skipAfterStr);
                } catch (...) {
                    spdlog::error("Failed to parse integer: {}", skipAfterStr);
                }
                config.Save("config.json");
                spdlog::set_level(config.LogLevel);
                spdlog::default_logger()->set_level(config.LogLevel);
                for (auto& sink : spdlog::default_logger()->sinks()) {
                    sink->set_level(config.LogLevel);
                }
                Close();
            }
        );
        buttonSizer->Add(okButton);

        auto calcelButton = new wxButton(buttonsPanel, wxID_ANY, "Cancel");
        calcelButton->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { Close(); });
        buttonSizer->Add(calcelButton);

        auto topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(optionsPanel);
        topSizer->Add(buttonsPanel);

        SetSizerAndFit(topSizer);
    }

private:
};

class MyFrame : public wxFrame
{
public:
    enum { ID_ScanFolder = 1, ID_ScanFile, ID_Options };

    explicit MyFrame(Config& config)
        : wxFrame(nullptr, wxID_ANY, "Cross Stitch Saga Parser")
        , config_(config)
    {
        auto menuBar = new wxMenuBar();

        auto menuFile = new wxMenu();
        menuFile->Append(
            ID_ScanFolder, "&Scan folder...\tCtrl-O", "Select a folder with .sp files to scan"
        );
        menuFile->Append(
            ID_ScanFile, "&Scan a file...\tCtrl-F", "Select a single .sp files to scan"
        );
        menuFile->AppendSeparator();
        menuFile->Append(wxID_EXIT);

        auto menuTools = new wxMenu();
        menuTools->Append(ID_Options, "&Options");

        auto menuHelp = new wxMenu();
        menuHelp->Append(wxID_ABOUT);
        menuBar->Append(menuFile, "&File");
        menuBar->Append(menuTools, "&Tools");
        menuBar->Append(menuHelp, "&Help");

        SetMenuBar(menuBar);

        Bind(
            wxEVT_MENU,
            [](wxCommandEvent&) {
                wxMessageBox(
                    "Cross Stitch Saga parser v1.0\nThat's all, folks!", "SagaParser",
                    wxOK | wxICON_INFORMATION
                );
            },
            wxID_ABOUT
        );

        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent&) {
                wxDirDialog dirOpenDialog(
                    this, _("Select folder to scan"), "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST
                );
                if (dirOpenDialog.ShowModal() == wxID_CANCEL) {
                    spdlog::debug("Cancel on select folder");
                    return;
                }
                SetStatusText("Scanning. Please wait...");
                Parser parser(config_);
                parser.ParseDir(dirOpenDialog.GetPath().ToStdString());
                fillStats(parser);
                SetStatusText("Scan completed.");
            },
            ID_ScanFolder
        );

        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent&) {
                wxFileDialog fileOpenDialog(
                    this, _("Open SP file"), "", "", "SP files (*.sp)|*.sp",
                    wxFD_DEFAULT_STYLE | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE
                );
                if (fileOpenDialog.ShowModal() == wxID_CANCEL) {
                    spdlog::debug("Cancel on select file");
                    return;
                }
                SetStatusText("Scanning. Please wait...");
                Parser parser(config_);
                wxArrayString paths;
                fileOpenDialog.GetPaths(paths);
                for (auto& path : paths) {
                    parser.ParseFile(path.ToStdWstring());
                }
                fillStats(parser);
                SetStatusText("Scan completed.");
            },
            ID_ScanFile
        );

        Bind(
            wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT
        );

        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent&) {
                auto frame = new OptionsFrame(config_);
                frame->Show(true);
            },
            ID_Options
        );

        auto panel = new wxPanel(
            this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxRAISED_BORDER
        );

        auto panelSizer = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(panelSizer);

        statsTree_ = new wxTreeCtrl(panel, wxID_ANY, wxDefaultPosition, {600, 600});
        panelSizer->Add(statsTree_, 1, wxEXPAND | wxSHRINK);

        stats_ = new wxTextCtrl(
            panel, wxID_ANY, "", wxDefaultPosition, {600, 100}, wxTE_MULTILINE | wxTE_READONLY
        );
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

        auto callback_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
            [this](const spdlog::details::log_msg& msg) {
                stats_->AppendText(fmt::to_string(msg.payload));
                stats_->AppendText("\n");
            }
        );
        callback_sink->set_level(config_.LogLevel);
        spdlog::default_logger()->sinks().push_back(callback_sink);
    }

private:
    static std::string_view getMonthName(int num)
    {
        if (num < 1 || num > 12) {
            return "Unknown";
        }
        std::array<std::string_view, 12> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        return months[num - 1];
    }

    template <typename S>
    S formatStr(float f) const
    {
        using SS = typename std::conditional<
            std::is_same<S, std::string>::value, std::stringstream, std::wstringstream>::type;
        SS ss;
        if (static_cast<int>(f) == f) {
            ss << f;
        } else {
            ss << std::fixed << std::setprecision(1) << f;
        }
        return ss.str();
    }

    void fillStatsSubtree(wxTreeItemId rootId, const Stats& stats, bool bySet)
    {
        for (const auto& year : stats.years) {
            auto yearId = statsTree_->AppendItem(
                rootId,
                std::format("Year {}: {}", year.first, year.second.stat.stitches.totalStr())
            );
            if (bySet) {
                for (const auto& set : year.second.stat.sets) {
                    auto text = std::format(
                        L"{}: {} ({:.2f}%)", set.first, formatStr<std::wstring>(set.second),
                        set.second / static_cast<float>(year.second.stat.stitches.total()) * 100.0f
                    );
                    statsTree_->AppendItem(yearId, text);
                }
            }

            for (const auto& month : year.second.months) {
                auto monthId = statsTree_->AppendItem(
                    yearId,
                    std::format(
                        "{}: {}", getMonthName(month.first), month.second.stat.stitches.totalStr()
                    )
                );
                if (!bySet) {
                    assert(month.second.stat.sets.size() == 1);
                }
                for (const auto& set : month.second.stat.sets) {
                    wxTreeItemId setId;
                    if (bySet) {
                        setId = statsTree_->AppendItem(
                            monthId,
                            std::format(
                                L"{}: {} ({:.2f}%)", set.first, formatStr<std::wstring>(set.second),
                                set.second /
                                    static_cast<float>(month.second.stat.stitches.total()) * 100.0f
                            )
                        );
                    } else {
                        setId = monthId;
                    }
                    if (month.second.setByDay.count(set.first) == 0) {
                        stats_->AppendText(
                            std::format(L"Could not find by day entries for {}", set.first)
                        );
                        continue;
                    }
                    for (const auto& setDay : month.second.setByDay.at(set.first)) {
                        auto dayId = statsTree_->AppendItem(
                            setId,
                            std::format("{} : {}", setDay.first, setDay.second.stitches.totalStr())
                        );
                        for (int i = 0; i < setDay.second.stitches.stitches.size(); ++i) {
                            if (setDay.second.stitches.stitches[i] == 0) {
                                continue;
                            }
                            statsTree_->AppendItem(
                                dayId, std::format(
                                           "{}: {}", StitchCount::stitchNames[i],
                                           formatStr<std::string>(
                                               setDay.second.stitches.stitches[i] /
                                               StitchCount::crossMultiplier[i]
                                           )

                                       )
                            );
                        }
                        if (setDay.second.stitches.decorative > 0) {
                            statsTree_->AppendItem(
                                dayId,
                                std::format("Decorative: {}", setDay.second.stitches.decorative)
                            );
                        }
                    }
                }
            }
            statsTree_->Expand(yearId);
        }
    }

    void fillStats(const Parser& parser)
    {
        const auto& stats = parser.GetStats();
        statsTree_->DeleteAllItems();
        auto rootId = statsTree_->AddRoot("Statistics");

        auto globalTreeId = statsTree_->AppendItem(
            rootId, std::format("Total: {}", stats.stat.stitches.totalStr())
        );

        fillStatsSubtree(globalTreeId, stats, true);

        auto bySetId = statsTree_->AppendItem(rootId, "By design");

        for (const auto& [setName, setStats] : parser.GetSetStats()) {
            auto setId =
                statsTree_->AppendItem(bySetId, std::format(L"{}: {}", setName, setStats.stat.stitches.totalStr<std::wstring>())
            );
            fillStatsSubtree(setId, setStats, false);
        }        
        statsTree_->Expand(rootId);

        spdlog::info("Errors: {}", stats.errors.size());
        for (const auto& error : stats.errors) {
            stats_->AppendText(std::format(L"{}\n", error));
        }
    }
    wxTextCtrl* stats_;
    wxTreeCtrl* statsTree_;
    Config& config_;
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
            createConsole();
        }
        spdlog::set_level(config_.LogLevel);
        auto frame = new MyFrame(config_);
        frame->Show(true);
        spdlog::info("Application started");
        return true;
    }

    int OnExit() override
    {
        if (config_.UseConsole) {
            destroyConsole();
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