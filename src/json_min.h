#pragma once
// Minimal read-only JSON parser used for sources.json. Objects, arrays, strings
// (with escapes), numbers, true/false/null. No streaming, no comments, no
// duplicate-key detection: a later duplicate simply overwrites the earlier one.
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdlib>
#include <cstdio>

struct JValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool flag = false;
    double number = 0;
    std::string text;
    std::vector<JValue> items;
    std::map<std::string, JValue> fields;
    const JValue* get(const std::string& key) const {
        auto it = fields.find(key);
        return it == fields.end() ? 0 : &it->second;
    }
    std::string str(const std::string& key, const std::string& fallback = "") const {
        const JValue* v = get(key);
        return v && v->type == String ? v->text : fallback;
    }
    bool boolean(const std::string& key, bool fallback = false) const {
        const JValue* v = get(key);
        return v && v->type == Bool ? v->flag : fallback;
    }
    int integer(const std::string& key, int fallback = 0) const {
        const JValue* v = get(key);
        if (!v) return fallback;
        if (v->type == Number) return (int)v->number;
        if (v->type == String && !v->text.empty()) return atoi(v->text.c_str());
        return fallback;
    }
    const JValue* list(const std::string& key) const {
        const JValue* v = get(key);
        return v && v->type == Array ? v : 0;
    }
};

inline void jsonSkipSpace(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) ++p;
}

inline void jsonAppendUtf8(std::string& out, unsigned code) {
    if (code < 0x80) out += char(code);
    else if (code < 0x800) { out += char(0xC0 | (code >> 6)); out += char(0x80 | (code & 63)); }
    else if (code < 0x10000) { out += char(0xE0 | (code >> 12)); out += char(0x80 | ((code >> 6) & 63)); out += char(0x80 | (code & 63)); }
    else { out += char(0xF0 | (code >> 18)); out += char(0x80 | ((code >> 12) & 63)); out += char(0x80 | ((code >> 6) & 63)); out += char(0x80 | (code & 63)); }
}

inline bool jsonHex4(const std::string& s, size_t p, unsigned& value) {
    if (p + 4 > s.size()) return false;
    value = 0;
    for (size_t i = 0; i < 4; ++i) {
        char c = s[p + i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = unsigned(c - '0');
        else if (c >= 'a' && c <= 'f') digit = unsigned(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = unsigned(c - 'A' + 10);
        else return false;
        value = value * 16 + digit;
    }
    return true;
}

inline bool jsonParseString(const std::string& s, size_t& p, std::string& out) {
    out.clear();
    if (p >= s.size() || s[p] != '"') return false;
    ++p;
    while (p < s.size()) {
        char c = s[p++];
        if (c == '"') return true;
        if (c != '\\') { out += c; continue; }
        if (p >= s.size()) return false;
        char escape = s[p++];
        switch (escape) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned code = 0;
                if (!jsonHex4(s, p, code)) return false;
                p += 4;
                if (code >= 0xD800 && code < 0xDC00 && p + 6 <= s.size() && s[p] == '\\' && s[p + 1] == 'u') {
                    unsigned low = 0;
                    if (jsonHex4(s, p + 2, low) && low >= 0xDC00 && low < 0xE000) {
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        p += 6;
                    }
                }
                jsonAppendUtf8(out, code);
                break;
            }
            default: return false;
        }
    }
    return false;
}

inline bool jsonParseValue(const std::string& s, size_t& p, JValue& out, int depth) {
    if (depth > 32) return false;
    jsonSkipSpace(s, p);
    if (p >= s.size()) return false;
    char c = s[p];
    if (c == '{') {
        ++p;
        out.type = JValue::Object;
        jsonSkipSpace(s, p);
        if (p < s.size() && s[p] == '}') { ++p; return true; }
        for (;;) {
            jsonSkipSpace(s, p);
            std::string key;
            if (!jsonParseString(s, p, key)) return false;
            jsonSkipSpace(s, p);
            if (p >= s.size() || s[p] != ':') return false;
            ++p;
            JValue value;
            if (!jsonParseValue(s, p, value, depth + 1)) return false;
            out.fields[key] = value;
            jsonSkipSpace(s, p);
            if (p < s.size() && s[p] == ',') { ++p; continue; }
            if (p < s.size() && s[p] == '}') { ++p; return true; }
            return false;
        }
    }
    if (c == '[') {
        ++p;
        out.type = JValue::Array;
        jsonSkipSpace(s, p);
        if (p < s.size() && s[p] == ']') { ++p; return true; }
        for (;;) {
            JValue value;
            if (!jsonParseValue(s, p, value, depth + 1)) return false;
            out.items.push_back(value);
            jsonSkipSpace(s, p);
            if (p < s.size() && s[p] == ',') { ++p; continue; }
            if (p < s.size() && s[p] == ']') { ++p; return true; }
            return false;
        }
    }
    if (c == '"') { out.type = JValue::String; return jsonParseString(s, p, out.text); }
    if (s.compare(p, 4, "true") == 0) { out.type = JValue::Bool; out.flag = true; p += 4; return true; }
    if (s.compare(p, 5, "false") == 0) { out.type = JValue::Bool; out.flag = false; p += 5; return true; }
    if (s.compare(p, 4, "null") == 0) { out.type = JValue::Null; p += 4; return true; }
    size_t start = p;
    if (p < s.size() && (s[p] == '-' || s[p] == '+')) ++p;
    bool digits = false;
    while (p < s.size() && isdigit((unsigned char)s[p])) { ++p; digits = true; }
    if (p < s.size() && s[p] == '.') { ++p; while (p < s.size() && isdigit((unsigned char)s[p])) { ++p; digits = true; } }
    if (digits && p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
        ++p;
        if (p < s.size() && (s[p] == '-' || s[p] == '+')) ++p;
        while (p < s.size() && isdigit((unsigned char)s[p])) ++p;
    }
    if (!digits) return false;
    out.type = JValue::Number;
    out.number = strtod(s.substr(start, p - start).c_str(), 0);
    return true;
}

inline bool jsonParse(const std::string& s, JValue& out) {
    out = JValue();
    size_t p = 0;
    if (!jsonParseValue(s, p, out, 0)) return false;
    jsonSkipSpace(s, p);
    return p == s.size();
}

inline std::string jsonEscape(const std::string& s) {
    std::string out = "\"";
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04X", c); out += b; }
        else out += char(c);
    }
    out += "\"";
    return out;
}
