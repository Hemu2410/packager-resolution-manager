#ifndef DEPENDENCY_ENCODER_HPP
#define DEPENDENCY_ENCODER_HPP

#include "../core/Package.hpp"
#include "../core/Repository.hpp"
#include "../sat/CNFFormula.hpp"
#include <vector>
#include <string>
#include <map>
#include <set>

struct ClauseOrigin {
    enum class Type {
        TARGET_GOAL,
        DEPENDENCY,
        AT_MOST_ONE,
        EXPLICIT_CONFLICT,
        UNSATISFIABLE_DEP
    };

    Type type;
    std::string sourcePackage;
    std::string targetPackage;
    std::string description;

    std::string toString() const {
        return description;
    }
};

class DependencyEncoder {
private:
    CNFFormula formula;
    std::vector<ClauseOrigin> clauseOrigins;
    std::map<std::string, Package> candidateMap; // id -> Package

public:
    DependencyEncoder() {}

    const CNFFormula& getFormula() const {
        return formula;
    }

    const std::vector<ClauseOrigin>& getClauseOrigins() const {
        return clauseOrigins;
    }

    const std::map<std::string, Package>& getCandidateMap() const {
        return candidateMap;
    }

    // Encodes the reachable candidate packages and the target request into CNF
    void encode(
        const std::vector<Package>& candidates,
        const std::string& targetName,
        const VersionConstraint& targetConstraint
    ) {
        formula = CNFFormula();
        clauseOrigins.clear();
        candidateMap.clear();

        for (const auto& pkg : candidates) {
            candidateMap[pkg.getId()] = pkg;
            formula.getOrCreateVariable(pkg.getId());
        }

        // 1. Group candidate versions by package name
        std::map<std::string, std::vector<Package>> pkgsByName;
        for (const auto& pkg : candidates) {
            pkgsByName[pkg.name].push_back(pkg);
        }

        // 2. Target Goal Clause: At least one matching version of targetName must be installed
        std::vector<Literal> targetLits;
        auto targetMatches = pkgsByName[targetName];
        for (const auto& pkg : targetMatches) {
            if (targetConstraint.matches(pkg.version)) {
                int var = formula.getVariable(pkg.getId());
                targetLits.push_back(Literal(var, true));
            }
        }

        if (targetLits.empty()) {
            // Cannot satisfy target constraint with any package in repository
            // Add an empty clause (immediate UNSAT)
            formula.addClause(Clause());
            ClauseOrigin origin;
            origin.type = ClauseOrigin::Type::TARGET_GOAL;
            origin.sourcePackage = targetName;
            origin.description = "No version of " + targetName + " satisfies constraint: " + targetConstraint.toString();
            clauseOrigins.push_back(origin);
            return;
        }

        formula.addClause(Clause(targetLits));
        {
            ClauseOrigin origin;
            origin.type = ClauseOrigin::Type::TARGET_GOAL;
            origin.sourcePackage = targetName;
            origin.description = "Install goal: " + targetName + " (" + targetConstraint.toString() + ")";
            clauseOrigins.push_back(origin);
        }

        // 3. Mutual Exclusion (At-Most-One version per package)
        for (const auto& kv : pkgsByName) {
            const auto& versionList = kv.second;
            size_t n = versionList.size();
            for (size_t i = 0; i < n; i++) {
                for (size_t j = i + 1; j < n; j++) {
                    int var1 = formula.getVariable(versionList[i].getId());
                    int var2 = formula.getVariable(versionList[j].getId());

                    // Clause: (~X1 v ~X2)
                    formula.addClause(Clause({Literal(var1, false), Literal(var2, false)}));

                    ClauseOrigin origin;
                    origin.type = ClauseOrigin::Type::AT_MOST_ONE;
                    origin.sourcePackage = kv.first;
                    origin.description = "Cannot install both " + versionList[i].getId() + " and " + versionList[j].getId();
                    clauseOrigins.push_back(origin);
                }
            }
        }

        // 4. Dependency Implications
        for (const auto& pkg : candidates) {
            int srcVar = formula.getVariable(pkg.getId());

            for (const auto& dep : pkg.dependencies) {
                std::vector<Literal> depLits;
                // Add ~srcVar
                depLits.push_back(Literal(srcVar, false));

                auto it = pkgsByName.find(dep.packageName);
                bool foundMatching = false;
                if (it != pkgsByName.end()) {
                    for (const auto& candDep : it->second) {
                        if (dep.constraint.matches(candDep.version)) {
                            int depVar = formula.getVariable(candDep.getId());
                            depLits.push_back(Literal(depVar, true));
                            foundMatching = true;
                        }
                    }
                }

                if (!foundMatching) {
                    // srcPkg depends on something impossible to satisfy -> srcPkg cannot be installed: (~srcVar)
                    formula.addClause(Clause({Literal(srcVar, false)}));
                    ClauseOrigin origin;
                    origin.type = ClauseOrigin::Type::UNSATISFIABLE_DEP;
                    origin.sourcePackage = pkg.getId();
                    origin.targetPackage = dep.packageName;
                    origin.description = pkg.getId() + " requires " + dep.toString() + ", but no matching version exists";
                    clauseOrigins.push_back(origin);
                } else {
                    // (~srcVar v dep1 v dep2 ...)
                    formula.addClause(Clause(depLits));
                    ClauseOrigin origin;
                    origin.type = ClauseOrigin::Type::DEPENDENCY;
                    origin.sourcePackage = pkg.getId();
                    origin.targetPackage = dep.packageName;
                    origin.description = pkg.getId() + " requires " + dep.toString();
                    clauseOrigins.push_back(origin);
                }
            }
        }

        // 5. Explicit Conflicts
        for (const auto& pkg : candidates) {
            int srcVar = formula.getVariable(pkg.getId());

            for (const auto& conf : pkg.conflicts) {
                auto it = pkgsByName.find(conf.packageName);
                if (it != pkgsByName.end()) {
                    for (const auto& candConf : it->second) {
                        if (conf.constraint.matches(candConf.version)) {
                            int confVar = formula.getVariable(candConf.getId());
                            // (~srcVar v ~confVar)
                            formula.addClause(Clause({Literal(srcVar, false), Literal(confVar, false)}));

                            ClauseOrigin origin;
                            origin.type = ClauseOrigin::Type::EXPLICIT_CONFLICT;
                            origin.sourcePackage = pkg.getId();
                            origin.targetPackage = candConf.getId();
                            origin.description = pkg.getId() + " explicitly conflicts with " + candConf.getId();
                            clauseOrigins.push_back(origin);
                        }
                    }
                }
            }
        }
    }
};

#endif // DEPENDENCY_ENCODER_HPP
