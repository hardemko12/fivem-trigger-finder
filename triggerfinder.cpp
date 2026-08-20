#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <map>
#include <ctime>
#include <future>
#include <mutex>
#include <cctype>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <set>
#include <chrono>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

std::string get_executable_directory();

std::string to_lower(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (unsigned char c : str)
        result += static_cast<char>(std::tolower(c));

    return result;
}

std::string get_timestamp() {
    std::time_t now = std::time(nullptr);
    char buffer[20];

#ifdef _WIN32
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &timeinfo);
#else
    struct tm* timeinfo = std::localtime(&now);

    if (timeinfo)
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", timeinfo);
    else
        return "unknown_time";
#endif

    return std::string(buffer);
}

std::string json_escape(const std::string& s) {
    std::ostringstream oss;

    for (unsigned char c : s) {
        switch (c) {
        case '"':
            oss << "\\\"";
            break;
        case '\\':
            oss << "\\\\";
            break;
        case '\b':
            oss << "\\b";
            break;
        case '\f':
            oss << "\\f";
            break;
        case '\n':
            oss << "\\n";
            break;
        case '\r':
            oss << "\\r";
            break;
        case '\t':
            oss << "\\t";
            break;
        default:
            if (c < 0x20) {
                oss << "\\u"
                    << std::hex
                    << std::setw(4)
                    << std::setfill('0')
                    << static_cast<int>(c)
                    << std::dec;
            } else {
                oss << static_cast<char>(c);
            }
        }
    }

    return oss.str();
}

struct ScanConfig {
    std::string dump_path;
    std::string output_path;
    std::vector<std::string> exclude_dirs = {
        "node_modules",
        ".git",
        "cache"
    };
    std::unordered_set<std::string> ignore_triggers;
    std::vector<std::regex> ignore_patterns;
    int thread_count = 0;
    bool json_output = false;
    bool verbose = false;
};

std::unordered_set<std::string> default_ignore_triggers() {
    return {
        "event",
        "Event",
        "onEvent",
        "trigger",
        "Trigger",
        "playerConnecting",
        "playerDropped",
        "onClientResourceStart",
        "onClientResourceStop",
        "onResourceStart",
        "onResourceStop",
        "onClientMapStart",
        "onClientGameTypeStart"
    };
}

static const std::unordered_set<std::string> SUPPORTED_EXTENSIONS = {
    ".lua",
    ".js",
    ".json",
    ".xml",
    ".cfg",
    ".txt",
    ".sql",
    ".yaml",
    ".yml",
    ".html",
    ".css",
    ".md",
    ".ini",
    ".toml",
    ".mjs",
    ".cjs",
    ".ts"
};

struct Trigger {
    std::string file_path;
    std::string trigger_name;
    std::string line_content;
    int line_number;
    std::string trigger_type;
};

struct TriggerPattern {
    std::regex pattern;
    int capture_group;
    std::string type;
    std::string description;

    TriggerPattern(
        const std::string& pat,
        int capture,
        std::string t,
        std::string desc
    )
        : pattern(pat, std::regex::icase),
          capture_group(capture),
          type(std::move(t)),
          description(std::move(desc)) {
    }
};

enum class LangType {
    Lua,
    JsLike,
    Unknown
};

LangType detect_language(const std::string& ext) {
    if (ext == ".lua")
        return LangType::Lua;

    if (
        ext == ".js" ||
        ext == ".mjs" ||
        ext == ".cjs" ||
        ext == ".ts" ||
        ext == ".html" ||
        ext == ".css" ||
        ext == ".json"
    )
        return LangType::JsLike;

    return LangType::Unknown;
}

std::string strip_comments(const std::string& line, LangType lang) {
    if (line.empty())
        return "";

    std::string result;
    result.reserve(line.length());

    bool in_sq = false;
    bool in_dq = false;
    bool in_bt = false;
    bool in_lb = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        char next = (i + 1 < line.length()) ? line[i + 1] : '\0';
        bool escaped = (i > 0 && line[i - 1] == '\\');

        if (!escaped) {
            if (c == '\'' && !in_dq && !in_bt && !in_lb)
                in_sq = !in_sq;
            else if (c == '"' && !in_sq && !in_bt && !in_lb)
                in_dq = !in_dq;
            else if (lang == LangType::JsLike && c == '`' && !in_sq && !in_dq)
                in_bt = !in_bt;
            else if (lang == LangType::Lua && c == '[' && next == '[' && !in_sq && !in_dq)
                in_lb = true;
            else if (lang == LangType::Lua && c == ']' && next == ']' && in_lb)
                in_lb = false;
        }

        if (!in_sq && !in_dq && !in_bt && !in_lb) {
            if (lang == LangType::Lua && c == '-' && next == '-')
                return result;

            if (lang == LangType::JsLike && c == '/' && next == '/')
                return result;
        }

        result += c;
    }

    return result;
}

