// Afterglow engine — scene spec strings used by the screenshot harness and the sandbox:
//   "pos=missionary;depth=0.6;face=blink:1,moan:0.4;body=bust:0.6;cam=orbit:0.6,0.25,2.4"
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdlib>

namespace ag {

struct Spec {
    std::vector<std::pair<std::string, std::string>> kv;

    static Spec parse(const std::string& s) {
        Spec sp;
        size_t i = 0;
        while (i <= s.size()) {
            size_t j = s.find(';', i);
            if (j == std::string::npos) j = s.size();
            std::string item = trim(s.substr(i, j - i));
            if (!item.empty()) {
                size_t eq = item.find('=');
                if (eq == std::string::npos) sp.kv.emplace_back(item, "1");
                else sp.kv.emplace_back(trim(item.substr(0, eq)), trim(item.substr(eq + 1)));
            }
            i = j + 1;
        }
        return sp;
    }
    bool has(const std::string& k) const { for (auto& p : kv) if (p.first == k) return true; return false; }
    std::string get(const std::string& k, const std::string& def = "") const {
        for (auto& p : kv) if (p.first == k) return p.second;
        return def;
    }
    float num(const std::string& k, float def) const {
        std::string v = get(k);
        return v.empty() ? def : (float)std::atof(v.c_str());
    }
    // "a:1,b:0.5" -> {{a,1},{b,0.5}}
    static std::vector<std::pair<std::string, float>> pairs(const std::string& v) {
        std::vector<std::pair<std::string, float>> out;
        for (auto& item : split(v, ',')) {
            size_t c = item.find(':');
            if (c == std::string::npos) out.emplace_back(trim(item), 1.f);
            else out.emplace_back(trim(item.substr(0, c)), (float)std::atof(item.substr(c + 1).c_str()));
        }
        return out;
    }
    static std::vector<std::string> split(const std::string& v, char sep) {
        std::vector<std::string> out; size_t i = 0;
        while (i <= v.size()) {
            size_t j = v.find(sep, i);
            if (j == std::string::npos) j = v.size();
            std::string t = trim(v.substr(i, j - i));
            if (!t.empty()) out.push_back(t);
            i = j + 1;
        }
        return out;
    }
    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    }
};

}  // namespace ag
