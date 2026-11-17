#ifndef REPOSITORY_HPP
#define REPOSITORY_HPP

#include "Package.hpp"
#include "SimpleJSON.hpp"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

class Repository {
private:
    std::vector<Package> packages;
    // Fast index by package name: name -> list of indices in `packages`
    std::map<std::string, std::vector<size_t>> nameIndex;

public:
    Repository() {}

    void addPackage(const Package& pkg) {
        size_t idx = packages.size();
        packages.push_back(pkg);
        nameIndex[pkg.name].push_back(idx);
    }

    size_t size() const {
        return packages.size();
    }

    const std::vector<Package>& getAllPackages() const {
        return packages;
    }

    std::vector<std::string> getAllPackageNames() const {
        std::vector<std::string> names;
        for (const auto& kv : nameIndex) {
            names.push_back(kv.first);
        }
        return names;
    }

    bool hasPackage(const std::string& name) const {
        return nameIndex.find(name) != nameIndex.end();
    }

    std::vector<Package> getPackages(const std::string& name) const {
        std::vector<Package> res;
        auto it = nameIndex.find(name);
        if (it != nameIndex.end()) {
            for (size_t idx : it->second) {
                res.push_back(packages[idx]);
            }
        }
        // Sort descending by version (latest first)
        std::sort(res.begin(), res.end(), [](const Package& a, const Package& b) {
            return b.version < a.version;
        });
        return res;
    }

    std::vector<Package> findSatisfying(const std::string& name, const VersionConstraint& vc) const {
        std::vector<Package> res;
        auto it = nameIndex.find(name);
        if (it != nameIndex.end()) {
            for (size_t idx : it->second) {
                if (vc.matches(packages[idx].version)) {
                    res.push_back(packages[idx]);
                }
            }
        }
        // Sort descending by version (latest first)
        std::sort(res.begin(), res.end(), [](const Package& a, const Package& b) {
            return b.version < a.version;
        });
        return res;
    }

    const Package* getPackage(const std::string& name, const Version& ver) const {
        auto it = nameIndex.find(name);
        if (it != nameIndex.end()) {
            for (size_t idx : it->second) {
                if (packages[idx].version == ver) {
                    return &packages[idx];
                }
            }
        }
        return nullptr;
    }

    static Repository loadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open repository file: " + filepath);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return loadFromJSON(buffer.str());
    }

    static Repository loadFromJSON(const std::string& jsonStr) {
        Repository repo;
        JSON root = JSON::parse(jsonStr);

        JSON pkgsArray;
        if (root.isArray()) {
            pkgsArray = root;
        } else if (root.isObject() && root.contains("packages")) {
            pkgsArray = root["packages"];
        } else {
            throw std::runtime_error("JSON repository must contain a 'packages' array or be an array of packages");
        }

        for (const auto& item : pkgsArray.asArray()) {
            if (!item.contains("name") || !item.contains("version")) {
                continue;
            }
            std::string name = item["name"].asString();
            std::string verStr = item["version"].asString();
            Package pkg(name, verStr);

            // Dependencies
            if (item.contains("dependencies")) {
                const auto& deps = item["dependencies"];
                if (deps.isObject()) {
                    for (const auto& kv : deps.asObject()) {
                        pkg.addDependency(kv.first, kv.second.asString());
                    }
                } else if (deps.isArray()) {
                    for (const auto& depItem : deps.asArray()) {
                        if (depItem.contains("name")) {
                            std::string depName = depItem["name"].asString();
                            std::string depVer = depItem.contains("version") ? depItem["version"].asString() : "*";
                            pkg.addDependency(depName, depVer);
                        }
                    }
                }
            }

            // Conflicts
            if (item.contains("conflicts")) {
                const auto& confs = item["conflicts"];
                if (confs.isObject()) {
                    for (const auto& kv : confs.asObject()) {
                        pkg.addConflict(kv.first, kv.second.asString());
                    }
                } else if (confs.isArray()) {
                    for (const auto& confItem : confs.asArray()) {
                        if (confItem.contains("name")) {
                            std::string confName = confItem["name"].asString();
                            std::string confVer = confItem.contains("version") ? confItem["version"].asString() : "*";
                            pkg.addConflict(confName, confVer);
                        }
                    }
                }
            }

            repo.addPackage(pkg);
        }

        return repo;
    }
};

#endif // REPOSITORY_HPP
