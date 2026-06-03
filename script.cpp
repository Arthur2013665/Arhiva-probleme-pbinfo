#include "script.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

const char* RESET = "\033[0m";
const char* GREEN = "\033[32m";
const char* CYAN = "\033[36m";
const char* YELLOW = "\033[33m";
const char* MAGENTA = "\033[35m";
const char* GRAY = "\033[90m";

int parseIdArg(const ArchiveContext& ctx, const std::vector<std::string>& args, size_t index = 0) {
    if (index < args.size()) {
        try { return std::stoi(args[index]); } catch (...) {}
    }
    return ctx.activeProblemId;
}

void themeColor(const ArchiveContext& ctx) {
    if (ctx.terminalTheme == "matrix") std::cout << GREEN;
    else if (ctx.terminalTheme == "neon") std::cout << MAGENTA;
    else if (ctx.terminalTheme == "stealth") std::cout << GRAY;
    else if (ctx.terminalTheme == "monokai") std::cout << YELLOW;
    else std::cout << CYAN;
}

void beepIfEnabled(const ArchiveContext& ctx) {
    if (!ctx.soundAlerts) return;
#ifdef _WIN32
    Beep(800, 120);
#else
    std::cout << '\a' << std::flush;
#endif
}

void asciiProgress(const std::string& label, size_t current, size_t total, int width = 40) {
    if (total == 0) total = 1;
    const int filled = static_cast<int>((current * width) / total);
    std::cout << label << " [";
    for (int i = 0; i < width; ++i) std::cout << (i < filled ? '#' : '.');
    std::cout << "] " << (100 * current / total) << "%\r" << std::flush;
    if (current >= total) std::cout << '\n';
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeFile(const fs::path& p, const std::string& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p);
    out << data;
}

int countNestedLoops(const std::string& code) {
    int depth = 0, maxDepth = 0;
    std::regex loopRe(R"(\b(for|while)\s*\()");
    auto begin = std::sregex_iterator(code.begin(), code.end(), loopRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        ++depth;
        maxDepth = std::max(maxDepth, depth);
    }
    std::regex closeRe(R"(\})");
    auto cbegin = std::sregex_iterator(code.begin(), code.end(), closeRe);
    for (auto it = cbegin; it != std::sregex_iterator(); ++it) {
        if (depth > 0) --depth;
    }
    return std::max(maxDepth, depth);
}

// ===================== MODULE 1: Code Analysis (1-10) =====================

void analyzeComplexity(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    if (code.empty()) { std::cout << "Problema invalida.\n"; return; }
    const int nest = countNestedLoops(code);
    std::string guess = "O(N)";
    if (code.find("while") != std::string::npos && code.find("/= 2") != std::string::npos) guess = "O(log N)";
    else if (nest >= 2) guess = "O(N^2)";
    else if (nest == 1) guess = "O(N)";
  std::cout << "[Complexity] Problema #" << id << " estimare: " << guess
              << " (adancime bucle detectata: " << nest << ")\n";
}

void analyzeSpace(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    if (code.empty()) return;
    std::regex arr(R"((\w+)\s+(\w+)\s*\[\s*(\d+)\s*\])");
    long long bytes = 0;
    for (std::sregex_iterator it(code.begin(), code.end(), arr), end; it != end; ++it) {
        const std::string type = (*it)[1].str();
        const long long n = std::stoll((*it)[3].str());
        long long sz = 4;
        if (type.find("long long") != std::string::npos) sz = 8;
        else if (type.find("char") != std::string::npos) sz = 1;
        else if (type.find("double") != std::string::npos) sz = 8;
        bytes += n * sz;
    }
    std::cout << "[Space] Estimare memorie statica: ~" << bytes << " bytes ("
              << std::fixed << std::setprecision(2) << (bytes / 1048576.0) << " MB)\n";
}

void lintPbinfo(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    if (code.empty()) return;
    int warnings = 0;
    if (code.find("long long") == std::string::npos &&
        (code.find("sum") != std::string::npos || code.find("S +=") != std::string::npos))
        std::cout << "  [!] Posibil overflow: foloseste long long la sume mari.\n", ++warnings;
    if (std::regex_search(code, std::regex(R"(\bif\s*\([^=]*=[^=])")))
        std::cout << "  [!] Verifica atribuiri in if (= vs ==).\n", ++warnings;
    if (code.find("endl") != std::string::npos && code.find("ios_base::sync") == std::string::npos)
        std::cout << "  [!] Folosesti endl; considera '\\n' pentru viteza.\n", ++warnings;
    std::cout << "[Lint] " << warnings << " avertismente pentru #" << id << ".\n";
}

void sweepHeaders(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    auto code = scriptReadSolutionSource(ctx, id);
    if (code.empty()) return;
    if (code.find("bits/stdc++.h") == std::string::npos) {
        std::cout << "[Headers] Nu foloseste bits/stdc++.h\n"; return;
    }
    std::vector<std::string> needed;
    if (code.find("cout") != std::string::npos || code.find("cin") != std::string::npos) needed.push_back("#include <iostream>");
    if (code.find("vector") != std::string::npos) needed.push_back("#include <vector>");
    if (code.find("fstream") != std::string::npos || code.find("ifstream") != std::string::npos)
        needed.push_back("#include <fstream>");
    if (code.find("algorithm") != std::string::npos) needed.push_back("#include <algorithm>");
    std::cout << "[Headers] Inlocuiri sugerate:\n";
    for (const auto& h : needed) std::cout << "  " << h << '\n';
}

void injectSnippet(ArchiveContext& ctx, const std::vector<std::string>& args) {
  if (args.empty()) {
    std::cout << "Snippet-uri: fastio, read, digits, gcd\n";
    return;
  }
  const std::string key = args[0];
  if (key == "fastio") std::cout << "ios_base::sync_with_stdio(0); cin.tie(0);\n";
  else if (key == "read") std::cout << "template<class T> void read(T& x){cin>>x;}\n";
  else if (key == "digits") std::cout << "while(n){d.push_back(n%10);n/=10;}\n";
  else if (key == "gcd") std::cout << "long long gcd(ll a,ll b){return b?gcd(b,a%b):a;}\n";
  else std::cout << "Snippet necunoscut.\n";
}

