#ifndef DPLL_SOLVER_HPP
#define DPLL_SOLVER_HPP

#include "CNFFormula.hpp"
#include <vector>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <map>

enum class SolverResult {
    SAT,
    UNSAT,
    TIMEOUT
};

struct SolverStats {
    long long decisions = 0;
    long long unitPropagations = 0;
    long long backtracks = 0;
    double timeMs = 0.0;

    void reset() {
        decisions = 0;
        unitPropagations = 0;
        backtracks = 0;
        timeMs = 0.0;
    }
};

class DPLLSolver {
private:
    CNFFormula formula;
    std::vector<int> assignment; // 0 = unassigned, 1 = true, -1 = false
    SolverStats stats;

public:
    DPLLSolver() {}
    explicit DPLLSolver(const CNFFormula& f) : formula(f) {}

    void setFormula(const CNFFormula& f) {
        formula = f;
    }

    const SolverStats& getStats() const {
        return stats;
    }

    const std::vector<int>& getModel() const {
        return assignment;
    }

    bool isVariableTrue(int var) const {
        if (var > 0 && var < (int)assignment.size()) {
            return assignment[var] == 1;
        }
        return false;
    }

    int getAssignmentValue(int var) const {
        if (var > 0 && var < (int)assignment.size()) {
            return assignment[var];
        }
        return 0;
    }

    SolverResult solve(const std::vector<Literal>& assumptions = {}) {
        stats.reset();
        auto startTime = std::chrono::high_resolution_clock::now();

        int nVars = formula.getNumVariables();
        assignment.assign(nVars + 1, 0);

        // Apply assumptions as initial unit assignments
        for (const auto& lit : assumptions) {
            int v = lit.var();
            int requiredVal = lit.sign() ? 1 : -1;
            if (assignment[v] != 0 && assignment[v] != requiredVal) {
                // Direct contradiction in assumptions
                auto endTime = std::chrono::high_resolution_clock::now();
                stats.timeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                return SolverResult::UNSAT;
            }
            assignment[v] = requiredVal;
        }

        bool result = dpll();

        auto endTime = std::chrono::high_resolution_clock::now();
        stats.timeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        return result ? SolverResult::SAT : SolverResult::UNSAT;
    }

private:
    // Boolean Constraint Propagation (Unit Propagation)
    // Returns false if a conflict (empty/unsatisfiable clause) is found
    bool bcp(std::vector<int>& currentAssignment, std::vector<int>& propagatedVars) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& clause : formula.getClauses()) {
                Literal unitLit;
                ClauseStatus status = clause.evaluate(currentAssignment, unitLit);

                if (status == ClauseStatus::UNSATISFIED) {
                    return false; // Conflict!
                }

                if (status == ClauseStatus::UNIT) {
                    int v = unitLit.var();
                    int val = unitLit.sign() ? 1 : -1;

                    if (currentAssignment[v] == 0) {
                        currentAssignment[v] = val;
                        propagatedVars.push_back(v);
                        stats.unitPropagations++;
                        changed = true;
                    } else if (currentAssignment[v] != val) {
                        return false; // Conflict!
                    }
                }
            }
        }
        return true;
    }

    // Pure Literal Elimination
    void pureLiteralElimination(std::vector<int>& currentAssignment, std::vector<int>& pureVars) {
        int nVars = formula.getNumVariables();
        std::vector<int> posCount(nVars + 1, 0);
        std::vector<int> negCount(nVars + 1, 0);

        for (const auto& clause : formula.getClauses()) {
            Literal dummy;
            if (clause.evaluate(currentAssignment, dummy) == ClauseStatus::SATISFIED) {
                continue; // Clause already satisfied, ignore its literals
            }
            for (const auto& lit : clause.literals) {
                int v = lit.var();
                if (currentAssignment[v] == 0) {
                    if (lit.sign()) posCount[v]++;
                    else negCount[v]++;
                }
            }
        }

        for (int v = 1; v <= nVars; v++) {
            if (currentAssignment[v] == 0) {
                // Only force pure negative literals to false; positive pure literals
                // may belong to unreferenced packages and should not be forced installed.
                if (negCount[v] > 0 && posCount[v] == 0) {
                    currentAssignment[v] = -1;
                    pureVars.push_back(v);
                }
            }
        }
    }

    // Branching Heuristic: Dynamic Largest Individual Sum (DLIS)
    // Counts occurrences of unassigned literals in unresolved clauses.
    Literal pickBranchingLiteral(const std::vector<int>& currentAssignment) {
        int nVars = formula.getNumVariables();
        std::vector<int> posWeight(nVars + 1, 0);
        std::vector<int> negWeight(nVars + 1, 0);

        for (const auto& clause : formula.getClauses()) {
            Literal dummy;
            if (clause.evaluate(currentAssignment, dummy) == ClauseStatus::SATISFIED) {
                continue;
            }
            for (const auto& lit : clause.literals) {
                int v = lit.var();
                if (currentAssignment[v] == 0) {
                    if (lit.sign()) posWeight[v]++;
                    else negWeight[v]++;
                }
            }
        }

        int bestVar = 0;
        bool bestSign = true;
        int maxWeight = -1;

        for (int v = 1; v <= nVars; v++) {
            if (currentAssignment[v] == 0) {
                if (posWeight[v] > maxWeight) {
                    maxWeight = posWeight[v];
                    bestVar = v;
                    bestSign = true;
                }
                if (negWeight[v] > maxWeight) {
                    maxWeight = negWeight[v];
                    bestVar = v;
                    bestSign = false;
                }
            }
        }

        if (bestVar == 0) {
            // Pick first unassigned variable if no weights found
            for (int v = 1; v <= nVars; v++) {
                if (currentAssignment[v] == 0) {
                    return Literal(v, true);
                }
            }
            return Literal(0);
        }

        return Literal(bestVar, bestSign);
    }

    // Core recursive DPLL procedure
    bool dpll() {
        std::vector<int> propagatedVars;

        // 1. Boolean Constraint Propagation (BCP)
        if (!bcp(assignment, propagatedVars)) {
            // Revert propagations
            for (int v : propagatedVars) {
                assignment[v] = 0;
            }
            return false;
        }

        // 2. Pure Literal Elimination
        std::vector<int> pureVars;
        pureLiteralElimination(assignment, pureVars);

        // Check if all clauses are satisfied
        bool allSatisfied = true;
        for (const auto& clause : formula.getClauses()) {
            Literal dummy;
            ClauseStatus st = clause.evaluate(assignment, dummy);
            if (st != ClauseStatus::SATISFIED) {
                allSatisfied = false;
                break;
            }
        }

        if (allSatisfied) {
            return true;
        }

        // 3. Pick branching variable
        Literal branchLit = pickBranchingLiteral(assignment);
        if (!branchLit.isValid()) {
            // All variables assigned and not conflicting -> SAT
            return true;
        }

        stats.decisions++;
        int v = branchLit.var();

        // 4. Branch 1: Try chosen literal sign
        assignment[v] = branchLit.sign() ? 1 : -1;
        if (dpll()) {
            return true;
        }

        // Backtrack
        stats.backtracks++;

        // 5. Branch 2: Invert literal and try opposite
        assignment[v] = branchLit.sign() ? -1 : 1;
        if (dpll()) {
            return true;
        }

        // Revert decision and local assignments
        assignment[v] = 0;
        for (int pv : pureVars) {
            assignment[pv] = 0;
        }
        for (int pv : propagatedVars) {
            assignment[pv] = 0;
        }

        return false;
    }
};

#endif // DPLL_SOLVER_HPP
