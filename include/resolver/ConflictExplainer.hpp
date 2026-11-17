#ifndef CONFLICT_EXPLAINER_HPP
#define CONFLICT_EXPLAINER_HPP

#include "DependencyEncoder.hpp"
#include "../sat/DPLLSolver.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <set>
#include <map>
#include <iostream>

struct ConflictDiagnosis {
    bool hasConflict;
    std::string rootCause;
    std::vector<std::string> conflictChain;
    std::vector<std::string> involvedClauses;
    std::string suggestedFix;
};

class ConflictExplainer {
public:
    static ConflictDiagnosis explain(
        const Repository& repo,
        const DependencyEncoder& encoder,
        const std::string& targetName,
        const VersionConstraint& targetConstraint
    ) {
        ConflictDiagnosis diag;
        diag.hasConflict = true;

        // 1. Check if target package exists in repository
        if (!repo.hasPackage(targetName)) {
            diag.rootCause = "Target package '" + targetName + "' was not found in the repository.";
            diag.suggestedFix = "Check the package name spelling or add it to the repository registry.";
            return diag;
        }

        // 2. Check if any version of target satisfies the constraint
        auto targetMatches = repo.findSatisfying(targetName, targetConstraint);
        if (targetMatches.empty()) {
            auto allVersions = repo.getPackages(targetName);
            std::stringstream ss;
            ss << "Target package '" << targetName << "' has no versions matching constraint '" 
               << targetConstraint.toString() << "'. Available versions: [";
            for (size_t i = 0; i < allVersions.size(); i++) {
                if (i > 0) ss << ", ";
                ss << allVersions[i].version.toString();
            }
            ss << "]";
            diag.rootCause = ss.str();
            diag.suggestedFix = "Adjust the target version constraint to match available versions.";
            return diag;
        }

        // 3. Check for unsatisfiable direct dependencies
        const auto& candidates = encoder.getCandidateMap();
        for (const auto& kv : candidates) {
            const Package& pkg = kv.second;
            for (const auto& dep : pkg.dependencies) {
                auto depMatches = repo.findSatisfying(dep.packageName, dep.constraint);
                if (depMatches.empty()) {
                    diag.conflictChain.push_back(pkg.getId() + " requires " + dep.toString());
                    if (!repo.hasPackage(dep.packageName)) {
                        diag.rootCause = "Missing dependency: " + pkg.getId() + " requires package '" + dep.packageName + "', which does not exist in the repository.";
                        diag.suggestedFix = "Add package '" + dep.packageName + "' to the repository or downgrade '" + pkg.name + "'.";
                    } else {
                        diag.rootCause = "Unsatisfiable dependency constraint: " + pkg.getId() + " requires " + dep.toString() + ", but no matching versions are available.";
                        diag.suggestedFix = "Publish a version of '" + dep.packageName + "' satisfying '" + dep.constraint.toString() + "'.";
                    }
                    return diag;
                }
            }
        }

        // 4. Check for diamond dependency / version incompatibility across distinct package names
        // Map: depName -> (sourcePkgName -> list of (sourcePkgId, VersionConstraint))
        std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, VersionConstraint>>>> sharedDepReqs;
        for (const auto& kv : candidates) {
            const Package& pkg = kv.second;
            for (const auto& dep : pkg.dependencies) {
                sharedDepReqs[dep.packageName][pkg.name].push_back({pkg.getId(), dep.constraint});
            }
        }

        for (const auto& depEntry : sharedDepReqs) {
            const std::string& depName = depEntry.first;
            const auto& bySourcePkg = depEntry.second;
            if (bySourcePkg.size() > 1) {
                auto allVersions = repo.getPackages(depName);
                // For each distinct source package name, compute which versions of depName
                // satisfy at least one candidate version of that source package.
                std::vector<std::pair<std::string, std::set<std::string>>> allowedSets;
                std::vector<std::string> summaryLines;

                for (const auto& srcEntry : bySourcePkg) {
                    const std::string& srcName = srcEntry.first;
                    const auto& reqs = srcEntry.second;
                    std::set<std::string> allowed;
                    for (const auto& vPkg : allVersions) {
                        for (const auto& r : reqs) {
                            if (r.second.matches(vPkg.version)) {
                                allowed.insert(vPkg.getId());
                                break;
                            }
                        }
                    }
                    allowedSets.push_back({srcName, allowed});
                    for (const auto& r : reqs) {
                        summaryLines.push_back(r.first + " requires " + depName + " " + r.second.toString());
                    }
                }

                // Check if the intersection across all distinct source packages is empty
                bool hasCommonVersion = false;
                for (const auto& vPkg : allVersions) {
                    bool okForAll = true;
                    for (const auto& aset : allowedSets) {
                        if (aset.second.find(vPkg.getId()) == aset.second.end()) {
                            okForAll = false;
                            break;
                        }
                    }
                    if (okForAll) {
                        hasCommonVersion = true;
                        break;
                    }
                }

                if (!hasCommonVersion) {
                    std::stringstream ss;
                    ss << "Version collision on shared dependency '" << depName << "':\n";
                    for (const auto& line : summaryLines) {
                        ss << "  - " << line << "\n";
                        diag.conflictChain.push_back(line);
                    }
                    ss << "No single version of '" << depName << "' satisfies all conflicting constraints simultaneously.";
                    diag.rootCause = ss.str();
                    diag.suggestedFix = "Align version requirements across dependencies or select compatible version ranges.";
                    return diag;
                }
            }
        }

        // 5. Check for explicit conflict rules that prevent satisfaction
        std::vector<std::string> explicitConflictMsgs;
        for (const auto& kv : candidates) {
            const Package& pkg = kv.second;
            for (const auto& conf : pkg.conflicts) {
                auto matching = repo.findSatisfying(conf.packageName, conf.constraint);
                for (const auto& m : matching) {
                    if (candidates.find(m.getId()) != candidates.end()) {
                        std::string msg = pkg.getId() + " explicitly conflicts with " + m.getId();
                        diag.conflictChain.push_back(msg);
                        explicitConflictMsgs.push_back(msg);
                    }
                }
            }
        }

        // 6. Generic UNSAT core extraction from CNF clauses
        const auto& origins = encoder.getClauseOrigins();
        for (const auto& origin : origins) {
            if (origin.type == ClauseOrigin::Type::EXPLICIT_CONFLICT ||
                origin.type == ClauseOrigin::Type::UNSATISFIABLE_DEP ||
                origin.type == ClauseOrigin::Type::AT_MOST_ONE) {
                diag.involvedClauses.push_back(origin.description);
            }
        }

        if (!explicitConflictMsgs.empty()) {
            diag.rootCause = "Explicit package conflict prevents installation:\n  - " + explicitConflictMsgs[0];
            diag.suggestedFix = "Remove or replace one of the conflicting packages, or relax version constraints.";
        } else {
            diag.rootCause = "The dependency constraints form an unsatisfiable Boolean proposition (UNSAT).\n"
                             "Mutually exclusive package versions or circular conflict rules prevent a valid configuration.";
            diag.suggestedFix = "Review the conflicting constraints in the report and relax version restrictions.";
        }
        return diag;
    }
};

#endif // CONFLICT_EXPLAINER_HPP
