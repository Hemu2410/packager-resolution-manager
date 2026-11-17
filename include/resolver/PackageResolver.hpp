#ifndef PACKAGE_RESOLVER_HPP
#define PACKAGE_RESOLVER_HPP

#include "../core/Repository.hpp"
#include "../graph/DependencyGraph.hpp"
#include "../graph/TopologicalSort.hpp"
#include "../sat/DPLLSolver.hpp"
#include "DependencyEncoder.hpp"
#include "ConflictExplainer.hpp"
#include "../core/SimpleJSON.hpp"
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <set>
#include <queue>
#include <map>
#include <algorithm>

struct ResolutionResult {
    bool success = false;
    std::string targetPackage;
    std::string targetConstraint;
    
    // Graph pruning metrics
    size_t totalRepoPackages = 0;
    size_t candidatePackagesCount = 0;
    std::vector<Package> reachableCandidates;
    std::vector<Package> prunedPackages;

    // SAT metrics & artifacts
    int numVariables = 0;
    size_t numClauses = 0;
    SolverStats solverStats;
    CNFFormula formula;
    std::vector<ClauseOrigin> clauseOrigins;
    std::vector<int> variableModel; // 0 = unassigned, 1 = true, -1 = false

    // Solution
    std::vector<Package> resolvedPackages;
    std::vector<Package> installationOrder;
    std::map<std::string, std::vector<std::string>> dependencyTree;
    bool hasCycle = false;
    std::vector<std::string> cyclePath;

    // Conflict diagnosis if failed
    ConflictDiagnosis conflictInfo;

    static JSON packageToJSON(const Package& p) {
        JSON item = JSON::makeObject();
        item["id"] = p.getId();
        item["name"] = p.name;
        item["version"] = p.version.toString();

        JSON deps = JSON::makeObject();
        for (const auto& d : p.dependencies) {
            deps[d.packageName] = d.constraint.toString();
        }
        item["dependencies"] = deps;

        JSON confs = JSON::makeObject();
        for (const auto& c : p.conflicts) {
            confs[c.packageName] = c.constraint.toString();
        }
        item["conflicts"] = confs;
        return item;
    }

    static std::string clauseTypeToString(ClauseOrigin::Type t) {
        switch (t) {
            case ClauseOrigin::Type::TARGET_GOAL: return "TARGET_GOAL";
            case ClauseOrigin::Type::DEPENDENCY: return "DEPENDENCY";
            case ClauseOrigin::Type::AT_MOST_ONE: return "AT_MOST_ONE";
            case ClauseOrigin::Type::EXPLICIT_CONFLICT: return "EXPLICIT_CONFLICT";
            case ClauseOrigin::Type::UNSATISFIABLE_DEP: return "UNSATISFIABLE_DEP";
        }
        return "UNKNOWN";
    }

