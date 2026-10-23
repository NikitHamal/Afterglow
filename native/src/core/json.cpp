#include "core/json.h"
#include <cstdlib>
#include <cstdio>

namespace ag {

namespace {
struct Parser {
    const std::string& t;
    size_t i = 0;
    std::string err;
    explicit Parser(const std::string& text) : t(text) {}

    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) ++i; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at " + std::to_string(i); return false; }

    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool hex4(unsigned& v) {
        if (i + 4 > t.size()) return fail("bad \\u escape");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = t[i++]; v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return fail("bad hex digit");
        }
        return true;
    }
    bool str(std::string& out) {
        if (t[i] != '"') return fail("expected string");
        ++i;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) break;
            char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                        i += 2; unsigned lo;
                        if (!hex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool value(Json& v) {
        ws();
        if (i >= t.size()) return fail("unexpected end");
        char c = t[i];
        if (c == '{') {
            v.type = Json::Object; ++i; ws();
            if (i < t.size() && t[i] == '}') { ++i; return true; }
            while (true) {
                ws(); std::string k;
                if (!str(k)) return false;
                ws();
                if (i >= t.size() || t[i] != ':') return fail("expected ':'");
                ++i;
                Json child;
                if (!value(child)) return false;
                v.obj.emplace_back(std::move(k), std::move(child));
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == '}') { ++i; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Json::Array; ++i; ws();
            if (i < t.size() && t[i] == ']') { ++i; return true; }
            while (true) {
                Json child;
                if (!value(child)) return false;
                v.arr.push_back(std::move(child));
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Json::String; return str(v.s); }
        if (t.compare(i, 4, "true") == 0) { v.type = Json::Bool; v.b = true; i += 4; return true; }
        if (t.compare(i, 5, "false") == 0) { v.type = Json::Bool; v.b = false; i += 5; return true; }
        if (t.compare(i, 4, "null") == 0) { v.type = Json::Null; i += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            const char* start = t.c_str() + i; char* end = nullptr;
            v.type = Json::Number; v.n = std::strtod(start, &end);
            if (end == start) return fail("bad number");
            i += (size_t)(end - start);
            return true;
        }
        return fail("unexpected character");
    }
};

void dumpStr(std::string& o, const std::string& s) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    o += '"';
}
void dumpVal(std::string& o, const Json& j, int depth) {
    std::string ind((size_t)depth * 2, ' ');
    switch (j.type) {
        case Json::Null: o += "null"; break;
        case Json::Bool: o += j.b ? "true" : "false"; break;
        case Json::Number: { char b[32]; std::snprintf(b, sizeof b, "%.6g", j.n); o += b; break; }
        case Json::String: dumpStr(o, j.s); break;
        case Json::Array:
            o += "[";
            for (size_t k = 0; k < j.arr.size(); ++k) { if (k) o += ", "; dumpVal(o, j.arr[k], depth + 1); }
            o += "]"; break;
        case Json::Object:
            o += "{\n";
            for (size_t k = 0; k < j.obj.size(); ++k) {
                o += ind + "  "; dumpStr(o, j.obj[k].first); o += ": "; dumpVal(o, j.obj[k].second, depth + 1);
                if (k + 1 < j.obj.size()) o += ",";
                o += "\n";
            }
            o += ind + "}"; break;
    }
}
}  // namespace

const Json& Json::nullValue() { static Json n; return n; }

Json Json::parse(const std::string& text, std::string* err) {
    Parser p(text);
    Json v;
    if (!p.value(v)) { if (err) *err = p.err; return Json(); }
    return v;
}

std::string Json::dump() const { std::string o; dumpVal(o, *this, 0); return o; }

bool Json::has(const std::string& key) const {
    if (type != Object) return false;
    for (auto& kv : obj) if (kv.first == key) return true;
    return false;
}
const Json& Json::operator[](const std::string& key) const {
    if (type == Object) for (auto& kv : obj) if (kv.first == key) return kv.second;
    return nullValue();
}
const Json& Json::operator[](size_t idx) const {
    if (type == Array && idx < arr.size()) return arr[idx];
    return nullValue();
}
Json& Json::set(const std::string& key, const Json& v) {
    type = Object;
    for (auto& kv : obj) if (kv.first == key) { kv.second = v; return kv.second; }
    obj.emplace_back(key, v);
    return obj.back().second;
}

}  // namespace ag
