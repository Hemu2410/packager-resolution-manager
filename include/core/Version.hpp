#ifndef VERSION_HPP
#define VERSION_HPP

#include <string>
#include <sstream>
#include <vector>
#include <tuple>
#include <iostream>
#include <algorithm>
#include <cctype>

class Version {
public:
    int major;
    int minor;
    int patch;
    std::string prerelease;

    Version() : major(0), minor(0), patch(0), prerelease("") {}
    Version(int maj, int min = 0, int pat = 0, const std::string& pre = "")
        : major(maj), minor(min), patch(pat), prerelease(pre) {}

    static Version parse(std::string str) {
        // Trim leading spaces or 'v'
        size_t start = 0;
        while (start < str.size() && (std::isspace(str[start]) || str[start] == 'v' || str[start] == 'V')) {
            start++;
        }
        str = str.substr(start);

        // Check for prerelease suffix (e.g. -alpha, -rc.1)
        std::string pre = "";
        size_t dash = str.find('-');
        if (dash != std::string::npos) {
            pre = str.substr(dash + 1);
            str = str.substr(0, dash);
        }

        int maj = 0, min = 0, pat = 0;
        std::stringstream ss(str);
        std::string part;
        if (std::getline(ss, part, '.')) {
            try { maj = std::stoi(part); } catch (...) { maj = 0; }
        }
        if (std::getline(ss, part, '.')) {
            try { min = std::stoi(part); } catch (...) { min = 0; }
        }
        if (std::getline(ss, part, '.')) {
            try { pat = std::stoi(part); } catch (...) { pat = 0; }
        }

        return Version(maj, min, pat, pre);
    }

    std::string toString() const {
        std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        if (!prerelease.empty()) {
            s += "-" + prerelease;
        }
        return s;
    }

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor && patch == other.patch && prerelease == other.prerelease;
    }

    bool operator!=(const Version& other) const {
        return !(*this == other);
    }

    bool operator<(const Version& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        if (patch != other.patch) return patch < other.patch;
        // Semver rule: a version with prerelease has lower precedence than normal version
        if (prerelease.empty() && !other.prerelease.empty()) return false;
        if (!prerelease.empty() && other.prerelease.empty()) return true;
        return prerelease < other.prerelease;
    }

    bool operator<=(const Version& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const Version& other) const {
        return !(*this <= other);
    }

    bool operator>=(const Version& other) const {
        return !(*this < other);
    }

    friend std::ostream& operator<<(std::ostream& os, const Version& v) {
        os << v.toString();
        return os;
    }
};

namespace std {
    template <>
    struct hash<Version> {
        size_t operator()(const Version& v) const {
            return ((hash<int>()(v.major) ^ (hash<int>()(v.minor) << 1)) >> 1) ^ (hash<int>()(v.patch) << 1);
        }
    };
}

#endif // VERSION_HPP