void stripComments(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    auto code = scriptReadSolutionSource(ctx, id);
    if (code.empty()) return;
    std::ostringstream out;
    std::istringstream in(code);
    std::string line;
    while (std::getline(in, line)) {
        const auto pos = line.find("//");
        if (pos != std::string::npos) line = line.substr(0, pos);
        if (!line.empty()) out << line << '\n';
    }
    const auto outPath = ctx.archiveDir / ("stripped_" + std::to_string(id) + ".cpp");
    writeFile(outPath, out.str());
    std::cout << "[Strip] Salvat: " << outPath.string() << '\n';
}

void checkFastIo(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    const bool has = code.find("sync_with_stdio") != std::string::npos;
    std::cout << (has ? "[FastIO] OK: fast I/O detectat.\n" : "[FastIO] Lipseste sync_with_stdio(0).\n");
}

void checkRecursion(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    std::regex rec(R"(\b(\w+)\s*\([^)]*\)\s*\{[\s\S]*?\b\1\s*\()");
    if (std::regex_search(code, rec))
        std::cout << "[Recursion] Recursie detectata - verifica cazuri de baza si limita stivei.\n";
    else std::cout << "[Recursion] Nu s-a detectat recursie evidenta.\n";
}

void suggestBitwise(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    if (code.find("/ 2") != std::string::npos || code.find("/2") != std::string::npos)
        std::cout << "  Sugestie: inlocuieste /2 cu >> 1\n";
    if (code.find("* 2") != std::string::npos || code.find("*2") != std::string::npos)
        std::cout << "  Sugestie: inlocuieste *2 cu << 1\n";
}

void extractConstants(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    std::regex num(R"(\b(\d{4,})\b)");
    std::set<std::string> seen;
    std::cout << "// Constante extrase pentru #" << id << ":\n";
    for (std::sregex_iterator it(code.begin(), code.end(), num), end; it != end; ++it) {
        const std::string v = (*it)[1].str();
        if (seen.insert(v).second)
            std::cout << "const int C" << v << " = " << v << ";\n";
    }
}

// ===================== MODULE 2: Search (11-20) =====================

void searchKeywords(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cout << "Usage: --search-keywords cuvant1 cuvant2 ...\n"; return; }
    std::vector<int> hits;
    for (size_t i = 0; i < ctx.problemIds.size(); ++i) {
        if (i % 200 == 0) asciiProgress("[Search]", i, ctx.problemIds.size());
        const auto code = scriptReadSolutionSource(ctx, ctx.problemIds[i]);
        std::string low = code;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        bool ok = true;
        for (const auto& kw : args) {
            std::string k = kw;
            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
            if (low.find(k) == std::string::npos) { ok = false; break; }
        }
        if (ok) hits.push_back(ctx.problemIds[i]);
    }
    asciiProgress("[Search]", ctx.problemIds.size(), ctx.problemIds.size());
    std::cout << "Gasite " << hits.size() << " probleme: ";
    for (size_t i = 0; i < std::min(hits.size(), size_t(30)); ++i) std::cout << '#' << hits[i] << ' ';
    if (hits.size() > 30) std::cout << "...";
    std::cout << '\n';
}

void sortSuccessRate(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::cout << "[Analytics] Sortare dupa rata succes (meta locala / euristic).\n";
    std::vector<std::pair<double, int>> ranked;
    for (int id : ctx.problemIds) {
        const auto meta = ctx.archiveDir / ("meta_" + std::to_string(id) + ".txt");
        double rate = 0.5;
        if (fs::exists(meta)) {
            const auto s = readFile(meta);
            const auto p = s.find("success=");
            if (p != std::string::npos) rate = std::stod(s.substr(p + 8));
        } else rate = (id < 500) ? 0.85 : (id < 2000 ? 0.55 : 0.35);
        ranked.emplace_back(rate, id);
    }
    std::sort(ranked.begin(), ranked.end(), std::greater<>());
    for (size_t i = 0; i < std::min(ranked.size(), size_t(25)); ++i)
        std::cout << "  #" << ranked[i].second << " rate~" << ranked[i].first << '\n';
}

void suggestScore(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::cout << "[Score/min] Recomandari (ID mic, complexitate mica):\n";
    int shown = 0;
    for (int id : ctx.problemIds) {
        if (id > 300) continue;
        const auto code = scriptReadSolutionSource(ctx, id);
        if (code.size() < 400 && code.find("vector") == std::string::npos) {
            std::cout << "  #" << id << " (~usor, cod scurt)\n";
            if (++shown >= 15) break;
        }
    }
}

void tagCloud(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::map<std::string, int> tags;
    const std::vector<std::pair<std::string, std::string>> keys = {
        {"vector", "Arrays"}, {"for", "Loops"}, {"ifstream", "Files"},
        {"gcd", "Math"}, {"string", "Strings"}, {"queue", "Graph/BFS"}
    };
    for (int id : ctx.problemIds) {
        const auto code = scriptReadSolutionSource(ctx, id);
        for (const auto& kv : keys)
            if (code.find(kv.first) != std::string::npos) ++tags[kv.second];
    }
    std::cout << "[Tag Cloud]\n";
    for (const auto& t : tags) {
        std::cout << "  " << t.first << " ";
        for (int i = 0; i < std::min(t.second / 50, 20); ++i) std::cout << '#';
        std::cout << " (" << t.second << ")\n";
    }
}

void similarProblems(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto base = scriptReadSolutionSource(ctx, id);
    std::vector<std::pair<int, int>> scores;
    for (int other : ctx.problemIds) {
        if (other == id) continue;
        const auto code = scriptReadSolutionSource(ctx, other);
        int score = 0;
        if ((base.find("ifstream") != std::string::npos) == (code.find("ifstream") != std::string::npos)) score += 2;
        if ((base.find("vector") != std::string::npos) == (code.find("vector") != std::string::npos)) score += 2;
        if (std::abs((int)code.size() - (int)base.size()) < 200) score += 1;
        if (score >= 3) scores.emplace_back(score, other);
    }
    std::sort(scores.rbegin(), scores.rend());
    std::cout << "[Similar] Pentru #" << id << ":\n";
    for (size_t i = 0; i < std::min(scores.size(), size_t(3)); ++i)
        std::cout << "  #" << scores[i].second << " (scor " << scores[i].first << ")\n";
}

