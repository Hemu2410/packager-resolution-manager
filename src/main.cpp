#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

#include "../include/core/Version.hpp"
#include "../include/core/VersionConstraint.hpp"
#include "../include/core/Package.hpp"
#include "../include/core/Repository.hpp"
#include "../include/graph/DependencyGraph.hpp"
#include "../include/graph/TopologicalSort.hpp"
#include "../include/sat/CNFFormula.hpp"
#include "../include/sat/DPLLSolver.hpp"
#include "../include/resolver/DependencyEncoder.hpp"
#include "../include/resolver/ConflictExplainer.hpp"
#include "../include/resolver/PackageResolver.hpp"

// Utility: Print ASCII Tree of dependencies
void printAsciiTree(const std::string& node, const std::map<std::string, std::vector<std::string>>& depTree, const std::string& prefix = "", bool isLast = true, bool isRoot = true, std::set<std::string> visited = {}) {
    if (isRoot) {
        std::cout << "  " << node << "\n";
    } else {
        std::cout << "  " << prefix << (isLast ? "└── " : "├── ") << node << "\n";
    }
    visited.insert(node);

    auto it = depTree.find(node);
    if (it != depTree.end() && !it->second.empty()) {
        const auto& children = it->second;
        for (size_t i = 0; i < children.size(); i++) {
            bool childLast = (i + 1 == children.size());
            std::string nextPrefix = isRoot ? "" : (prefix + (isLast ? "    " : "│   "));
            if (visited.find(children[i]) == visited.end()) {
                printAsciiTree(children[i], depTree, nextPrefix, childLast, false, visited);
            } else {
                std::cout << "  " << nextPrefix << (childLast ? "└── " : "├── ") << children[i] << " (shared)\n";
            }
        }
    }
}

void printBanner() {
    std::cout << "========================================================================\n";
    std::cout << "  Constraint-Based Software Package Dependency Resolution Engine        \n";
    std::cout << "  MFCS Term Project | Graph Theory & SAT (DPLL/CNF) Reduction           \n";
    std::cout << "========================================================================\n\n";
}

void printHelp() {
    printBanner();
    std::cout << "Usage:\n";
    std::cout << "  pkg-resolver resolve <repo.json> <package_name> [constraint] [--json]\n";
    std::cout << "  pkg-resolver explain <repo.json> <package_name> [constraint]\n";
    std::cout << "  pkg-resolver export-dimacs <repo.json> <package_name> [constraint] <out.cnf>\n";
    std::cout << "  pkg-resolver demo\n\n";
    std::cout << "Examples:\n";
    std::cout << "  pkg-resolver resolve data/sample_repo.json webapp \"^2.0.0\"\n";
    std::cout << "  pkg-resolver resolve data/diamond_problem.json app \"*\"\n";
    std::cout << "  pkg-resolver explain data/conflict_case.json app \"*\"\n";
    std::cout << "  pkg-resolver export-dimacs data/sample_repo.json webapp \"*\" problem.cnf\n";
}

