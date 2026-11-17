#ifndef VERSION_CONSTRAINT_HPP
#define VERSION_CONSTRAINT_HPP

#include "Version.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>

enum class ConstraintOp {
    ANY,
    EQ,
    NEQ,
    LT,
    LTE,
    GT,
    GTE,
    CARET, // ^1.2.3
    TILDE  // ~1.2.3
};

struct SingleConstraint {
    ConstraintOp op;
    Version version;

    SingleConstraint(ConstraintOp o = ConstraintOp::ANY, Version v = Version())
        : op(o), version(v) {}

    bool matches(const Version& target) const {
        switch (op) {
            case ConstraintOp::ANY:
                return true;
            case ConstraintOp::EQ:
                return target == version;
            case ConstraintOp::NEQ:
                return target != version;
            case ConstraintOp::LT:
                return target < version;
            case ConstraintOp::LTE:
                return target <= version;
            case ConstraintOp::GT:
                return target > version;
            case ConstraintOp::GTE:
                return target >= version;
            case ConstraintOp::CARET: {
                if (target < version) return false;
                if (version.major > 0) {
                    return target.major == version.major;
                } else if (version.minor > 0) {
                    return target.major == 0 && target.minor == version.minor;
                } else {
                    return target == version;
                }
            }
            case ConstraintOp::TILDE: {
                if (target < version) return false;
                return target.major == version.major && target.minor == version.minor;
            }
        }
        return false;
    }

    std::string toString() const {
        switch (op) {
            case ConstraintOp::ANY: return "*";
            case ConstraintOp::EQ: return "==" + version.toString();
            case ConstraintOp::NEQ: return "!=" + version.toString();
            case ConstraintOp::LT: return "<" + version.toString();
            case ConstraintOp::LTE: return "<=" + version.toString();
            case ConstraintOp::GT: return ">" + version.toString();
            case ConstraintOp::GTE: return ">=" + version.toString();
            case ConstraintOp::CARET: return "^" + version.toString();
            case ConstraintOp::TILDE: return "~" + version.toString();
        }
        return "*";
    }
};

class VersionConstraint {
private:
    std::vector<SingleConstraint> constraints;
    std::string rawString;

public:
    VersionConstraint() : rawString("*") {
        constraints.push_back(SingleConstraint(ConstraintOp::ANY));
    }

    explicit VersionConstraint(const std::string& str) : rawString(str) {
        parse(str);
    }

    static VersionConstraint any() {
        return VersionConstraint("*");
    }

    static VersionConstraint exact(const Version& v) {
        VersionConstraint vc;
        vc.constraints.clear();
        vc.constraints.push_back(SingleConstraint(ConstraintOp::EQ, v));
        vc.rawString = "==" + v.toString();
        return vc;
    }

    void parse(const std::string& input) {
        constraints.clear();
        rawString = input;

        // Split by comma or space
        std::string cleaned = input;
        for (char& c : cleaned) {
            if (c == ',') c = ' ';
        }

        std::stringstream ss(cleaned);
        std::string token;
        while (ss >> token) {
            if (token == "*" || token == "latest" || token == "any") {
                constraints.push_back(SingleConstraint(ConstraintOp::ANY));
                continue;
            }

            ConstraintOp op = ConstraintOp::EQ;
            size_t vstart = 0;

            if (token.size() >= 2 && token.substr(0, 2) == "<=") {
                op = ConstraintOp::LTE;
                vstart = 2;
            } else if (token.size() >= 2 && token.substr(0, 2) == ">=") {
                op = ConstraintOp::GTE;
                vstart = 2;
            } else if (token.size() >= 2 && token.substr(0, 2) == "==") {
                op = ConstraintOp::EQ;
                vstart = 2;
            } else if (token.size() >= 2 && token.substr(0, 2) == "!=") {
                op = ConstraintOp::NEQ;
                vstart = 2;
            } else if (token[0] == '<') {
                op = ConstraintOp::LT;
                vstart = 1;
            } else if (token[0] == '>') {
                op = ConstraintOp::GT;
                vstart = 1;
            } else if (token[0] == '=') {
                op = ConstraintOp::EQ;
                vstart = 1;
            } else if (token[0] == '^') {
                op = ConstraintOp::CARET;
                vstart = 1;
            } else if (token[0] == '~') {
                op = ConstraintOp::TILDE;
                vstart = 1;
            } else {
                op = ConstraintOp::EQ;
                vstart = 0;
            }

            Version v = Version::parse(token.substr(vstart));
            constraints.push_back(SingleConstraint(op, v));
        }

        if (constraints.empty()) {
            constraints.push_back(SingleConstraint(ConstraintOp::ANY));
        }
    }

    bool matches(const Version& v) const {
        for (const auto& c : constraints) {
            if (!c.matches(v)) {
                return false;
            }
        }
        return true;
    }

    std::string toString() const {
        if (!rawString.empty()) return rawString;
        std::string res;
        for (size_t i = 0; i < constraints.size(); i++) {
            if (i > 0) res += " ";
            res += constraints[i].toString();
        }
        return res;
    }

    friend std::ostream& operator<<(std::ostream& os, const VersionConstraint& vc) {
        os << vc.toString();
        return os;
    }
};

#endif // VERSION_CONSTRAINT_HPP
