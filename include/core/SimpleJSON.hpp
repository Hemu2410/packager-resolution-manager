#ifndef SIMPLE_JSON_HPP
#define SIMPLE_JSON_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iostream>

class JSON {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type;
    bool boolValue;
    double numberValue;
    std::string stringValue;
    std::vector<JSON> arrayValue;
    std::map<std::string, JSON> objectValue;

    JSON() : type(Type::Null), boolValue(false), numberValue(0.0) {}
    JSON(std::nullptr_t) : type(Type::Null), boolValue(false), numberValue(0.0) {}
    JSON(bool b) : type(Type::Bool), boolValue(b), numberValue(0.0) {}
    JSON(int n) : type(Type::Number), boolValue(false), numberValue(n) {}
    JSON(double n) : type(Type::Number), boolValue(false), numberValue(n) {}
    JSON(const char* s) : type(Type::String), boolValue(false), numberValue(0.0), stringValue(s) {}
    JSON(const std::string& s) : type(Type::String), boolValue(false), numberValue(0.0), stringValue(s) {}

    static JSON makeArray() {
        JSON j;
        j.type = Type::Array;
        return j;
    }

    static JSON makeObject() {
        JSON j;
        j.type = Type::Object;
        return j;
    }

    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    bool asBool() const { return boolValue; }
    int asInt() const { return static_cast<int>(numberValue); }
    double asDouble() const { return numberValue; }
    const std::string& asString() const { return stringValue; }
    const std::vector<JSON>& asArray() const { return arrayValue; }
    const std::map<std::string, JSON>& asObject() const { return objectValue; }

    void push_back(const JSON& val) {
        if (type != Type::Array) {
            type = Type::Array;
            arrayValue.clear();
        }
        arrayValue.push_back(val);
    }

    JSON& operator[](const std::string& key) {
        if (type != Type::Object) {
            type = Type::Object;
            objectValue.clear();
        }
        return objectValue[key];
    }

    const JSON& operator[](const std::string& key) const {
        static JSON nullObj;
        auto it = objectValue.find(key);
        if (it != objectValue.end()) return it->second;
        return nullObj;
    }

    bool contains(const std::string& key) const {
        if (type != Type::Object) return false;
        return objectValue.find(key) != objectValue.end();
    }

    std::string dump(int indent = 0) const {
        std::string ind(indent * 2, ' ');
        switch (type) {
            case Type::Null: return "null";
            case Type::Bool: return boolValue ? "true" : "false";
            case Type::Number: {
                if (numberValue == static_cast<int>(numberValue)) {
                    return std::to_string(static_cast<int>(numberValue));
                }
                return std::to_string(numberValue);
            }
            case Type::String: {
                std::string s = "\"";
                for (char c : stringValue) {
                    if (c == '"') s += "\\\"";
                    else if (c == '\\') s += "\\\\";
                    else if (c == '\n') s += "\\n";
                    else if (c == '\r') s += "\\r";
                    else if (c == '\t') s += "\\t";
                    else s += c;
                }
                s += "\"";
                return s;
            }
            case Type::Array: {
                if (arrayValue.empty()) return "[]";
                std::string res = "[\n";
                for (size_t i = 0; i < arrayValue.size(); i++) {
                    res += std::string((indent + 1) * 2, ' ') + arrayValue[i].dump(indent + 1);
                    if (i + 1 < arrayValue.size()) res += ",";
                    res += "\n";
                }
                res += ind + "]";
                return res;
            }
            case Type::Object: {
                if (objectValue.empty()) return "{}";
                std::string res = "{\n";
                size_t i = 0;
                for (const auto& kv : objectValue) {
                    res += std::string((indent + 1) * 2, ' ') + "\"" + kv.first + "\": " + kv.second.dump(indent + 1);
                    if (++i < objectValue.size()) res += ",";
                    res += "\n";
                }
                res += ind + "}";
                return res;
            }
        }
        return "null";
    }

    static JSON parse(const std::string& src) {
        size_t idx = 0;
        skipWhitespace(src, idx);
        return parseValue(src, idx);
    }

private:
    static void skipWhitespace(const std::string& s, size_t& idx) {
        while (idx < s.size() && (std::isspace(s[idx]) || s[idx] == '\r' || s[idx] == '\n' || s[idx] == '\t')) {
            idx++;
        }
    }

