#ifndef CLAUSE_HPP
#define CLAUSE_HPP

#include "Literal.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>

enum class ClauseStatus {
    SATISFIED,
    UNSATISFIED,
    UNIT,
    UNRESOLVED
};

class Clause {
public:
    std::vector<Literal> literals;

    Clause() {}
    explicit Clause(const std::vector<Literal>& lits) : literals(lits) {}

    void addLiteral(const Literal& lit) {
        // Avoid duplicate literals in the clause
        for (const auto& l : literals) {
            if (l == lit) return;
        }
        literals.push_back(lit);
    }

    size_t size() const {
        return literals.size();
    }

    bool empty() const {
        return literals.empty();
    }

    // Evaluate clause given assignments:
    // assignment array: index = variable id (1-based), value = 0 (unassigned), 1 (true), -1 (false)
    ClauseStatus evaluate(const std::vector<int>& assignment, Literal& unitLit) const {
        int unassignedCount = 0;
        Literal lastUnassigned;

        for (const auto& lit : literals) {
            int v = lit.var();
            int assign = (v < (int)assignment.size()) ? assignment[v] : 0;

            if (assign != 0) {
                // Check if this literal is satisfied
                if ((lit.sign() && assign == 1) || (!lit.sign() && assign == -1)) {
                    return ClauseStatus::SATISFIED;
                }
                // Otherwise this literal is falsified, continue checking
            } else {
                unassignedCount++;
                lastUnassigned = lit;
            }
        }

        if (unassignedCount == 0) {
            return ClauseStatus::UNSATISFIED; // All literals are false -> conflict
        }

        if (unassignedCount == 1) {
            unitLit = lastUnassigned;
            return ClauseStatus::UNIT;
        }

        return ClauseStatus::UNRESOLVED;
    }

    std::string toString() const {
        if (literals.empty()) return "()";
        std::string s = "(";
        for (size_t i = 0; i < literals.size(); i++) {
            if (i > 0) s += " v ";
            s += literals[i].toString();
        }
        s += ")";
        return s;
    }

    friend std::ostream& operator<<(std::ostream& os, const Clause& c) {
        os << c.toString();
        return os;
    }
};

#endif // CLAUSE_HPP