int handleResolve(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Error: Missing arguments for 'resolve'.\n";
        std::cerr << "Usage: pkg-resolver resolve <repo.json> <package_name> [constraint] [--json]\n";
        return 1;
    }

    std::string repoPath = argv[2];
    std::string targetPkg = argv[3];
    std::string constraintStr = "*";
    bool jsonOutput = false;

    for (int i = 4; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--json") {
            jsonOutput = true;
        } else {
            constraintStr = arg;
        }
    }

    Repository repo;
    try {
        repo = Repository::loadFromFile(repoPath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load repository: " << e.what() << "\n";
        return 1;
    }

    VersionConstraint constraint(constraintStr);
    ResolutionResult res = PackageResolver::resolve(repo, targetPkg, constraint);

    if (jsonOutput) {
        std::cout << res.toJSON().dump(2) << "\n";
        return res.success ? 0 : 2;
    }

    printBanner();
    std::cout << ">>> Target Request: " << targetPkg << " (" << constraint.toString() << ")\n";
    std::cout << ">>> Repository File: " << repoPath << " (" << repo.size() << " packages loaded)\n\n";

    // Step 1: Graph Theory Reachability Pruning
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "[Step 1] Graph Theory: Reachability Subgraph Reduction\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "  * Total Repository Packages : " << res.totalRepoPackages << "\n";
    std::cout << "  * Reachable Candidates      : " << res.candidatePackagesCount << "\n";
    double prunedPct = res.totalRepoPackages > 0 
        ? (100.0 * (res.totalRepoPackages - res.candidatePackagesCount) / res.totalRepoPackages) 
        : 0.0;
    std::cout << "  * Pruned Search Space       : " << std::fixed << std::setprecision(1) << prunedPct << "%\n";
    std::cout << "  * Candidate Set             : [";
    for (size_t i = 0; i < res.reachableCandidates.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << res.reachableCandidates[i].getId();
    }
    std::cout << "]\n\n";

    // Step 2: Propositional Logic CNF Encoding
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "[Step 2] Propositional Logic: CNF Reduction\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "  * Boolean Variables (N)     : " << res.numVariables << "\n";
    std::cout << "  * CNF Clauses (M)           : " << res.numClauses << "\n";
    std::cout << "  * Constraints Encoded       : Dependency implications, At-Most-One (AMO), Conflicts\n\n";

    // Step 3: DPLL SAT Solver Metrics
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "[Step 3] DPLL SAT Solving Metrics\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "  * Status                    : " << (res.success ? "SATISFIABLE (SAT)" : "UNSATISFIABLE (UNSAT)") << "\n";
    std::cout << "  * Decisions Made            : " << res.solverStats.decisions << "\n";
    std::cout << "  * Unit Propagations (BCP)   : " << res.solverStats.unitPropagations << "\n";
    std::cout << "  * Backtracks                : " << res.solverStats.backtracks << "\n";
    std::cout << "  * Solving Time              : " << res.solverStats.timeMs << " ms\n\n";

    if (!res.success) {
        std::cout << "========================================================================\n";
        std::cout << "  RESOLUTION FAILED: CONFLICT DETECTED                                  \n";
        std::cout << "========================================================================\n";
        std::cout << "Root Cause:\n  " << res.conflictInfo.rootCause << "\n\n";
        if (!res.conflictInfo.conflictChain.empty()) {
            std::cout << "Conflicting Constraints Chain:\n";
            for (const auto& line : res.conflictInfo.conflictChain) {
                std::cout << "  - " << line << "\n";
            }
            std::cout << "\n";
        }
        std::cout << "Suggested Resolution:\n  " << res.conflictInfo.suggestedFix << "\n";
        std::cout << "========================================================================\n";
        return 2;
    }

    // Step 4: Resolved Package Manifest
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "[Step 4] Resolved Package Manifest (Optimal Truth Assignment)\n";
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(25) << "  Package" << std::setw(15) << "Version" << "ID\n";
    std::cout << "  --------------------------------------------------\n";
    for (const auto& pkg : res.resolvedPackages) {
        std::cout << "  " << std::left << std::setw(23) << pkg.name 
                  << std::setw(15) << pkg.version.toString() 
                  << pkg.getId() << "\n";
    }
    std::cout << "\n";

    // Step 5: Topological Installation Schedule
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "[Step 5] Graph Theory: Topological Installation Schedule\n";
    std::cout << "------------------------------------------------------------------------\n";
    if (res.hasCycle) {
        std::cout << "  WARNING: Circular dependency detected in solution DAG!\n";
        std::cout << "  Cycle path: ";
        for (size_t i = 0; i < res.cyclePath.size(); i++) {
            if (i > 0) std::cout << " -> ";
            std::cout << res.cyclePath[i];
        }
        std::cout << "\n\n";
    } else {
        std::cout << "  Computed linear installation sequence (dependencies first):\n";
        for (size_t i = 0; i < res.installationOrder.size(); i++) {
            std::cout << "    " << (i + 1) << ". " << res.installationOrder[i].getId() << "\n";
        }
        std::cout << "\n";
    }

    // Step 6: ASCII Dependency Tree
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << "[Step 6] Dependency Hierarchy Tree\n";
    std::cout << "------------------------------------------------------------------------\n";
    // Find target package ID in resolved packages
    std::string rootTargetId = "";
    for (const auto& pkg : res.resolvedPackages) {
        if (pkg.name == targetPkg) {
            rootTargetId = pkg.getId();
            break;
        }
    }
    if (!rootTargetId.empty()) {
        printAsciiTree(rootTargetId, res.dependencyTree);
    }
    std::cout << "\n========================================================================\n";
    std::cout << "  Resolution successfully completed.\n";
    std::cout << "========================================================================\n";

    return 0;
}