class ResultAggregator {
public:
    void add_batch(const std::vector<Trigger>& batch) {
        if (batch.empty())
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_triggers.insert(
            m_triggers.end(),
            batch.begin(),
            batch.end()
        );
    }

    std::vector<Trigger> steal() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::move(m_triggers);
    }

private:
    mutable std::mutex m_mutex;
    std::vector<Trigger> m_triggers;
};

class ProgressTracker {
public:
    explicit ProgressTracker(size_t total)
        : m_total(total),
          m_current(0) {
    }

    void tick() {
        size_t val = m_current.fetch_add(1) + 1;

        if (
            m_total > 0 &&
            val % (std::max<size_t>)(1, m_total / 100) == 0
        ) {
            int pct = static_cast<int>(
                (static_cast<double>(val) / m_total) * 100
            );

            std::cout
                << "\rProgress: "
                << pct
                << "% ("
                << val
                << "/"
                << m_total
                << " files)"
                << std::flush;
        }
    }

    void done() {
        std::cout
            << "\rProgress: 100% ("
            << m_total
            << "/"
            << m_total
            << " files)\n";
    }

private:
    size_t m_total;
    std::atomic<size_t> m_current;
};

std::vector<TriggerPattern> build_patterns() {
    std::vector<TriggerPattern> patterns;

    std::string Q = "['\"]";
    std::string ID = "([a-zA-Z_][a-zA-Z0-9_:]*)";
    std::string WS = "\\s*";
    std::string LP = "\\(";
    std::string RP = "\\)";

    patterns.emplace_back(
        "(?:Trigger(?:Server|Client|LatentClient)?Event|RegisterNetEvent)"
        + WS + LP + WS + Q + ID + Q,
        1,
        "TriggerNetEvent",
        "Trigger/Register net event"
    );

    patterns.emplace_back(
        "(?:AddEventHandler|RemoveEventHandler)"
        + WS + LP + WS + Q + ID + Q,
        1,
        "EventHandler",
        "Add/Remove event handler"
    );

    patterns.emplace_back(
        "(?:onNet|on)"
        + WS + LP + WS + Q + ID + Q,
        1,
        "on/onNet",
        "onNet/on event"
    );

    patterns.emplace_back(
        "(?:emitNet|emit)"
        + WS + LP + WS + Q + ID + Q,
        1,
        "emit/emitNet",
        "emit/emitNet event"
    );

    patterns.emplace_back(
        "exports"
        + WS
        + "(?:\\[" + WS + Q + "|" + LP + "?" + WS + Q + ")"
        + "([a-zA-Z_][a-zA-Z0-9_:]*)"
        + Q
        + "(?:\\]" + WS + "|" + WS + RP + "?)"
        + WS
        + "\\."
        + "\\w+"
        + WS
        + LP,
        1,
        "Export call",
        "Export call"
    );

    patterns.emplace_back(
        "RegisterExport"
        + WS + LP + WS + Q + ID + Q,
        1,
        "RegisterExport",
        "Register export"
    );

    patterns.emplace_back(
        "RegisterCommand"
        + WS + LP + WS + Q + ID + Q,
        1,
        "RegisterCommand",
        "Register command"
    );

    patterns.emplace_back(
        "AddStateBagChangeHandler"
        + WS + LP + WS + Q + ID + Q,
        1,
        "AddStateBagChangeHandler",
        "State bag change handler"
    );

    patterns.emplace_back(
        "GlobalState"
        + WS
        + ":"
        + WS
        + "set(?:Handler)?"
        + WS
        + LP
        + WS
        + Q
        + ID
        + Q,
        1,
        "GlobalState",
        "GlobalState set/setHandler"
    );

    patterns.emplace_back(
        "Player"
        + WS
        + LP
        + "[^)]*"
        + RP
        + WS
        + "\\."
        + WS
        + "set"
        + WS
        + LP
        + WS
        + Q
        + ID
        + Q,
        1,
        "PlayerState",
        "Player state bag set"
    );

    patterns.emplace_back(
        "(?:server|client)\\.emit"
        + WS
        + LP
        + WS
        + Q
        + ID
        + Q,
        1,
        "Socket emit",
        "Socket.io emit (alt:V)"
    );

    patterns.emplace_back(
        "RegisterUICallback"
        + WS
        + LP
        + WS
        + Q
        + ID
        + Q,
        1,
        "RegisterUICallback",
        "UI callback"
    );

    patterns.emplace_back(
        "SetHttpHandler"
        + WS
        + LP
        + WS
        + Q
        + "(/[a-zA-Z0-9_/]*)"
        + Q,
        1,
        "SetHttpHandler",
        "HTTP handler"
    );

    patterns.emplace_back(
        "AddMapLoader"
        + WS
        + LP
        + WS
        + Q
        + ID
        + Q,
        1,
        "AddMapLoader",
        "Map loader"
    );

    patterns.emplace_back(
        Q
        + "([a-zA-Z_][a-zA-Z0-9_:]*(?:Event|event|Trigger|trigger))"
        + Q
        + WS
        + "(?=[:=,])",
        1,
        "String Event",
        "String literal Event/Trigger"
    );

    return patterns;
}

