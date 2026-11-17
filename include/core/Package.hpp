#ifndef PACKAGE_HPP
#define PACKAGE_HPP

#include "Version.hpp"
#include "VersionConstraint.hpp"
#include <string>
#include <vector>
#include <iostream>

struct Dependency {
    std::string packageName;
    VersionConstraint constraint;

    Dependency() : packageName(""), constraint(VersionConstraint::any()) {}
    Dependency(const std::string& name, const VersionConstraint& vc)
        : packageName(name), constraint(vc) {}
    Dependency(const std::string& name, const std::string& constraintStr)
        : packageName(name), constraint(VersionConstraint(constraintStr)) {}

    std::string toString() const {
        return packageName + " (" + constraint.toString() + ")";
    }
};

struct Conflict {
    std::string packageName;
    VersionConstraint constraint;

    Conflict() : packageName(""), constraint(VersionConstraint::any()) {}
    Conflict(const std::string& name, const VersionConstraint& vc)
        : packageName(name), constraint(vc) {}
    Conflict(const std::string& name, const std::string& constraintStr)
        : packageName(name), constraint(VersionConstraint(constraintStr)) {}

    std::string toString() const {
        return packageName + " (" + constraint.toString() + ")";
    }
};

class Package {
public:
    std::string name;
    Version version;
    std::vector<Dependency> dependencies;
    std::vector<Conflict> conflicts;

    Package() : name(""), version(Version()) {}
    Package(const std::string& pkgName, const Version& ver)
        : name(pkgName), version(ver) {}
    Package(const std::string& pkgName, const std::string& verStr)
        : name(pkgName), version(Version::parse(verStr)) {}

    void addDependency(const std::string& depName, const std::string& constraintStr) {
        dependencies.push_back(Dependency(depName, VersionConstraint(constraintStr)));
    }

    void addDependency(const Dependency& dep) {
        dependencies.push_back(dep);
    }

    void addConflict(const std::string& confName, const std::string& constraintStr) {
        conflicts.push_back(Conflict(confName, VersionConstraint(constraintStr)));
    }

    void addConflict(const Conflict& conf) {
        conflicts.push_back(conf);
    }

    std::string getId() const {
        return name + "@" + version.toString();
    }

    bool operator==(const Package& other) const {
        return name == other.name && version == other.version;
    }

    bool operator<(const Package& other) const {
        if (name != other.name) return name < other.name;
        return version < other.version;
    }

    friend std::ostream& operator<<(std::ostream& os, const Package& p) {
        os << p.getId();
        return os;
    }
};

namespace std {
    template <>
    struct hash<Package> {
        size_t operator()(const Package& p) const {
            return hash<string>()(p.name) ^ (hash<Version>()(p.version) << 1);
        }
    };
}

#endif // PACKAGE_HPP
