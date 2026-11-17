#ifndef DEPENDENCY_GRAPH_HPP
#define DEPENDENCY_GRAPH_HPP

#include "../core/Package.hpp"
#include "../core/Repository.hpp"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <iostream>

class DependencyGraph {
public:
    // Reachability Pruning:
    // Given the repository and the initial install goal (package name + constraint),
    // extract ONLY the subset of packages that are transitively reachable.
    static std::vector<Package> getReachableCandidates(
        const Repository& repo,
        const std::string& targetName,
        const VersionConstraint& targetConstraint
    ) {
        std::set<std::string> visitedIds;
        std::vector<Package> candidates;
        std::queue<Package> queue;

        // Find initial matching target packages
        auto initialTargets = repo.findSatisfying(targetName, targetConstraint);
        for (const auto& pkg : initialTargets) {
            if (visitedIds.insert(pkg.getId()).second) {
                candidates.push_back(pkg);
                queue.push(pkg);
            }
        }

        // BFS traversal
        while (!queue.empty()) {
            Package current = queue.front();
            queue.pop();

            for (const auto& dep : current.dependencies) {
                auto matchingDeps = repo.findSatisfying(dep.packageName, dep.constraint);
                for (const auto& depPkg : matchingDeps) {
                    if (visitedIds.insert(depPkg.getId()).second) {
                        candidates.push_back(depPkg);
                        queue.push(depPkg);
                    }
                }
            }
        }

        return candidates;
    }

    // Reachability Pruning for multiple install goals:
    static std::vector<Package> getReachableCandidates(
        const Repository& repo,
        const std::vector<std::pair<std::string, VersionConstraint>>& targets
    ) {
        std::set<std::string> visitedIds;
        std::vector<Package> candidates;
        std::queue<Package> queue;

        for (const auto& target : targets) {
            auto matches = repo.findSatisfying(target.first, target.second);
            for (const auto& pkg : matches) {
                if (visitedIds.insert(pkg.getId()).second) {
                    candidates.push_back(pkg);
                    queue.push(pkg);
                }
            }
        }

        while (!queue.empty()) {
            Package current = queue.front();
            queue.pop();

            for (const auto& dep : current.dependencies) {
                auto matchingDeps = repo.findSatisfying(dep.packageName, dep.constraint);
                for (const auto& depPkg : matchingDeps) {
                    if (visitedIds.insert(depPkg.getId()).second) {
                        candidates.push_back(depPkg);
                        queue.push(depPkg);
                    }
                }
            }
        }

        return candidates;
    }
};

#endif // DEPENDENCY_GRAPH_HPP
