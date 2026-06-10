#include <smap1/codec.hpp>

#include "log_internal.hpp"

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace smap1::codec {

namespace {

// ---- File / JSON helpers ----
//
// calibration_codec.cpp / robot_model_codec.cpp keep the analogous helpers in
// their own anonymous namespace; same trade-off here — duplicating these small
// functions beats a shared internal header.

std::expected<std::string, Error> read_file(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) {
        return std::unexpected{Error{Error::Code::FileNotFound, "no such file: " + p.string()}};
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::unexpected{Error{Error::Code::IoError, "cannot open: " + p.string()}};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad()) {
        return std::unexpected{Error{Error::Code::IoError, "read failed: " + p.string()}};
    }
    return ss.str();
}

std::expected<nlohmann::json, Error> parse_json(std::string_view s, std::string_view label) {
    try {
        return nlohmann::json::parse(s);
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected{Error{Error::Code::ParseFailed,
            "JSON parse failed (" + std::string{label} + "): " + e.what()}};
    }
}

// 手写小写转换, 与 calibration_codec.cpp 一致 — 避免依赖 locale 相关的
// <cctype> std::tolower.
std::string to_lower(std::string_view s) {
    std::string out{s};
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// ---- SQLite RAII helpers (rbk34) ----

struct SqliteCloser {
    void operator()(sqlite3* db) const noexcept {
        if (db) sqlite3_close_v2(db);
    }
};
using SqlitePtr = std::unique_ptr<sqlite3, SqliteCloser>;

// 把任意文件路径编码进一个 authority-less 的 "file:" URI.  保留路径分隔符与
// unreserved 字符, 其余按 RFC 3986 百分号编码 (尤其 '?' '#' 与空格), 这样
// query 参数 (immutable=1) 不会被路径里的特殊字符破坏.
std::string path_to_file_uri(const std::filesystem::path& path) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out = "file:";
    for (unsigned char c : path.generic_string()) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '/' || c == '.' || c == '_' || c == '-' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

std::expected<SqlitePtr, Error> open_sqlite(const std::filesystem::path& path) {
    if (std::error_code ec; !std::filesystem::exists(path, ec)) {
        return std::unexpected{Error{Error::Code::FileNotFound, "no such file: " + path.string()}};
    }
    // immutable=1 告诉 SQLite 该库是只读快照: 跳过 WAL/journal 恢复, 不创建
    // -shm/-wal.  归档日志里的 robot.param 常处于 WAL 模式且位于只读位置,
    // 不加此标志时只读打开可能在首次查询返回 SQLITE_READONLY/CANTOPEN.
    std::string uri = path_to_file_uri(path) + "?immutable=1";
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(uri.c_str(), &db,
        SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db);
        if (db) sqlite3_close_v2(db);
        return std::unexpected{Error{Error::Code::IoError,
            "cannot open SQLite: " + path.string() + ": " + msg}};
    }
    return SqlitePtr{db};
}

// Execute SQL and invoke callback for each row.
// callback: (int nCols, char** colValues, char** colNames) -> bool (true=continue)
std::expected<void, Error> sqlite_exec(sqlite3* db, const char* sql,
                                       const std::function<bool(int, char**, char**)>& cb) {
    char* errmsg = nullptr;
    struct Ctx { const std::function<bool(int, char**, char**)>* cb; };
    Ctx ctx{&cb};
    auto lambda = [](void* p, int n, char** v, char** c) -> int {
        // sqlite3_exec aborts the query on non-zero return; our callback
        // returns true to continue, so map continue->0 and stop->non-zero.
        return (*static_cast<Ctx*>(p)->cb)(n, v, c) ? 0 : 1;
    };
    int rc = sqlite3_exec(db, sql, lambda, &ctx, &errmsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        // 文件不是合法 SQLite 库时 (rbk34 传错文件), SQLite 在首次查询时报
        // SQLITE_NOTADB; 按库的语义这是"解析失败"而非 I/O 失败.
        auto code = (rc == SQLITE_NOTADB) ? Error::Code::ParseFailed : Error::Code::IoError;
        return std::unexpected{Error{code, "SQLite query failed: " + msg}};
    }
    sqlite3_free(errmsg);
    return {};
}

// ---- rbk34: SQLite robot.param ----

// rbk34 每张表都是同一 schema:
//   Key TEXT NOT NULL UNIQUE, Type TEXT NOT NULL, Value TEXT NOT NULL,
//   Mutable BOOLEAN NOT NULL, DefaultValue TEXT NOT NULL
// 我们按 PRAGMA table_info 检测是否包含 Key / Value 列.
// 实际日志中所有表都符合此 schema.

struct ColumnMeta {
    std::string name;
    int idx = -1;  // column index in SELECT *
};
struct TableSchema {
    bool has_key  = false;
    int  key_col  = -1;
    bool has_type = false;
    int  type_col = -1;
    bool has_val  = false;
    int  val_col  = -1;
    bool has_mut  = false;
    int  mut_col  = -1;
    bool has_def  = false;
    int  def_col  = -1;
};

TableSchema detect_rbk34_schema(sqlite3* db, const std::string& table_name) {
    TableSchema s;
    // Default: we know the schema is Key(0), Type(1), Value(2), Mutable(3), DefaultValue(4)
    // but use PRAGMA for robustness.
    std::vector<ColumnMeta> cols;
    std::string pragma = "PRAGMA table_info(\"" + table_name + "\")";
    (void)sqlite_exec(db, pragma.c_str(), [&](int n, char** v, char**) -> bool {
        // cid | name | type | notnull | dflt_value | pk
        if (n >= 2 && v[1]) {
            cols.push_back({v[1], static_cast<int>(cols.size())});
        }
        return true;
    });
    // Detect columns by name (case-insensitive)
    for (const auto& c : cols) {
        const std::string lower = to_lower(c.name);
        if (lower == "key")  { s.has_key = true;  s.key_col = c.idx; }
        if (lower == "type") { s.has_type = true;  s.type_col = c.idx; }
        if (lower == "value"){ s.has_val  = true;  s.val_col  = c.idx; }
        if (lower == "mutable"){ s.has_mut = true; s.mut_col = c.idx; }
        if (lower == "defaultvalue"){ s.has_def = true; s.def_col = c.idx; }
    }
    return s;
}

std::expected<void, Error> parse_rbk34(sqlite3* db, proto::RobotParams& params) {
    // Enumerate tables
    std::vector<std::string> table_names;
    auto r = sqlite_exec(db,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name",
        [&](int n, char** v, char**) -> bool {
            if (n >= 1 && v[0]) table_names.emplace_back(v[0]);
            return true;
        });
    if (!r) return std::unexpected{std::move(r.error())};

    if (table_names.empty()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 SQLite has no user tables"}};
    }

    for (const auto& tbl : table_names) {
        auto* mod = params.add_modules();
        mod->set_name(tbl);
        mod->set_source_format("sqlite");

        auto schema = detect_rbk34_schema(db, tbl);
        if (!schema.has_key || !schema.has_val) {
            SMAP1_LOG_DEBUG("codec.load_params rbk34: table '{}' has no Key/Value columns, skipping", tbl);
            continue;
        }

        // SELECT all rows; ORDER BY the Key column (1-based position) so output
        // is deterministic.  Key column position = key_col + 1.
        std::string sql = "SELECT * FROM \"" + tbl + "\" ORDER BY "
            + std::to_string(schema.key_col + 1);

        // 逐行读出参数.  SELECT 中途失败 (I/O 错误 / 库损坏) 必须向上传播,
        // 否则会静默返回不完整的 entries.
        auto rr = sqlite_exec(db, sql.c_str(), [&](int n, char** v, char**) -> bool {
            // Key 列为 NULL 时跳过整行 (rbk34 schema 中 Key 是 NOT NULL UNIQUE,
            // 实测不会发生; 防御性地避免产出 key 为空的脏条目).
            if (!schema.has_key || schema.key_col >= n || !v[schema.key_col]) {
                return true;
            }
            auto* entry = mod->add_entries();
            entry->set_key(v[schema.key_col]);
            if (schema.has_type && schema.type_col < n && v[schema.type_col])
                entry->set_type_code(v[schema.type_col]);
            if (schema.has_val && schema.val_col < n && v[schema.val_col])
                entry->set_value(v[schema.val_col]);
            if (schema.has_def && schema.def_col < n && v[schema.def_col])
                entry->set_default_value(v[schema.def_col]);
            if (schema.has_mut && schema.mut_col < n && v[schema.mut_col])
                entry->set_mutable_flag(v[schema.mut_col]);
            return true;
        });
        if (!rr) return std::unexpected{std::move(rr).error()};
    }
    return {};
}

// ---- rbk35: resources/apps/ JSON files ----

// Recursively flatten a JSON parameter tree node into key-value pairs.
// The key is the dot-separated path from the root of this node.
void flatten_node(const nlohmann::json& node, const std::string& prefix,
                  proto::ParamModule* mod) {
    std::string key;
    if (auto k = node.find("key"); k != node.end() && k->is_string()) {
        key = k->get<std::string>();
    } else if (auto n = node.find("name"); n != node.end() && n->is_string()) {
        key = n->get<std::string>();
    } else {
        return;  // anonymous node, skip
    }

    std::string full_key = prefix.empty() ? key : prefix + "." + key;

    // Determine node type
    std::string type_str;
    if (auto t = node.find("type"); t != node.end() && t->is_string()) {
        type_str = t->get<std::string>();
    }

    // 节点类型决定如何展开:
    // - array: 纯容器, 递归全部 children/groups/params.
    // - comboBox / comboBoxBool: 单选容器, "value" 命名当前选中的子项 key,
    //   只有该分支生效 — 仅递归 key==value 的那一个 child.
    // - 叶子 (bool/int/float/double/string/bindType/json/stringComboList):
    //   直接携带标量 value (stringComboList 的 value 是从选项里选中的字符串).
    const bool has_value = node.contains("value") && !node["value"].is_null();
    const bool has_default = node.contains("defaultValue") && !node["defaultValue"].is_null();
    const bool is_selection = (type_str == "comboBox" || type_str == "comboBoxBool");
    const bool is_container = (type_str == "array" || is_selection);

    auto json_to_text = [](const nlohmann::json& v) -> std::string {
        if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
        if (v.is_string())  return v.get<std::string>();
        return v.dump();  // numbers (preserve precision), arrays, objects
    };

    // Emit leaf value.  rbk35 instance files (default.s*) carry "value"; the
    // script template (task.json) carries only "defaultValue".  Emit either.
    if ((has_value || has_default) && !is_container) {
        auto* entry = mod->add_entries();
        entry->set_key(full_key);
        entry->set_type_code(type_str);
        if (has_value) {
            entry->set_value(json_to_text(node["value"]));
        }
        if (has_default) {
            entry->set_default_value(json_to_text(node["defaultValue"]));
        }
        return;
    }

    // 单选容器: 先记录选中分支名, 再仅展开选中分支.
    std::string selected_key;
    if (is_selection) {
        auto* entry = mod->add_entries();
        entry->set_key(full_key + ".__selected__");
        entry->set_type_code(type_str);
        if (has_value) {
            selected_key = json_to_text(node["value"]);
            entry->set_value(selected_key);
        }
        if (has_default) {
            entry->set_default_value(json_to_text(node["defaultValue"]));
        }
    }

    // 子项可能在 children 或 params 中.  单选容器只展开 key 匹配选中值的那一支.
    auto recurse_children = [&](const nlohmann::json& arr) {
        for (const auto& child : arr) {
            if (is_selection) {
                auto ck = child.find("key");
                if (ck == child.end() || !ck->is_string() ||
                    ck->get<std::string>() != selected_key) {
                    continue;
                }
            }
            flatten_node(child, full_key, mod);
        }
    };
    if (auto ch = node.find("children"); ch != node.end() && ch->is_array()) {
        recurse_children(*ch);
    }
    if (auto p = node.find("params"); p != node.end() && p->is_array()) {
        recurse_children(*p);
    }
    // cloneChildren: same structure as children, but prefixed to show instance index
    if (auto cc = node.find("cloneChildren"); cc != node.end() && cc->is_array()) {
        int idx = 0;
        for (const auto& child : *cc) {
            std::string key_with_idx = full_key + "[" + std::to_string(idx) + "]";
            flatten_node(child, key_with_idx, mod);
            ++idx;
        }
    }
    // groups: at the top level of each JSON file, groups is the container
    if (auto g = node.find("groups"); g != node.end() && g->is_array()) {
        for (const auto& grp : *g) {
            flatten_node(grp, full_key, mod);
        }
    }
}

std::expected<void, Error> parse_rbk35(const std::filesystem::path& apps_dir,
                                       proto::RobotParams& params) {
    if (!std::filesystem::is_directory(apps_dir)) {
        return std::unexpected{Error{Error::Code::FileNotFound,
            "rbk35 path is not a directory: " + apps_dir.string()}};
    }

    // Collect JSON files, sorted by subdirectory name for determinism
    std::vector<std::filesystem::path> json_files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(apps_dir, ec)) {
        if (!entry.is_regular_file() && !entry.is_directory()) continue;
        auto p = entry.path();
        // Accept known extensions
        std::string ext = p.extension().string();
        if (ext == ".json" || ext == ".sctrl" || ext == ".sfs" || ext == ".sloc" ||
            ext == ".snav" || ext == ".spow" || ext == ".srec") {
            json_files.push_back(p);
        }
        // Also recurse into subdirectories for .json files
        if (entry.is_directory()) {
            std::error_code ec2;
            for (const auto& sub : std::filesystem::directory_iterator(p, ec2)) {
                auto sp = sub.path();
                std::string sext = sp.extension().string();
                if (sext == ".json" || sext == ".sctrl" || sext == ".sfs" ||
                    sext == ".sloc" || sext == ".snav" || sext == ".spow" ||
                    sext == ".srec") {
                    json_files.push_back(sp);
                }
            }
        }
    }

    if (ec) {
        return std::unexpected{Error{Error::Code::IoError,
            "directory iteration failed: " + apps_dir.string() + ": " + ec.message()}};
    }

    // Sort for deterministic output
    std::sort(json_files.begin(), json_files.end());

    // Group by subdirectory (module name)
    // key = relative directory name (e.g. "localization"), list of files
    std::map<std::string, std::vector<std::filesystem::path>, std::less<>> files_by_module;
    for (const auto& fp : json_files) {
        auto parent = fp.parent_path();
        std::string mod_name;
        if (parent == apps_dir) {
            // File directly in apps/ root (e.g. task.json)
            mod_name = fp.stem().string();
        } else {
            // In a subdirectory (e.g. localization/default.sloc)
            mod_name = parent.filename().string();
        }
        files_by_module[mod_name].push_back(fp);
    }

    bool any_parsed = false;
    for (auto& [mod_name, fps] : files_by_module) {
        auto* mod = params.add_modules();
        mod->set_name(mod_name);
        mod->set_source_format("apps");

        for (const auto& fp : fps) {
            auto bytes = read_file(fp);
            if (!bytes) {
                SMAP1_LOG_WARN("codec.load_params rbk35: cannot read '{}': {}",
                               fp.string(), bytes.error().message);
                continue;
            }
            auto doc = parse_json(*bytes, fp.filename().string());
            if (!doc) {
                SMAP1_LOG_WARN("codec.load_params rbk35: JSON parse failed for '{}': {}",
                               fp.string(), doc.error().message);
                continue;
            }
            any_parsed = true;

            // Top-level may be {"desc": ..., "groups": [...]} or directly groups array
            if (doc->is_object()) {
                if (auto g = doc->find("groups"); g != doc->end() && g->is_array()) {
                    for (const auto& grp : *g) {
                        flatten_node(grp, "", mod);
                    }
                } else if (auto ch = doc->find("children"); ch != doc->end() && ch->is_array()) {
                    for (const auto& child : *ch) {
                        flatten_node(child, "", mod);
                    }
                } else {
                    // Treat as a flat object of params directly
                    // If it has "key" at top level, it's a single node
                    if (doc->contains("key") || doc->contains("name")) {
                        flatten_node(*doc, "", mod);
                    }
                }
            }
        }
    }

    if (!any_parsed) {
        return std::unexpected{Error{Error::Code::ParseFailed,
            "rbk35 directory contains no parseable JSON files: " + apps_dir.string()}};
    }
    return {};
}

}  // namespace