void filterAuthor(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cout << "--filter-author <nume>\n"; return; }
    const std::string author = args[0];
    for (int id : ctx.problemIds) {
        const auto meta = ctx.archiveDir / ("meta_" + std::to_string(id) + ".txt");
        if (fs::exists(meta) && readFile(meta).find(author) != std::string::npos)
            std::cout << "  #" << id << '\n';
    }
}

void filterIdRange(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cout << "--filter-id-range <min> <max>\n"; return; }
    int lo = std::stoi(args[0]), hi = std::stoi(args[1]);
    for (int id : ctx.problemIds)
        if (id >= lo && id <= hi) std::cout << '#' << id << ' ';
    std::cout << '\n';
}

void flagMissingMeta(ArchiveContext& ctx, const std::vector<std::string>&) {
    int missing = 0;
    for (int id : ctx.problemIds) {
        const auto dir = ctx.pbinfoMain / ("pbinfo-" + std::to_string(id));
        const bool hasTests = fs::exists(dir / "tests") || fs::exists(dir / "sample.in");
        const bool hasMeta = fs::exists(ctx.archiveDir / ("meta_" + std::to_string(id) + ".txt"));
        if (!hasTests || !hasMeta) {
            std::cout << "  #" << id << (hasTests ? "" : " [fara tests]") << (hasMeta ? "" : " [fara meta]") << '\n';
            ++missing;
        }
    }
    std::cout << "[Missing] Total flaguite: " << missing << '\n';
}

void filterMemory(ArchiveContext& ctx, const std::vector<std::string>& args) {
    int limitMb = 8;
    if (!args.empty()) limitMb = std::stoi(args[0]);
    std::cout << "[Memory] Probleme cu restrictie stransa (<=" << limitMb << "MB) - verifica meta:\n";
    for (int id : ctx.problemIds) {
        const auto meta = ctx.archiveDir / ("meta_" + std::to_string(id) + ".txt");
        if (fs::exists(meta)) {
            const auto s = readFile(meta);
            if (s.find("mem=" + std::to_string(limitMb)) != std::string::npos)
                std::cout << "  #" << id << '\n';
        }
    }
}

void sortTimeLimit(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::vector<std::pair<double, int>> items;
    for (int id : ctx.problemIds) {
        double tl = 1.0;
        const auto meta = ctx.archiveDir / ("meta_" + std::to_string(id) + ".txt");
        if (fs::exists(meta)) {
            const auto s = readFile(meta);
            const auto p = s.find("time=");
            if (p != std::string::npos) tl = std::stod(s.substr(p + 5));
        }
        items.emplace_back(tl, id);
    }
    std::sort(items.begin(), items.end());
    for (size_t i = 0; i < std::min(items.size(), size_t(20)); ++i)
        std::cout << "  #" << items[i].second << " " << items[i].first << "s\n";
}

// ===================== MODULE 3: Testing (21-30) =====================

void genRandomArray(ArchiveContext&, const std::vector<std::string>& args) {
    int n = 10, lo = 1, hi = 100;
    if (args.size() > 0) n = std::stoi(args[0]);
    if (args.size() > 1) lo = std::stoi(args[1]);
    if (args.size() > 2) hi = std::stoi(args[2]);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(lo, hi);
    std::cout << n << '\n';
    for (int i = 0; i < n; ++i) std::cout << dist(rng) << (i + 1 < n ? ' ' : '\n');
}

void genRandomGraph(ArchiveContext&, const std::vector<std::string>& args) {
    int n = 5, m = 6;
    if (!args.empty()) n = std::stoi(args[0]);
    if (args.size() > 1) m = std::stoi(args[1]);
    std::cout << n << ' ' << m << '\n';
    std::mt19937 rng(std::random_device{}());
    for (int i = 0; i < m; ++i)
        std::cout << (rng() % n + 1) << ' ' << (rng() % n + 1) << ' ' << (rng() % 10 + 1) << '\n';
}

void genRandomString(ArchiveContext&, const std::vector<std::string>& args) {
    int len = 20;
    std::string mode = "alpha";
    if (!args.empty()) len = std::stoi(args[0]);
    if (args.size() > 1) mode = args[1];
    const std::string alpha = "abcdefghijklmnopqrstuvwxyz";
    const std::string bin = "01";
    std::mt19937 rng(std::random_device{}());
    std::string out;
    for (int i = 0; i < len; ++i) {
        if (mode == "binary") out += bin[rng() % 2];
        else out += alpha[rng() % alpha.size()];
    }
    if (mode == "palindrome" && len > 1) {
        out = out.substr(0, len / 2);
        std::string rev = out;
        std::reverse(rev.begin(), rev.end());
        out += rev;
    }
    std::cout << out << '\n';
}

void oracleBrute(ArchiveContext& ctx, const std::vector<std::string>& args) {
    std::cout << "[Oracle] Ruleaza comparatie optim vs brute (necesita brute.cpp in folder).\n";
    const int id = parseIdArg(ctx, args);
    const auto dir = ctx.pbinfoMain / ("pbinfo-" + std::to_string(id));
    if (!fs::exists(dir / "brute.cpp"))
        std::cout << "  Adauga brute.cpp langa main.cpp pentru #" << id << '\n';
    else std::cout << "  Foloseste --batch-test dupa compilare.\n";
}

void sandboxRun(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    std::cout << "[Sandbox] Introdu input (linie goala = ruleaza):\n";
    std::string input, line;
    while (std::getline(std::cin, line) && !line.empty()) input += line + '\n';
    const auto inFile = ctx.archiveDir / "sandbox.in";
    writeFile(inFile, input);
    std::cout << "Input salvat in " << inFile.string() << ". Compileaza si ruleaza manual.\n";
}

void vaultSave(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cout << "--vault-save <nume>\n"; return; }
    const auto src = ctx.archiveDir / "sandbox.in";
    if (!fs::exists(src)) { std::cout << "Nu exista sandbox.in\n"; return; }
    const auto dst = ctx.vaultDir / (args[0] + ".in");
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    std::cout << "[Vault] Salvat: " << dst.string() << '\n';
}

