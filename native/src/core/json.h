// Afterglow engine — compact JSON value + parser (glTF extras, clips.json, settings).
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace ag {

class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    static Json parse(const std::string& text, std::string* err = nullptr);
    std::string dump() const;

    bool isNull() const { return type == Null; }
    bool isObject() const { return type == Object; }
    bool isArray() const { return type == Array; }
    bool isString() const { return type == String; }
    bool isNumber() const { return type == Number; }
    bool has(const std::string& key) const;
    const Json& operator[](const std::string& key) const;
    const Json& operator[](size_t i) const;
    size_t size() const { return type == Array ? arr.size() : (type == Object ? obj.size() : 0); }
    double num(double def = 0) const { return type == Number ? n : (type == Bool ? (b ? 1 : 0) : def); }
    float f(float def = 0) const { return (float)num(def); }
    std::string str(const std::string& def = "") const { return type == String ? s : def; }
    bool boolean(bool def = false) const { return type == Bool ? b : (type == Number ? n != 0 : def); }

    // builders (settings files)
    static Json object() { Json j; j.type = Object; return j; }
    static Json array() { Json j; j.type = Array; return j; }
    static Json of(double v) { Json j; j.type = Number; j.n = v; return j; }
    static Json of(const std::string& v) { Json j; j.type = String; j.s = v; return j; }
    static Json ofBool(bool v) { Json j; j.type = Bool; j.b = v; return j; }
    Json& set(const std::string& key, const Json& v);
    Json& push(const Json& v) { type = Array; arr.push_back(v); return arr.back(); }

    static const Json& nullValue();
};

}  // namespace ag