    JSON toJSON() const {
        JSON root = JSON::makeObject();
        root["success"] = success;
        root["targetPackage"] = targetPackage;
        root["targetConstraint"] = targetConstraint;

        // Metrics
        JSON metrics = JSON::makeObject();
        metrics["totalRepoPackages"] = (int)totalRepoPackages;
        metrics["candidatePackagesCount"] = (int)candidatePackagesCount;
        metrics["prunedCount"] = (int)prunedPackages.size();
        metrics["numVariables"] = numVariables;
        metrics["numClauses"] = (int)numClauses;
        metrics["decisions"] = (int)solverStats.decisions;
        metrics["unitPropagations"] = (int)solverStats.unitPropagations;
        metrics["backtracks"] = (int)solverStats.backtracks;
        metrics["timeMs"] = solverStats.timeMs;
        root["metrics"] = metrics;

        // Candidates & Pruned Packages (for Graph Pruning view)
        JSON candArr = JSON::makeArray();
        for (const auto& p : reachableCandidates) {
            candArr.push_back(packageToJSON(p));
        }
        root["candidates"] = candArr;

        JSON prunedArr = JSON::makeArray();
        for (const auto& p : prunedPackages) {
            prunedArr.push_back(packageToJSON(p));
        }
        root["prunedPackages"] = prunedArr;

        // SAT Variables & Truth Assignments
        JSON varsArr = JSON::makeArray();
        for (int v = 1; v <= formula.getNumVariables(); v++) {
            JSON vObj = JSON::makeObject();
            vObj["id"] = v;
            vObj["name"] = formula.getVariableName(v);
            if (v < (int)variableModel.size() && variableModel[v] != 0) {
                vObj["assigned"] = (variableModel[v] == 1);
            } else {
                vObj["assigned"] = JSON(nullptr);
            }
            varsArr.push_back(vObj);
        }
        root["variables"] = varsArr;

        // SAT Clauses with origins
        JSON clausesArr = JSON::makeArray();
        const auto& rawClauses = formula.getClauses();
        for (size_t i = 0; i < rawClauses.size(); i++) {
            JSON cObj = JSON::makeObject();
            cObj["index"] = (int)(i + 1);
            if (i < clauseOrigins.size()) {
                cObj["type"] = clauseTypeToString(clauseOrigins[i].type);
                cObj["description"] = clauseOrigins[i].description;
            } else {
                cObj["type"] = "OTHER";
                cObj["description"] = "";
            }
            cObj["formula"] = rawClauses[i].toString();

            std::string humanFormula = "(";
            JSON litArr = JSON::makeArray();
            for (size_t j = 0; j < rawClauses[i].literals.size(); j++) {
                const auto& lit = rawClauses[i].literals[j];
                litArr.push_back(lit.toInt());
                if (j > 0) humanFormula += " v ";
                if (!lit.sign()) humanFormula += "~";
                humanFormula += formula.getVariableName(lit.var());
            }
            if (rawClauses[i].literals.empty()) humanFormula += "FALSE";
            humanFormula += ")";
            cObj["literals"] = litArr;
            cObj["humanFormula"] = humanFormula;
            clausesArr.push_back(cObj);
        }
        root["clauses"] = clausesArr;
        root["dimacs"] = formula.toDIMACS();

        if (success) {
            JSON resolved = JSON::makeArray();
            for (const auto& p : resolvedPackages) {
                resolved.push_back(packageToJSON(p));
            }
            root["resolvedPackages"] = resolved;

            JSON order = JSON::makeArray();
            for (const auto& p : installationOrder) {
                order.push_back(p.getId());
            }
            root["installationOrder"] = order;

            JSON tree = JSON::makeObject();
            for (const auto& kv : dependencyTree) {
                JSON deps = JSON::makeArray();
                for (const auto& d : kv.second) {
                    deps.push_back(d);
                }
                tree[kv.first] = deps;
            }
            root["dependencyTree"] = tree;

            root["hasCycle"] = hasCycle;
            if (hasCycle) {
                JSON cycleArr = JSON::makeArray();
                for (const auto& node : cyclePath) {
                    cycleArr.push_back(node);
                }
                root["cyclePath"] = cycleArr;
            }
        } else {
            JSON conflict = JSON::makeObject();
            conflict["rootCause"] = conflictInfo.rootCause;
            conflict["suggestedFix"] = conflictInfo.suggestedFix;
            JSON chain = JSON::makeArray();
            for (const auto& c : conflictInfo.conflictChain) {
                chain.push_back(c);
            }
            conflict["conflictChain"] = chain;
            JSON involved = JSON::makeArray();
            for (const auto& c : conflictInfo.involvedClauses) {
                involved.push_back(c);
            }
            conflict["involvedClauses"] = involved;
            root["conflict"] = conflict;
        }

        return root;
    }
};