void validateFloat(ArchiveContext&, const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cout << "--validate-float <a> <b> [eps]\n"; return; }
    const double a = std::stod(args[0]), b = std::stod(args[1]);
    const double eps = args.size() > 2 ? std::stod(args[2]) : 1e-5;
    std::cout << (std::fabs(a - b) < eps ? "PASS\n" : "FAIL\n");
}

void compareLoose(ArchiveContext&, const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cout << "--compare-loose <fisier1> <fisier2>\n"; return; }
    auto norm = [](std::string s) {
        std::istringstream iss(s);
        std::ostringstream oss;
        std::string w;
        while (iss >> w) oss << w << ' ';
        return oss.str();
    };
    const auto a = norm(readFile(args[0])), b = norm(readFile(args[1]));
    std::cout << (a == b ? "MATCH (whitespace-insensitive)\n" : "DIFFER\n");
}

void timeoutRun(ArchiveContext& ctx, const std::vector<std::string>& args) {
    std::cout << "[Timeout] Limita 3s pentru executie (configureaza in --batch-test).\n";
    (void)ctx; (void)args;
}

void batchTest(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto testDir = ctx.pbinfoMain / ("pbinfo-" + std::to_string(id)) / "tests";
    if (!fs::is_directory(testDir)) {
        std::cout << "[Batch] Nu exista folder tests/ pentru #" << id << '\n';
        return;
    }
    int pass = 0, total = 0;
    for (const auto& e : fs::directory_iterator(testDir)) {
        if (e.path().extension() != ".in") continue;
        ++total;
        std::cout << "  " << e.path().filename().string() << " [?]\n";
    }
    std::cout << "[Batch] " << total << " fisiere .in gasite. Compileaza main.cpp pentru rulare.\n";
    (void)pass;
}

// ===================== MODULE 4: UI (31-40) =====================

void demoProgress(ArchiveContext& ctx, const std::vector<std::string>&) {
    for (size_t i = 0; i <= 100; ++i) {
        asciiProgress("[Load]", i, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    beepIfEnabled(ctx);
}

void setTheme(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "Teme: classic, matrix, neon, monokai, stealth\n";
        return;
    }
    ctx.terminalTheme = args[0];
    scriptSaveConfig(ctx);
    themeColor(ctx);
    std::cout << "Tema activa: " << ctx.terminalTheme << RESET << '\n';
}

void showTimer(ArchiveContext& ctx, const std::vector<std::string>&) {
    const auto now = std::chrono::steady_clock::now();
    const auto sess = std::chrono::duration_cast<std::chrono::seconds>(now - ctx.sessionStart).count();
    std::cout << "[Timer] Sesiune: " << sess << "s";
    if (ctx.problemTimerRunning) {
        const auto p = std::chrono::duration_cast<std::chrono::seconds>(now - ctx.problemTimerStart).count();
        std::cout << " | Problema activa #" << ctx.activeProblemId << ": " << p << "s";
    }
    std::cout << '\n';
}

void listCompact(ArchiveContext& ctx, const std::vector<std::string>&) {
    scriptPrintAvailableProblems(ctx, true);
}

void wrapText(ArchiveContext&, const std::vector<std::string>& args) {
    if (args.empty()) return;
    const int width = 80;
    std::string text = args[0];
    for (size_t i = 1; i < args.size(); ++i) text += ' ' + args[i];
    for (size_t i = 0; i < text.size(); i += width)
        std::cout << text.substr(i, width) << '\n';
}

void toggleSound(ArchiveContext& ctx, const std::vector<std::string>&) {
    ctx.soundAlerts = !ctx.soundAlerts;
    scriptSaveConfig(ctx);
    std::cout << "[Sound] " << (ctx.soundAlerts ? "ON" : "OFF") << '\n';
    beepIfEnabled(ctx);
}

void peekCode(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    std::istringstream in(code);
    std::string line;
    int n = 0;
    while (std::getline(in, line) && n < 25) {
        std::cout << "| " << line << '\n';
        ++n;
    }
    if (n >= 25) std::cout << "| ... (foloseste id numeric pentru tot codul)\n";
}

void navKeysInfo(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "[Nav] In modul interactiv: taste numerice. Extensie viitoare: sageti/WASD.\n";
}

void leaderboard(ArchiveContext& ctx, const std::vector<std::string>&) {
    const auto lb = ctx.archiveDir / "leaderboard.txt";
    if (!fs::exists(lb)) { std::cout << "[Leaderboard] Gol. Adauga: id,timp_ms\n"; return; }
    std::cout << readFile(lb);
}

void diagnostics(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::cout << "[Diagnostics]\n";
    std::cout << "  Arhiva: " << ctx.pbinfoMain.string() << '\n';
    std::cout << "  Probleme: " << ctx.problemIds.size() << '\n';
    std::cout << "  Tema: " << ctx.terminalTheme << '\n';
#ifdef _WIN32
    system("g++ --version 2>nul | findstr /C:\"g++\"");
#else
    system("g++ --version 2>/dev/null | head -1");
#endif
    std::cout << "  Flags recomandate: -std=c++17 -O2\n";
}

// ===================== MODULE 5: Workspace (41-50) =====================

void cleanupTemp(ArchiveContext& ctx, const std::vector<std::string>&) {
    int removed = 0;
    for (const auto& root : {ctx.projectRoot, ctx.archiveDir}) {
        if (!fs::exists(root)) continue;
        for (const auto& e : fs::recursive_directory_iterator(root)) {
            if (!e.is_regular_file()) continue;
            const auto ext = e.path().extension().string();
            if (ext == ".exe" || ext == ".out" || ext == ".tmp" || ext == ".obj") {
                fs::remove(e.path());
                ++removed;
            }
        }
    }
    std::cout << "[Cleanup] Sterse " << removed << " fisiere temporare.\n";
}

void versionSolution(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    std::string tag = "v1";
    if (args.size() > 1) tag = args[1];
    const auto src = scriptSolutionPath(ctx, id);
    const auto dst = src.parent_path() / ("sol_" + tag + ".cpp");
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    std::cout << "[Version] Copiat la " << dst.string() << '\n';
}

void migrateWorkspace(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cout << "--migrate-workspace <dest_path>\n"; return; }
    const fs::path dest = args[0];
    fs::create_directories(dest);
    fs::copy(ctx.pbinfoMain, dest / "pbinfo-main", fs::copy_options::recursive);
    fs::copy(ctx.archiveDir, dest / ".archive", fs::copy_options::recursive);
    std::cout << "[Migrate] Copiat arhiva in " << fs::absolute(dest).string() << '\n';
}