class Scanner {
public:
    Scanner(
        ScanConfig config,
        std::vector<TriggerPattern> patterns
    )
        : m_config(std::move(config)),
          m_patterns(std::move(patterns)) {
    }

    std::vector<Trigger> scan() {
        if (!fs::exists(m_config.dump_path)) {
            std::cerr
                << "[ERROR] Path does not exist: "
                << m_config.dump_path
                << "\n";

            return {};
        }

        std::vector<std::string> file_list;

        for (
            const auto& entry :
            fs::recursive_directory_iterator(
                m_config.dump_path,
                fs::directory_options::skip_permission_denied
            )
        ) {
            if (!entry.is_regular_file())
                continue;

            std::string rel_path =
                relative_to_root(entry.path());

            if (is_excluded(rel_path))
                continue;

            std::string ext =
                to_lower(entry.path().extension().string());

            if (SUPPORTED_EXTENSIONS.count(ext))
                file_list.push_back(entry.path().string());
        }

        if (file_list.empty()) {
            std::cout << "No supported files found.\n";
            return {};
        }

        size_t total = file_list.size();

        std::cout
            << "Total files to scan: "
            << total
            << "\n\n";

        ResultAggregator aggregator;
        ProgressTracker progress(total);

        int threads =
            m_config.thread_count > 0
            ? m_config.thread_count
            : std::max(
                1,
                static_cast<int>(
                    std::thread::hardware_concurrency()
                ) - 1
            );

        std::vector<std::future<std::vector<Trigger>>> futures;
        std::atomic<size_t> file_idx(0);

        for (int i = 0; i < threads; ++i) {
            futures.push_back(
                std::async(
                    std::launch::async,
                    [&]() {
                        std::vector<Trigger> local_results;

                        while (true) {
                            size_t idx =
                                file_idx.fetch_add(1);

                            if (idx >= file_list.size())
                                break;

                            auto file_triggers =
                                scan_file(file_list[idx]);

                            local_results.insert(
                                local_results.end(),
                                file_triggers.begin(),
                                file_triggers.end()
                            );

                            progress.tick();
                        }

                        return local_results;
                    }
                )
            );
        }

        for (auto& f : futures)
            aggregator.add_batch(f.get());

        progress.done();

        return aggregator.steal();
    }

private:
    ScanConfig m_config;
    std::vector<TriggerPattern> m_patterns;

    std::string relative_to_root(
        const fs::path& p
    ) const {
        try {
            return fs::relative(
                p,
                m_config.dump_path
            ).string();
        }
        catch (...) {
            return p.string();
        }
    }

    bool is_excluded(
        const std::string& rel_path
    ) const {
        for (const auto& dir :
             m_config.exclude_dirs) {
            if (rel_path.find(dir) != std::string::npos)
                return true;
        }

        return false;
    }

    std::vector<Trigger> scan_file(
        const std::string& path
    ) {
        std::vector<Trigger> results;

        std::ifstream file(path);

        if (!file.is_open())
            return results;

        std::string rel_path =
            relative_to_root(path);

        std::string ext =
            to_lower(
                fs::path(path)
                    .extension()
                    .string()
            );

        LangType lang =
            detect_language(ext);

        std::string line;
        int line_num = 0;

        while (std::getline(file, line)) {
            line_num++;

            if (
                line.empty() ||
                line.length() > 4096
            )
                continue;

            std::string code =
                strip_comments(line, lang);

            if (code.empty())
                continue;

            for (const auto& p : m_patterns) {
                std::smatch match;
                auto search_start =
                    code.cbegin();

                while (
                    std::regex_search(
                        search_start,
                        code.cend(),
                        match,
                        p.pattern
                    )
                ) {
                    std::string name;

                    if (
                        p.capture_group <
                        static_cast<int>(match.size())
                    ) {
                        name =
                            match[p.capture_group].str();
                    }
                    else {
                        name =
                            match[0].str();
                    }

                    if (
                        m_config.ignore_triggers
                            .count(name)
                    ) {
                        search_start =
                            match.suffix().first;
                        continue;
                    }

                    Trigger t;

                    t.file_path = rel_path;
                    t.trigger_name = name;
                    t.line_content = line;
                    t.line_number = line_num;
                    t.trigger_type = p.description;

                    results.push_back(t);

                    search_start =
                        match.suffix().first;
                }
            }
        }

        return results;
    }
};