class PackageResolver {
private:
    // Extract only packages transitively reachable from the selected root in the SAT model
    static std::vector<Package> extractReachableSelected(
        const std::vector<Package>& allSelected,
        const std::string& targetName
    ) {
        std::map<std::string, Package> selectedByName;
        for (const auto& p : allSelected) {
            selectedByName[p.name] = p;
        }
        auto rootIt = selectedByName.find(targetName);
        if (rootIt == selectedByName.end()) {
            return allSelected;
        }

        std::set<std::string> visited;
        std::queue<Package> q;
        std::vector<Package> result;

        visited.insert(rootIt->second.getId());
        q.push(rootIt->second);
        result.push_back(rootIt->second);

        while (!q.empty()) {
            Package curr = q.front();
            q.pop();
            for (const auto& dep : curr.dependencies) {
                auto it = selectedByName.find(dep.packageName);
                if (it != selectedByName.end() && dep.constraint.matches(it->second.version)) {
                    if (visited.insert(it->second.getId()).second) {
                        result.push_back(it->second);
                        q.push(it->second);
                    }
                }
            }
        }
        return result;
    }

public:
    static ResolutionResult resolve(
        const Repository& repo,
        const std::string& targetName,
        const VersionConstraint& targetConstraint
    ) {
        ResolutionResult result;
        result.targetPackage = targetName;
        result.targetConstraint = targetConstraint.toString();
        result.totalRepoPackages = repo.size();

        // 1. Graph Pruning: Reachable Candidate Subgraph
        auto candidates = DependencyGraph::getReachableCandidates(repo, targetName, targetConstraint);
        result.candidatePackagesCount = candidates.size();
        result.reachableCandidates = candidates;

        std::set<std::string> reachableIds;
        for (const auto& c : candidates) {
            reachableIds.insert(c.getId());
        }
        for (const auto& p : repo.getAllPackages()) {
            if (reachableIds.find(p.getId()) == reachableIds.end()) {
                result.prunedPackages.push_back(p);
            }
        }

        if (candidates.empty()) {
            result.success = false;
            DependencyEncoder emptyEncoder;
            result.conflictInfo = ConflictExplainer::explain(repo, emptyEncoder, targetName, targetConstraint);
            return result;
        }

        // 2. Logic Phase: CNF Encoding
        DependencyEncoder encoder;
        encoder.encode(candidates, targetName, targetConstraint);

        result.formula = encoder.getFormula();
        result.clauseOrigins = encoder.getClauseOrigins();
        result.numVariables = result.formula.getNumVariables();
        result.numClauses = result.formula.getNumClauses();

        // 3. SAT Solving Phase (with Max-SAT SemVer preference)
        std::vector<Package> targetMatches;
        for (const auto& p : candidates) {
            if (p.name == targetName && targetConstraint.matches(p.version)) {
                targetMatches.push_back(p);
            }
        }
        std::sort(targetMatches.begin(), targetMatches.end(), [](const Package& a, const Package& b) {
            return b.version < a.version; // latest first
        });

        DPLLSolver solver(result.formula);
        SolverResult satStatus = SolverResult::UNSAT;
        std::vector<Literal> lockedAssumptions;

        // Lock highest satisfiable target version
        for (const auto& preferredTarget : targetMatches) {
            int var = result.formula.getVariable(preferredTarget.getId());
            if (var != 0) {
                satStatus = solver.solve({Literal(var, true)});
                if (satStatus == SolverResult::SAT) {
                    lockedAssumptions.push_back(Literal(var, true));
                    break;
                }
            }
        }

        if (satStatus != SolverResult::SAT) {
            satStatus = solver.solve();
        }

        if (satStatus != SolverResult::SAT) {
            result.solverStats = solver.getStats();
            result.variableModel = solver.getModel();
            result.success = false;
            result.conflictInfo = ConflictExplainer::explain(repo, encoder, targetName, targetConstraint);
            return result;
        }

        // Iterative SemVer Upgrade Pass (Max-SAT preference for transitive dependencies):
        // Group all candidate versions by package name, sorted descending
        std::map<std::string, std::vector<Package>> candidatesByName;
        for (const auto& p : candidates) {
            candidatesByName[p.name].push_back(p);
        }
        for (auto& kv : candidatesByName) {
            std::sort(kv.second.begin(), kv.second.end(), [](const Package& a, const Package& b) {
                return b.version < a.version;
            });
        }

        // Accumulate cumulative solver statistics across the initial solve + upgrade checks
        SolverStats totalStats = solver.getStats();
        std::vector<int> bestModel = solver.getModel();

        // Extract initial reachable selection and iteratively try newer versions for each dependency
        auto currentSelected = [&]() {
            std::vector<Package> raw;
            for (const auto& pkg : candidates) {
                int var = result.formula.getVariable(pkg.getId());
                if (var > 0 && var < (int)bestModel.size() && bestModel[var] == 1) {
                    raw.push_back(pkg);
                }
            }
            return extractReachableSelected(raw, targetName);
        };

        std::set<std::string> processedNames;
        processedNames.insert(targetName);

        bool upgradedAny = true;
        while (upgradedAny) {
            upgradedAny = false;
            auto activePkgs = currentSelected();
            for (const auto& active : activePkgs) {
                if (!processedNames.insert(active.name).second) {
                    continue;
                }
                const auto& versions = candidatesByName[active.name];
                for (const auto& candVer : versions) {
                    if (candVer.version <= active.version) {
                        break; // already at or above remaining candidates
                    }
                    int candVar = result.formula.getVariable(candVer.getId());
                    if (candVar == 0) continue;

                    std::vector<Literal> testAssumptions = lockedAssumptions;
                    testAssumptions.push_back(Literal(candVar, true));
                    if (solver.solve(testAssumptions) == SolverResult::SAT) {
                        lockedAssumptions.push_back(Literal(candVar, true));
                        bestModel = solver.getModel();
                        const auto& st = solver.getStats();
                        totalStats.decisions += st.decisions;
                        totalStats.unitPropagations += st.unitPropagations;
                        totalStats.backtracks += st.backtracks;
                        totalStats.timeMs += st.timeMs;
                        upgradedAny = true;
                        break;
                    }
                }
                if (upgradedAny) break;
            }
        }

        result.solverStats = totalStats;
        result.variableModel = bestModel;

        // 4. Model Extraction (only packages reachable from chosen root)
        result.success = true;
        result.resolvedPackages = currentSelected();

        // 5. Post-Resolution Graph Phase: Topological Sort & Install Sequence
        OrderResult order = TopologicalSort::computeInstallationOrder(result.resolvedPackages);
        result.hasCycle = order.hasCycle;
        result.installationOrder = order.installOrder;
        result.cyclePath = order.cyclePath;
        result.dependencyTree = order.dependencyTree;

        return result;
    }
};

#endif // PACKAGE_RESOLVER_HPP