void safeguardOverwrite(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto p = scriptSolutionPath(ctx, id);
    if (fs::exists(p) && fs::file_size(p) > 50)
        std::cout << "[Safeguard] ATENTIE: fisier existent cu continut. Confirma manual inainte de suprascriere.\n";
    else std::cout << "[Safeguard] Safe to write.\n";
}

void attachLink(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cout << "--attach-link <id> <url sau nota>\n"; return; }
    const int id = std::stoi(args[0]);
    const auto meta = ctx.archiveDir / ("meta_" + std::to_string(id) + ".txt");
    std::string content = fs::exists(meta) ? readFile(meta) : "";
    content += "link=" + args[1] + "\n";
    writeFile(meta, content);
    std::cout << "[Attach] Meta actualizat pentru #" << id << '\n';
}

void deadCodeFinder(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    std::regex varDecl(R"(\b(int|long long|double)\s+(\w+)\s*[=;])");
    for (std::sregex_iterator it(code.begin(), code.end(), varDecl), end; it != end; ++it) {
        const std::string name = (*it)[2].str();
        if (name == "main") continue;
        const std::regex use("\\b" + name + "\\b");
        auto begin = std::sregex_iterator(code.begin(), code.end(), use);
        if (std::distance(begin, std::sregex_iterator()) <= 1)
            std::cout << "  Posibil nefolosit: " << name << '\n';
    }
}

void headerMetaGenerator(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    auto code = scriptReadSolutionSource(ctx, id);
    const auto header = "// Problem: " + std::to_string(id) + " | Language: C++\n";
    if (code.find("// Problem:") == std::string::npos) {
        writeFile(scriptSolutionPath(ctx, id), header + code);
        std::cout << "[Header] Meta injectat.\n";
    }
}

void openEditor(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto p = scriptSolutionPath(ctx, id);
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + p.string() + "\"";
#else
    std::string cmd = "xdg-open \"" + p.string() + "\"";
#endif
    std::system(cmd.c_str());
}

void rotateLogs(ArchiveContext& ctx, const std::vector<std::string>&) {
    const auto log = ctx.archiveDir / "history.log";
    if (fs::exists(log) && fs::file_size(log) > 50000) {
        const auto bak = ctx.archiveDir / "history.log.old";
        fs::rename(log, bak);
        std::cout << "[Logs] Rotit history.log\n";
    } else std::cout << "[Logs] OK, fara rotatie necesara.\n";
}

void syncIntegrity(ArchiveContext& ctx, const std::vector<std::string>&) {
    int repaired = 0;
    for (int id : ctx.problemIds) {
        const auto dir = ctx.pbinfoMain / ("pbinfo-" + std::to_string(id));
        const auto tests = dir / "tests";
        if (!fs::exists(tests)) {
            fs::create_directories(tests);
            ++repaired;
        }
    }
    std::cout << "[Sync] Create tests/ pentru " << repaired << " probleme.\n";
}

// ===================== MODULE 6: Reference (51-60) =====================

void cheatStl(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "vector: push_back, pop_back, size, clear\n";
    std::cout << "set: insert, count, lower_bound, erase\n";
    std::cout << "map: operator[], find, insert\n";
}

void cheatMath(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "GCD: __gcd(a,b) sau Euclid\nLCM: a*b/gcd\n";
    std::cout << "Prim: trial division pana la sqrt(n)\n";
    std::cout << "Comb: C(n,k) = n!/(k!(n-k)!)\n";
}

void vizBits(ArchiveContext&, const std::vector<std::string>& args) {
    if (args.empty()) { std::cout << "--viz-bits <numar>\n"; return; }
    unsigned int v = std::stoul(args[0]);
    std::cout << "Decimal: " << v << "\nBinary: ";
    for (int i = 31; i >= 0; --i) std::cout << ((v >> i) & 1);
    std::cout << "\nSet bit i: v | (1<<i)  Clear: v & ~(1<<i)\n";
}

void cheatTypes(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "int: ~2e9 | long long: ~9e18\n";
    std::cout << "float/double: precizie limitata\n";
    std::cout << "unsigned: doar >= 0\n";
}

void cheatGraph(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "BFS: queue, dist[], viz[]\nDFS: recursiv/stack, viz[]\n";
}

void cheatString(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "substr(pos,len) | find | size\nC: strtok, strstr\n";
}

void cheatAscii(ArchiveContext&, const std::vector<std::string>&) {
    for (int c = 32; c < 127; c += 16) {
        for (int j = 0; j < 16 && c + j < 127; ++j)
            std::cout << std::setw(4) << (c + j) << (char)(c + j) << ' ';
        std::cout << '\n';
    }
}

void cheatDp(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "Knapsack: dp[i][w] max valoare\nLCS: dp[i][j] daca s1[i]==s2[j]\n";
}

void cheatSort(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "Bubble: comparari adiacente\nQuick: pivot, divide\nMerge: imparte, combina\n";
}

void configReset(ArchiveContext& ctx, const std::vector<std::string>&) {
    ctx.terminalTheme = "classic";
    ctx.soundAlerts = false;
    ctx.productionMode = false;
    ctx.config.clear();
    if (fs::exists(ctx.configPath)) fs::remove(ctx.configPath);
    std::cout << "[Config] Reset la valorile implicite.\n";
}

// ===================== BASIC 20 (custom) =====================

void basicCount(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::cout << "Total probleme: " << ctx.problemIds.size() << '\n';
}

void basicPath(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    std::cout << scriptSolutionPath(ctx, id).string() << '\n';
}

void basicLast(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::cout << "Ultima activa: #" << ctx.activeProblemId << '\n';
}

