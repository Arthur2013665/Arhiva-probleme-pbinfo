#ifndef SCRIPT_H
#define SCRIPT_H

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ArchiveContext {
    fs::path projectRoot;
    fs::path pbinfoMain;
    fs::path archiveDir;
    fs::path vaultDir;
    fs::path configPath;
    std::vector<int> problemIds;
    int activeProblemId = -1;
    std::string terminalTheme = "classic";
    bool productionMode = false;
    bool soundAlerts = false;
    std::chrono::steady_clock::time_point sessionStart;
    std::chrono::steady_clock::time_point problemTimerStart;
    bool problemTimerRunning = false;
    std::map<std::string, std::string> config;
    std::vector<std::string> commandHistory;
};

using ScriptHandler = std::function<void(ArchiveContext&, const std::vector<std::string>&)>;

fs::path scriptFindPbinfoMainDir();
fs::path scriptFindProjectRoot(const fs::path& pbinfoMain);
std::vector<int> scriptLoadProblemIds(const fs::path& pbinfoMain);

void scriptInitArchive(ArchiveContext& ctx);
void scriptRegisterAllCommands(std::map<std::string, ScriptHandler>& routes);
bool scriptHandleInput(ArchiveContext& ctx, const std::string& line,
                       const std::map<std::string, ScriptHandler>& routes);

void scriptPrintMasterHelp();
void scriptPrintModuleHelp(int module);

bool scriptShowSolution(const ArchiveContext& ctx, int id);
void scriptPrintAvailableProblems(const ArchiveContext& ctx, bool compact = false);

std::string scriptReadSolutionSource(const ArchiveContext& ctx, int id);
fs::path scriptSolutionPath(const ArchiveContext& ctx, int id);
bool scriptProblemExists(const ArchiveContext& ctx, int id);

void scriptSaveConfig(ArchiveContext& ctx);
void scriptLoadConfig(ArchiveContext& ctx);

std::vector<std::string> scriptTokenize(const std::string& line);

#endif