std::expected<proto::RobotParams, Error> load_params(std::filesystem::path path, ParamFormat format) {
    SMAP1_LOG_INFO("codec.load_params path={} format={}",
        path.string(),
        format == ParamFormat::Rbk34 ? "rbk34" : "rbk35");

    proto::RobotParams params;
    params.set_source_path(path.generic_string());

    if (format == ParamFormat::Rbk34) {
        // rbk34: path is the SQLite .param file
        auto db_or = open_sqlite(path);
        if (!db_or) {
            SMAP1_LOG_ERROR("codec.load_params failed: {}", db_or.error().message);
            return std::unexpected{std::move(db_or).error()};
        }
        auto parsed = parse_rbk34(db_or->get(), params);
        if (!parsed) {
            SMAP1_LOG_ERROR("codec.load_params failed: {}", parsed.error().message);
            return std::unexpected{std::move(parsed).error()};
        }
    } else {
        // rbk35: path is the resources/apps/ directory
        auto parsed = parse_rbk35(path, params);
        if (!parsed) {
            SMAP1_LOG_ERROR("codec.load_params failed: {}", parsed.error().message);
            return std::unexpected{std::move(parsed).error()};
        }
    }

    // Log summary
    std::size_t total_entries = 0;
    for (const auto& m : params.modules()) {
        total_entries += static_cast<std::size_t>(m.entries_size());
    }
    SMAP1_LOG_DEBUG("codec.load_params ok: modules={} entries={}",
        params.modules_size(), total_entries);
    return params;
}

}  // namespace smap1::codec