void basicRandom(ArchiveContext& ctx, const std::vector<std::string>&) {
    if (ctx.problemIds.empty()) return;
    std::mt19937 rng(std::random_device{}());
    const int id = ctx.problemIds[rng() % ctx.problemIds.size()];
    std::cout << "Random: #" << id << '\n';
    scriptShowSolution(ctx, id);
}

void basicSearchId(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (args.empty()) return;
    const std::string q = args[0];
    for (int id : ctx.problemIds) {
        if (std::to_string(id).find(q) != std::string::npos)
            std::cout << '#' << id << ' ';
    }
    std::cout << '\n';
}

void basicStatsBits(ArchiveContext& ctx, const std::vector<std::string>&) {
    int c = 0;
    for (int id : ctx.problemIds)
        if (scriptReadSolutionSource(ctx, id).find("bits/stdc++.h") != std::string::npos) ++c;
    std::cout << "Cu bits/stdc++.h: " << c << " / " << ctx.problemIds.size() << '\n';
}

void basicStatsFio(ArchiveContext& ctx, const std::vector<std::string>&) {
    int file = 0, cin = 0;
    for (int id : ctx.problemIds) {
        const auto code = scriptReadSolutionSource(ctx, id);
        if (code.find("ifstream") != std::string::npos) ++file;
        if (code.find("cin") != std::string::npos) ++cin;
    }
    std::cout << "File I/O: " << file << " | cin/cout: " << cin << '\n';
}

void basicExportList(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::ostringstream ss;
    for (int id : ctx.problemIds) ss << id << '\n';
    const auto out = ctx.archiveDir / "export_ids.txt";
    writeFile(out, ss.str());
    std::cout << "Exportat: " << out.string() << '\n';
}

void basicFileSize(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto p = scriptSolutionPath(ctx, id);
    if (fs::exists(p)) std::cout << fs::file_size(p) << " bytes\n";
}

void basicDuplicates(ArchiveContext& ctx, const std::vector<std::string>&) {
    std::map<int, int> seen;
    for (int id : ctx.problemIds) ++seen[id];
    int dup = 0;
    for (const auto& kv : seen) if (kv.second > 1) ++dup;
    std::cout << "Duplicate ID entries: " << dup << '\n';
}

void basicFindEmpty(ArchiveContext& ctx, const std::vector<std::string>&) {
    for (int id : ctx.problemIds) {
        const auto p = scriptSolutionPath(ctx, id);
        if (fs::file_size(p) < 10) std::cout << "  #" << id << " (gol)\n";
    }
}

void basicBenchCompile(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto src = scriptSolutionPath(ctx, id);
    const auto exe = ctx.archiveDir / "bench_tmp.exe";
    const auto t0 = std::chrono::steady_clock::now();
#ifdef _WIN32
    const std::string cmd = "g++ -std=c++17 -O2 \"" + src.string() + "\" -o \"" + exe.string() + "\" 2>nul";
#else
    const std::string cmd = "g++ -std=c++17 -O2 \"" + src.string() + "\" -o \"" + exe.string() + "\" 2>/dev/null";
#endif
    const int rc = std::system(cmd.c_str());
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << "[Bench] Compile " << (rc == 0 ? "OK" : "FAIL") << " in " << ms << "ms\n";
}

void basicHelp(ArchiveContext&, const std::vector<std::string>&) {
    scriptPrintMasterHelp();
}

void basicVersion(ArchiveContext&, const std::vector<std::string>&) {
    std::cout << "Arhiva pbinfo v2.0 | 60 module + 20 basic | script.h handler\n";
}

void basicSetActive(ArchiveContext& ctx, const std::vector<std::string>& args) {
    ctx.activeProblemId = parseIdArg(ctx, args);
    ctx.problemTimerStart = std::chrono::steady_clock::now();
    ctx.problemTimerRunning = true;
    std::cout << "Activ: #" << ctx.activeProblemId << '\n';
}

void basicLoc(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    const auto code = scriptReadSolutionSource(ctx, id);
    std::cout << "Linii: " << std::count(code.begin(), code.end(), '\n') + 1 << '\n';
}

void basicFindRecursion(ArchiveContext& ctx, const std::vector<std::string>&) {
    int c = 0;
    for (int id : ctx.problemIds) {
        const auto code = scriptReadSolutionSource(ctx, id);
        if (code.find("return ") != std::string::npos &&
            std::regex_search(code, std::regex(R"(\w+\s*\([^)]*\)\s*\{[\s\S]*return\s+\w+\s*\()")))
            std::cout << '#' << id << ' ', ++c;
    }
    std::cout << "\nTotal: " << c << '\n';
}

void basicClear(ArchiveContext&, const std::vector<std::string>&) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void basicPbinfoLink(ArchiveContext& ctx, const std::vector<std::string>& args) {
    const int id = parseIdArg(ctx, args);
    std::cout << "https://www.pbinfo.ro/?pag=problema&id=" << id << '\n';
}

void runAnalyzeBundle(ArchiveContext& ctx, const std::vector<std::string>& args) {
    analyzeComplexity(ctx, args);
    analyzeSpace(ctx, args);
    lintPbinfo(ctx, args);
    checkFastIo(ctx, args);
    checkRecursion(ctx, args);
}

void runSearchBundle(ArchiveContext& ctx, const std::vector<std::string>& args) {
    if (!args.empty()) searchKeywords(ctx, args);
    else std::cout << "Usage: --search <kw1> [kw2...]\n";
}

void runTestBundle(ArchiveContext& ctx, const std::vector<std::string>& args) {
    genRandomArray(ctx, args);
}

} // namespace

// ===================== Public API =====================