    static JSON parseValue(const std::string& s, size_t& idx) {
        skipWhitespace(s, idx);
        if (idx >= s.size()) return JSON();

        char c = s[idx];
        if (c == '{') return parseObject(s, idx);
        if (c == '[') return parseArray(s, idx);
        if (c == '"') return parseString(s, idx);
        if (c == 't' || c == 'f') return parseBool(s, idx);
        if (c == 'n') return parseNull(s, idx);
        if (c == '-' || std::isdigit(c)) return parseNumber(s, idx);

        throw std::runtime_error(std::string("Unexpected character in JSON: ") + c);
    }

    static JSON parseObject(const std::string& s, size_t& idx) {
        idx++; // skip '{'
        JSON obj = JSON::makeObject();
        while (true) {
            skipWhitespace(s, idx);
            if (idx >= s.size()) throw std::runtime_error("Unterminated object");
            if (s[idx] == '}') {
                idx++;
                break;
            }
            if (s[idx] != '"') throw std::runtime_error("Expected string key in object");
            std::string key = parseString(s, idx).asString();
            skipWhitespace(s, idx);
            if (idx >= s.size() || s[idx] != ':') throw std::runtime_error("Expected ':' after key");
            idx++; // skip ':'
            JSON val = parseValue(s, idx);
            obj[key] = val;
            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                idx++;
            } else if (idx < s.size() && s[idx] == '}') {
                idx++;
                break;
            } else {
                throw std::runtime_error("Expected ',' or '}' in object");
            }
        }
        return obj;
    }

    static JSON parseArray(const std::string& s, size_t& idx) {
        idx++; // skip '['
        JSON arr = JSON::makeArray();
        while (true) {
            skipWhitespace(s, idx);
            if (idx >= s.size()) throw std::runtime_error("Unterminated array");
            if (s[idx] == ']') {
                idx++;
                break;
            }
            JSON val = parseValue(s, idx);
            arr.push_back(val);
            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                idx++;
            } else if (idx < s.size() && s[idx] == ']') {
                idx++;
                break;
            } else {
                throw std::runtime_error("Expected ',' or ']' in array");
            }
        }
        return arr;
    }

    static JSON parseString(const std::string& s, size_t& idx) {
        idx++; // skip opening '"'
        std::string res;
        while (idx < s.size()) {
            char c = s[idx++];
            if (c == '"') return JSON(res);
            if (c == '\\') {
                if (idx >= s.size()) throw std::runtime_error("Unterminated escape in string");
                char esc = s[idx++];
                if (esc == '"') res += '"';
                else if (esc == '\\') res += '\\';
                else if (esc == '/') res += '/';
                else if (esc == 'b') res += '\b';
                else if (esc == 'f') res += '\f';
                else if (esc == 'n') res += '\n';
                else if (esc == 'r') res += '\r';
                else if (esc == 't') res += '\t';
                else res += esc;
            } else {
                res += c;
            }
        }
        throw std::runtime_error("Unterminated string");
    }

    static JSON parseNumber(const std::string& s, size_t& idx) {
        size_t start = idx;
        if (s[idx] == '-') idx++;
        while (idx < s.size() && std::isdigit(s[idx])) idx++;
        if (idx < s.size() && s[idx] == '.') {
            idx++;
            while (idx < s.size() && std::isdigit(s[idx])) idx++;
        }
        double val = std::stod(s.substr(start, idx - start));
        return JSON(val);
    }

    static JSON parseBool(const std::string& s, size_t& idx) {
        if (s.substr(idx, 4) == "true") {
            idx += 4;
            return JSON(true);
        }
        if (s.substr(idx, 5) == "false") {
            idx += 5;
            return JSON(false);
        }
        throw std::runtime_error("Invalid boolean");
    }

    static JSON parseNull(const std::string& s, size_t& idx) {
        if (s.substr(idx, 4) == "null") {
            idx += 4;
            return JSON(nullptr);
        }
        throw std::runtime_error("Invalid null");
    }
};

#endif // SIMPLE_JSON_HPP