int handleExplain(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: pkg-resolver explain <repo.json> <package_name> [constraint]\n";
        return 1;
    }

    std::string repoPath = argv[2];
    std::string targetPkg = argv[3];
    std::string constraintStr = (argc >= 5) ? argv[4] : "*";

    Repository repo;
    try {
        repo = Repository::loadFromFile(repoPath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load repository: " << e.what() << "\n";
        return 1;
    }

    VersionConstraint constraint(constraintStr);
    ResolutionResult res = PackageResolver::resolve(repo, targetPkg, constraint);

    printBanner();
    std::cout << ">>> Conflict Diagnosis Report for: " << targetPkg << " (" << constraint.toString() << ")\n\n";

    if (res.success) {
        std::cout << "No conflict found! The package request is SATISFIABLE.\n";
        std::cout << "Use 'pkg-resolver resolve' to view the complete solution plan.\n";
        return 0;
    }

    std::cout << "Root Cause Analysis:\n";
    std::cout << "  " << res.conflictInfo.rootCause << "\n\n";

    if (!res.conflictInfo.conflictChain.empty()) {
        std::cout << "Conflict Trace Chain:\n";
        for (const auto& line : res.conflictInfo.conflictChain) {
            std::cout << "  [X] " << line << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "Mathematical Unsatisfiable Clauses (CNF):\n";
    if (!res.conflictInfo.involvedClauses.empty()) {
        for (const auto& c : res.conflictInfo.involvedClauses) {
            std::cout << "  * " << c << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "Suggested Resolution:\n";
    std::cout << "  -> " << res.conflictInfo.suggestedFix << "\n";

    return 0;
}

int handleExportDIMACS(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: pkg-resolver export-dimacs <repo.json> <package_name> [constraint] <out.cnf>\n";
        return 1;
    }

    std::string repoPath = argv[2];
    std::string targetPkg = argv[3];
    std::string constraintStr = "*";
    std::string outputPath = "";

    if (argc == 5) {
        outputPath = argv[4];
    } else {
        constraintStr = argv[4];
        outputPath = argv[5];
    }

    Repository repo;
    try {
        repo = Repository::loadFromFile(repoPath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load repository: " << e.what() << "\n";
        return 1;
    }

    VersionConstraint constraint(constraintStr);
    auto candidates = DependencyGraph::getReachableCandidates(repo, targetPkg, constraint);
    DependencyEncoder encoder;
    encoder.encode(candidates, targetPkg, constraint);

    const auto& formula = encoder.getFormula();
    if (formula.exportDIMACS(outputPath)) {
        std::cout << "Successfully exported DIMACS CNF formula to: " << outputPath << "\n";
        std::cout << "Variables: " << formula.getNumVariables() << ", Clauses: " << formula.getNumClauses() << "\n";
        return 0;
    } else {
        std::cerr << "Error writing to " << outputPath << "\n";
        return 1;
    }
}

int handleDemo() {
    printBanner();
    std::cout << "Running Demonstration Benchmarks...\n\n";

    // Demo 1: Diamond Problem
    std::cout << "========================================================================\n";
    std::cout << "DEMO 1: Classic Diamond Dependency Resolution\n";
    std::cout << "Structure: App depends on LibA and LibB; both depend on CommonLib.\n";
    std::cout << "========================================================================\n";
    char* args1[] = { (char*)"pkg-resolver", (char*)"resolve", (char*)"data/diamond_problem.json", (char*)"app", (char*)"*" };
    handleResolve(5, args1);
    std::cout << "\n\n";

    // Demo 2: Conflict Case
    std::cout << "========================================================================\n";
    std::cout << "DEMO 2: Incompatible Version Conflict (UNSAT Core)\n";
    std::cout << "Structure: App requires AuthLib and DBLib. AuthLib needs CommonLib < 2.0,\n";
    std::cout << "           while DBLib needs CommonLib >= 2.0. Result must be UNSAT.\n";
    std::cout << "========================================================================\n";
    char* args2[] = { (char*)"pkg-resolver", (char*)"resolve", (char*)"data/conflict_case.json", (char*)"app", (char*)"*" };
    handleResolve(5, args2);
    std::cout << "\n\n";

    // Demo 3: Realistic Repository with SemVer ranges
    std::cout << "========================================================================\n";
    std::cout << "DEMO 3: Multi-Package Web Application with SemVer Constraints (^, ~)\n";
    std::cout << "Structure: Real-world microservice repo with multiple versions & choices.\n";
    std::cout << "========================================================================\n";
    char* args3[] = { (char*)"pkg-resolver", (char*)"resolve", (char*)"data/sample_repo.json", (char*)"webapp", (char*)"^2.0.0" };
    handleResolve(5, args3);

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "resolve") {
        return handleResolve(argc, argv);
    } else if (cmd == "explain") {
        return handleExplain(argc, argv);
    } else if (cmd == "export-dimacs") {
        return handleExportDIMACS(argc, argv);
    } else if (cmd == "demo") {
        return handleDemo();
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printHelp();
        return 0;
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        printHelp();
        return 1;
    }
}