fs::path scriptFindPbinfoMainDir() {
    fs::path dir = fs::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        const fs::path candidate = dir / "pbinfo-main";
        if (fs::is_directory(candidate)) return candidate;
        if (!dir.has_parent_path() || dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    return fs::path("pbinfo-main");
}

fs::path scriptFindProjectRoot(const fs::path& pbinfoMain) {
    return pbinfoMain.parent_path();
}

std::vector<int> scriptLoadProblemIds(const fs::path& pbinfoMain) {
    std::vector<int> ids;
    if (!fs::is_directory(pbinfoMain)) return ids;
    for (const auto& entry : fs::directory_iterator(pbinfoMain)) {
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind("pbinfo-", 0) != 0) continue;
        try {
            const int id = std::stoi(name.substr(7));
            if (fs::is_regular_file(entry.path() / "main.cpp")) ids.push_back(id);
        } catch (...) {}
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

void scriptInitArchive(ArchiveContext& ctx) {
    ctx.pbinfoMain = scriptFindPbinfoMainDir();
    ctx.projectRoot = scriptFindProjectRoot(ctx.pbinfoMain);
    ctx.archiveDir = ctx.projectRoot / ".archive";
    ctx.vaultDir = ctx.archiveDir / "vault";
    ctx.configPath = ctx.archiveDir / "config.ini";
    fs::create_directories(ctx.archiveDir);
    fs::create_directories(ctx.vaultDir);
    ctx.problemIds = scriptLoadProblemIds(ctx.pbinfoMain);
    ctx.sessionStart = std::chrono::steady_clock::now();
    scriptLoadConfig(ctx);
}

fs::path scriptSolutionPath(const ArchiveContext& ctx, int id) {
    return ctx.pbinfoMain / ("pbinfo-" + std::to_string(id)) / "main.cpp";
}

bool scriptProblemExists(const ArchiveContext& ctx, int id) {
    return fs::is_regular_file(scriptSolutionPath(ctx, id));
}

std::string scriptReadSolutionSource(const ArchiveContext& ctx, int id) {
    if (id < 0) return {};
    return readFile(scriptSolutionPath(ctx, id));
}

bool scriptShowSolution(const ArchiveContext& ctx, int id) {
    const auto code = scriptReadSolutionSource(ctx, id);
    if (code.empty()) {
        std::cout << "Problema #" << id << " nu exista.\n";
        return false;
    }
    std::cout << "\n=== Solutie pbinfo #" << id << " ===\n" << code
              << "\n=== Sfarsit ===\n";
    std::cout << "Enunt: https://www.pbinfo.ro/?pag=problema&id=" << id << '\n';
    return true;
}

void scriptPrintAvailableProblems(const ArchiveContext& ctx, bool compact) {
    if (ctx.problemIds.empty()) {
        std::cout << "Nu am gasit probleme.\n";
        return;
    }
    std::cout << "Probleme (" << ctx.problemIds.size() << "):\n";
    const int perLine = compact ? 20 : 12;
    for (size_t i = 0; i < ctx.problemIds.size(); ++i) {
        std::cout << '#' << ctx.problemIds[i];
        if ((i + 1) % perLine == 0 || i + 1 == ctx.problemIds.size()) std::cout << '\n';
        else std::cout << (compact ? " " : "  ");
    }
}

void scriptLoadConfig(ArchiveContext& ctx) {
    if (!fs::exists(ctx.configPath)) return;
    std::ifstream in(ctx.configPath);
    std::string line, key, val;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        key = line.substr(0, eq);
        val = line.substr(eq + 1);
        ctx.config[key] = val;
        if (key == "theme") ctx.terminalTheme = val;
        if (key == "sound") ctx.soundAlerts = (val == "1");
        if (key == "production") ctx.productionMode = (val == "1");
    }
}

void scriptSaveConfig(ArchiveContext& ctx) {
    std::ostringstream ss;
    ss << "theme=" << ctx.terminalTheme << '\n';
    ss << "sound=" << (ctx.soundAlerts ? "1" : "0") << '\n';
    ss << "production=" << (ctx.productionMode ? "1" : "0") << '\n';
    writeFile(ctx.configPath, ss.str());
}

std::vector<std::string> scriptTokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string t;
    while (iss >> t) tokens.push_back(t);
    return tokens;
}

void scriptRegisterAllCommands(std::map<std::string, ScriptHandler>& routes) {
    // Module 1 --analyze
    routes["--analyze"] = runAnalyzeBundle;
    routes["--analyze-complexity"] = analyzeComplexity;
    routes["--analyze-space"] = analyzeSpace;
    routes["--lint-pbinfo"] = lintPbinfo;
    routes["--sweep-headers"] = sweepHeaders;
    routes["--inject-snippet"] = injectSnippet;
    routes["--strip-comments"] = stripComments;
    routes["--check-fastio"] = checkFastIo;
    routes["--check-recursion"] = checkRecursion;
    routes["--suggest-bitwise"] = suggestBitwise;
    routes["--extract-constants"] = extractConstants;

    // Module 2 --search
    routes["--search"] = runSearchBundle;
    routes["--search-keywords"] = searchKeywords;
    routes["--sort-success-rate"] = sortSuccessRate;
    routes["--suggest-score"] = suggestScore;
    routes["--tag-cloud"] = tagCloud;
    routes["--similar"] = similarProblems;
    routes["--filter-author"] = filterAuthor;
    routes["--filter-id-range"] = filterIdRange;
    routes["--flag-missing-meta"] = flagMissingMeta;
    routes["--filter-memory"] = filterMemory;
    routes["--sort-time-limit"] = sortTimeLimit;

    // Module 3 --test
    routes["--test"] = runTestBundle;
    routes["--gen-array"] = genRandomArray;
    routes["--gen-graph"] = genRandomGraph;
    routes["--gen-string"] = genRandomString;
    routes["--oracle-brute"] = oracleBrute;
    routes["--sandbox"] = sandboxRun;
    routes["--vault-save"] = vaultSave;
    routes["--validate-float"] = validateFloat;
    routes["--compare-loose"] = compareLoose;
    routes["--timeout-run"] = timeoutRun;
    routes["--batch-test"] = batchTest;

    // Module 4 --ui
    routes["--ui-progress"] = demoProgress;
    routes["--theme"] = setTheme;
    routes["--timer"] = showTimer;
    routes["--list-compact"] = listCompact;
    routes["--wrap-text"] = wrapText;
    routes["--sound"] = toggleSound;
    routes["--peek"] = peekCode;
    routes["--nav-info"] = navKeysInfo;
    routes["--leaderboard"] = leaderboard;
    routes["--diagnostics"] = diagnostics;

    // Module 5 --workspace
    routes["--cleanup"] = cleanupTemp;
    routes["--version-solution"] = versionSolution;
    routes["--migrate-workspace"] = migrateWorkspace;
    routes["--safeguard"] = safeguardOverwrite;
    routes["--attach-link"] = attachLink;
    routes["--dead-code"] = deadCodeFinder;
    routes["--header-meta"] = headerMetaGenerator;
    routes["--open-editor"] = openEditor;
    routes["--rotate-logs"] = rotateLogs;
    routes["--sync-integrity"] = syncIntegrity;

    // Module 6 --reference
    routes["--cheat-stl"] = cheatStl;
    routes["--cheat-math"] = cheatMath;
    routes["--viz-bits"] = vizBits;
    routes["--cheat-types"] = cheatTypes;
    routes["--cheat-graph"] = cheatGraph;
    routes["--cheat-string"] = cheatString;
    routes["--cheat-ascii"] = cheatAscii;
    routes["--cheat-dp"] = cheatDp;
    routes["--cheat-sort"] = cheatSort;
    routes["--config-reset"] = configReset;

    // Basic 20
    routes["--count"] = basicCount;
    routes["--path"] = basicPath;
    routes["--last"] = basicLast;
    routes["--random"] = basicRandom;
    routes["--search-id"] = basicSearchId;
    routes["--stats-bits"] = basicStatsBits;
    routes["--stats-fio"] = basicStatsFio;
    routes["--export-list"] = basicExportList;
    routes["--file-size"] = basicFileSize;
    routes["--check-duplicates"] = basicDuplicates;
    routes["--find-empty"] = basicFindEmpty;
    routes["--bench-compile"] = basicBenchCompile;
    routes["--help"] = basicHelp;
    routes["-h"] = basicHelp;
    routes["--version"] = basicVersion;
    routes["--set-active"] = basicSetActive;
    routes["--loc"] = basicLoc;
    routes["--find-recursion-all"] = basicFindRecursion;
    routes["--clear"] = basicClear;
    routes["--link"] = basicPbinfoLink;
    routes["--help-basic"] = [](ArchiveContext&, const std::vector<std::string>&) {
        std::cout << "Basic: --count --path --last --random --search-id --stats-bits --stats-fio\n";
        std::cout << "       --export-list --file-size --check-duplicates --find-empty\n";
        std::cout << "       --bench-compile --set-active --loc --find-recursion-all --clear --link\n";
    };
    routes["--help-modules"] = [](ArchiveContext&, const std::vector<std::string>& args) {
        int m = 0;
        if (!args.empty()) m = std::stoi(args[0]);
        scriptPrintModuleHelp(m);
    };
}

