#ifndef TOPOLOGICAL_SORT_HPP
#define TOPOLOGICAL_SORT_HPP

#include "../core/Package.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <functional>
#include <iostream>

struct OrderResult {
    bool hasCycle;
    std::vector<Package> installOrder; // Order from first to install to last
    std::vector<std::string> cyclePath; // Non-empty if cycle detected
    std::map<std::string, std::vector<std::string>> dependencyTree; // pkgId -> list of dependency pkgIds
};

class TopologicalSort {
public:
    static OrderResult computeInstallationOrder(const std::vector<Package>& selectedPackages) {
        OrderResult result;
        result.hasCycle = false;

        // Map from package name to Package object
        std::map<std::string, Package> nameToPkg;
        std::map<std::string, std::string> nameToId;
        for (const auto& pkg : selectedPackages) {
            nameToPkg[pkg.name] = pkg;
            nameToId[pkg.name] = pkg.getId();
        }

        // Build directed graph:
        // Precedence edge: dependency -> dependent (dependency must be installed before dependent)
        std::map<std::string, std::set<std::string>> adj; // depPkgId -> set of dependentPkgIds
        std::map<std::string, int> inDegree; // pkgId -> number of unresolved dependencies
        std::map<std::string, std::vector<std::string>> depTree; // dependentPkgId -> list of depPkgIds

        for (const auto& pkg : selectedPackages) {
            std::string id = pkg.getId();
            if (inDegree.find(id) == inDegree.end()) {
                inDegree[id] = 0;
            }

            for (const auto& dep : pkg.dependencies) {
                auto it = nameToPkg.find(dep.packageName);
                if (it != nameToPkg.end()) {
                    const Package& targetDep = it->second;
                    if (dep.constraint.matches(targetDep.version)) {
                        std::string targetId = targetDep.getId();
                        adj[targetId].insert(id);
                        inDegree[id]++;
                        depTree[id].push_back(targetId);
                    }
                }
            }
        }

        result.dependencyTree = depTree;

        // Kahn's Algorithm
        std::queue<std::string> q;
        for (const auto& kv : inDegree) {
            if (kv.second == 0) {
                q.push(kv.first);
            }
        }

        std::vector<std::string> order;
        while (!q.empty()) {
            std::string u = q.front();
            q.pop();
            order.push_back(u);

            for (const std::string& v : adj[u]) {
                inDegree[v]--;
                if (inDegree[v] == 0) {
                    q.push(v);
                }
            }
        }

        if (order.size() < selectedPackages.size()) {
            // Cycle detected!
            result.hasCycle = true;
            result.cyclePath = findCycle(selectedPackages, depTree);
            return result;
        }

        // Convert ordered IDs back to Packages
        std::map<std::string, Package> idToPkg;
        for (const auto& p : selectedPackages) {
            idToPkg[p.getId()] = p;
        }
        for (const auto& id : order) {
            result.installOrder.push_back(idToPkg[id]);
        }

        return result;
    }

private:
    static std::vector<std::string> findCycle(
        const std::vector<Package>& packages,
        const std::map<std::string, std::vector<std::string>>& depTree
    ) {
        // Find cycle using DFS (color: 0=unvisited, 1=visiting, 2=visited)
        std::map<std::string, int> color;
        std::map<std::string, std::string> parent;
        std::vector<std::string> cycle;

        std::function<bool(const std::string&)> dfs = [&](const std::string& u) -> bool {
            color[u] = 1;
            auto it = depTree.find(u);
            if (it != depTree.end()) {
                for (const auto& v : it->second) {
                    if (color[v] == 1) { // back-edge found!
                        cycle.push_back(v);
                        std::string curr = u;
                        while (curr != v && !curr.empty()) {
                            cycle.push_back(curr);
                            curr = parent[curr];
                        }
                        cycle.push_back(v);
                        std::reverse(cycle.begin(), cycle.end());
                        return true;
                    }
                    if (color[v] == 0) {
                        parent[v] = u;
                        if (dfs(v)) return true;
                    }
                }
            }
            color[u] = 2;
            return false;
        };

        for (const auto& p : packages) {
            std::string id = p.getId();
            if (color[id] == 0) {
                if (dfs(id)) break;
            }
        }

        return cycle;
    }
};

#endif // TOPOLOGICAL_SORT_HPP