class ReportWriter {
public:
    static void write_txt(
        const std::string& out,
        const std::vector<Trigger>& trigs,
        const std::string& path,
        size_t count
    ) {
        std::ofstream file(out);

        if (!file.is_open())
            return;

        file
            << "FiveM Trigger Finder Report\n"
            << std::string(30, '=')
            << "\n";

        file
            << "Scan Path: "
            << path
            << "\n";

        file
            << "Files: "
            << count
            << "\n";

        file
            << "Triggers: "
            << trigs.size()
            << "\n\n";

        for (const auto& t : trigs) {
            file
                << "["
                << t.trigger_type
                << "] "
                << t.trigger_name
                << "\n";

            file
                << "  File: "
                << t.file_path
                << ":"
                << t.line_number
                << "\n";

            file
                << "  Line: "
                << t.line_content
                << "\n\n";
        }
    }

    static void write_json(
        const std::string& out,
        const std::vector<Trigger>& trigs,
        const std::string& path,
        size_t count
    ) {
        std::ofstream file(out);

        if (!file.is_open())
            return;

        file
            << "{\n"
            << "  \"path\": \""
            << json_escape(path)
            << "\",\n"
            << "  \"files\": "
            << count
            << ",\n"
            << "  \"triggers\": [\n";

        for (size_t i = 0; i < trigs.size(); ++i) {
            const auto& t = trigs[i];

            file
                << "    {\n"
                << "      \"name\": \""
                << json_escape(t.trigger_name)
                << "\",\n"
                << "      \"file\": \""
                << json_escape(t.file_path)
                << "\",\n"
                << "      \"line\": "
                << t.line_number
                << ",\n"
                << "      \"type\": \""
                << json_escape(t.trigger_type)
                << "\"\n"
                << "    }";

            if (i != trigs.size() - 1)
                file << ",";

            file << "\n";
        }

        file
            << "  ]\n"
            << "}";
    }
};

std::string get_executable_directory() {
#ifdef _WIN32
    char buffer[MAX_PATH];

    GetModuleFileNameA(
        NULL,
        buffer,
        MAX_PATH
    );

    std::string path(buffer);

    size_t pos =
        path.find_last_of("\\/");

    return
        pos != std::string::npos
        ? path.substr(0, pos)
        : ".";
#else
    char buffer[1024];

    ssize_t len =
        readlink(
            "/proc/self/exe",
            buffer,
            sizeof(buffer) - 1
        );

    if (len != -1) {
        buffer[len] = '\0';

        std::string path(buffer);

        size_t pos =
            path.find_last_of("/");

        return
            pos != std::string::npos
            ? path.substr(0, pos)
            : ".";
    }

    return ".";
#endif
}

int main(int argc, char* argv[]) {
    ScanConfig config;

    config.dump_path =
        get_executable_directory() + "/dump";

    config.output_path =
        "report_" + get_timestamp() + ".txt";

    config.ignore_triggers =
        default_ignore_triggers();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (
            arg == "--path" &&
            i + 1 < argc
        ) {
            config.dump_path = argv[++i];
        }
        else if (arg == "--json") {
            config.json_output = true;
        }
    }

    std::cout
        << "Starting scan in: "
        << config.dump_path
        << "\n";

    auto patterns = build_patterns();

    Scanner scanner(
        config,
        patterns
    );

    auto results =
        scanner.scan();

    if (config.json_output) {
        ReportWriter::write_json(
            config.output_path + ".json",
            results,
            config.dump_path,
            results.size()
        );
    }
    else {
        ReportWriter::write_txt(
            config.output_path,
            results,
            config.dump_path,
            results.size()
        );
    }

    std::cout
        << "\nFound "
        << results.size()
        << " triggers. Results saved to "
        << config.output_path
        << "\n";

    return 0;
}