bool scriptHandleInput(ArchiveContext& ctx, const std::string& line,
                       const std::map<std::string, ScriptHandler>& routes) {
    auto tokens = scriptTokenize(line);
    if (tokens.empty()) return false;

    if (tokens[0][0] != '-' && tokens[0] != "help" && tokens[0] != "analyze" &&
        tokens[0] != "search" && tokens[0] != "test")
        return false;

    std::string cmd = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    if (cmd == "help") { scriptPrintMasterHelp(); return true; }
    if (cmd == "analyze" && !args.empty()) {
        cmd = "--analyze-" + args[0];
        args.erase(args.begin());
    } else if (cmd == "analyze") {
        cmd = "--analyze";
    }
    if (cmd == "search") cmd = "--search";
    if (cmd == "test") cmd = "--test";

    const auto it = routes.find(cmd);
    if (it == routes.end()) {
        std::cout << "Comanda necunoscuta: " << cmd << ". Foloseste --help\n";
        return true;
    }
    ctx.commandHistory.push_back(line);
    it->second(ctx, args);
    return true;
}

void scriptPrintModuleHelp(int module) {
    auto print = [module](int m, const char* title, std::initializer_list<const char*> cmds) {
        if (module != 0 && module != m) return;
        std::cout << "\n[" << title << "]\n";
        for (const char* c : cmds) std::cout << "  " << c << '\n';
    };
    print(1, "MODULE 1: Analyze", {
        "--analyze [id]", "--analyze-complexity", "--analyze-space", "--lint-pbinfo",
        "--sweep-headers", "--inject-snippet <key>", "--strip-comments", "--check-fastio",
        "--check-recursion", "--suggest-bitwise", "--extract-constants"
    });
    print(2, "MODULE 2: Search", {
        "--search <kw...>", "--search-keywords", "--sort-success-rate", "--suggest-score",
        "--tag-cloud", "--similar [id]", "--filter-author", "--filter-id-range <a> <b>",
        "--flag-missing-meta", "--filter-memory", "--sort-time-limit"
    });
    print(3, "MODULE 3: Test", {
        "--test", "--gen-array [n lo hi]", "--gen-graph", "--gen-string",
        "--oracle-brute", "--sandbox", "--vault-save", "--validate-float",
        "--compare-loose", "--timeout-run", "--batch-test"
    });
    print(4, "MODULE 4: UI", {
        "--ui-progress", "--theme <name>", "--timer", "--list-compact", "--wrap-text",
        "--sound", "--peek [id]", "--nav-info", "--leaderboard", "--diagnostics"
    });
    print(5, "MODULE 5: Workspace", {
        "--cleanup", "--version-solution", "--migrate-workspace", "--safeguard",
        "--attach-link", "--dead-code", "--header-meta", "--open-editor",
        "--rotate-logs", "--sync-integrity"
    });
    print(6, "MODULE 6: Reference", {
        "--cheat-stl", "--cheat-math", "--viz-bits <n>", "--cheat-types",
        "--cheat-graph", "--cheat-string", "--cheat-ascii", "--cheat-dp",
        "--cheat-sort", "--config-reset"
    });
    if (module == 0 || module == 7)
        std::cout << "\n[BASIC 20] --help-basic\n";
}

void scriptPrintMasterHelp() {
    std::cout << "\n=== Arhiva pbinfo - Script Handler (80 functii) ===\n";
    std::cout << "Rutare: --analyze | --search | --test | --ui-* | --workspace | --cheat-*\n";
    std::cout << "Scrie --help-modules [1-6] pentru detalii pe modul.\n";
    std::cout << "Comenzi rapide: help | analyze <id> | 99 | 0 | <id problema>\n\n";
    scriptPrintModuleHelp(0);
}
