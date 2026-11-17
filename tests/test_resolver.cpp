#include <iostream>
#include <cassert>
#include <vector>

#include "../include/core/Version.hpp"
#include "../include/core/VersionConstraint.hpp"
#include "../include/core/Package.hpp"
#include "../include/core/Repository.hpp"
#include "../include/sat/Literal.hpp"
#include "../include/sat/Clause.hpp"
#include "../include/sat/CNFFormula.hpp"
#include "../include/sat/DPLLSolver.hpp"
#include "../include/graph/DependencyGraph.hpp"
#include "../include/graph/TopologicalSort.hpp"
#include "../include/resolver/PackageResolver.hpp"

void testVersion() {
    std::cout << "[Test] Running Version tests...\n";
    Version v1 = Version::parse("1.2.3");
    assert(v1.major == 1 && v1.minor == 2 && v1.patch == 3);

    Version v2 = Version::parse("2.0.0");
    assert(v1 < v2);
    assert(v2 > v1);
    assert(v1 <= v2);
    assert(v1 != v2);

    Version v3 = Version::parse("1.2.3");
    assert(v1 == v3);

    Version v_pre = Version::parse("1.2.3-beta.1");
    assert(v_pre < v1); // Pre-release has lower precedence
    std::cout << "  -> Version tests passed!\n";
}

void testConstraint() {
    std::cout << "[Test] Running VersionConstraint tests...\n";
    Version v = Version::parse("1.5.2");

    assert(VersionConstraint("*").matches(v));
    assert(VersionConstraint(">=1.0.0").matches(v));
    assert(VersionConstraint("<=1.5.2").matches(v));
    assert(!VersionConstraint("<1.5.2").matches(v));
    assert(VersionConstraint(">1.5.0").matches(v));
    assert(!VersionConstraint(">2.0.0").matches(v));

    // Caret (^): ^1.2.0 allows >=1.2.0 and <2.0.0
    assert(VersionConstraint("^1.2.0").matches(v));
    assert(!VersionConstraint("^2.0.0").matches(v));

    // Tilde (~): ~1.5.0 allows >=1.5.0 and <1.6.0
    assert(VersionConstraint("~1.5.0").matches(v));
    assert(!VersionConstraint("~1.4.0").matches(v));

    // Compound
    assert(VersionConstraint(">=1.0.0, <2.0.0").matches(v));
    assert(!VersionConstraint(">=1.6.0, <2.0.0").matches(v));

    std::cout << "  -> VersionConstraint tests passed!\n";
}

void testSAT() {
    std::cout << "[Test] Running DPLL SAT Solver tests...\n";

    // Test SAT: (X1 v X2) ^ (~X1 v X2) ^ (X1 v ~X2)
    // Solution: X1=true, X2=true
    CNFFormula f1;
    f1.getOrCreateVariable("X1");
    f1.getOrCreateVariable("X2");
    f1.addClause({Literal(1, true), Literal(2, true)});
    f1.addClause({Literal(1, false), Literal(2, true)});
    f1.addClause({Literal(1, true), Literal(2, false)});

    DPLLSolver solver1(f1);
    assert(solver1.solve() == SolverResult::SAT);
    assert(solver1.isVariableTrue(1) && solver1.isVariableTrue(2));

    // Test UNSAT: (X1) ^ (~X1)
    CNFFormula f2;
    f2.getOrCreateVariable("X1");
    f2.addClause({Literal(1, true)});
    f2.addClause({Literal(1, false)});

    DPLLSolver solver2(f2);
    assert(solver2.solve() == SolverResult::UNSAT);

    std::cout << "  -> SAT Solver tests passed!\n";
}

void testDiamondResolution() {
    std::cout << "[Test] Running Diamond Dependency Resolution test...\n";
    Repository repo = Repository::loadFromFile("data/diamond_problem.json");

    ResolutionResult res = PackageResolver::resolve(repo, "app", VersionConstraint("*"));
    assert(res.success == true);

    // Verify app, libA, libB, common are installed
    bool foundCommon = false;
    std::string commonVer = "";
    for (const auto& pkg : res.resolvedPackages) {
        if (pkg.name == "common") {
            foundCommon = true;
            commonVer = pkg.version.toString();
        }
    }
    assert(foundCommon);
    // common must be >= 1.2.0 and < 2.0.0. With Max-SAT SemVer preference, 1.3.0 must be selected.
    assert(commonVer == "1.3.0");

    // Verify unrelated-pkg is pruned
    for (const auto& pkg : res.resolvedPackages) {
        assert(pkg.name != "unrelated-pkg");
    }

    // Verify topological order: common must be installed BEFORE libA and libB
    size_t commonIdx = 0, libAIdx = 0, libBIdx = 0, appIdx = 0;
    for (size_t i = 0; i < res.installationOrder.size(); i++) {
        if (res.installationOrder[i].name == "common") commonIdx = i;
        if (res.installationOrder[i].name == "libA") libAIdx = i;
        if (res.installationOrder[i].name == "libB") libBIdx = i;
        if (res.installationOrder[i].name == "app") appIdx = i;
    }
    assert(commonIdx < libAIdx);
    assert(commonIdx < libBIdx);
    assert(libAIdx < appIdx);
    assert(libBIdx < appIdx);

    std::cout << "  -> Diamond Dependency test passed! Chosen common: " << commonVer << "\n";
}

void testConflictResolution() {
    std::cout << "[Test] Running Incompatible Conflict test...\n";
    Repository repo = Repository::loadFromFile("data/conflict_case.json");

    ResolutionResult res = PackageResolver::resolve(repo, "app", VersionConstraint("*"));
    assert(res.success == false);
    assert(res.conflictInfo.hasConflict == true);
    assert(!res.conflictInfo.rootCause.empty());

    std::cout << "  -> Conflict test passed! Detected root cause: " << res.conflictInfo.rootCause << "\n";
}

void testCycleDetection() {
    std::cout << "[Test] Running Cycle Detection test...\n";
    Repository repo = Repository::loadFromFile("data/cyclic_dependency.json");

    ResolutionResult res = PackageResolver::resolve(repo, "serviceA", VersionConstraint("*"));
    assert(res.success == true);
    assert(res.hasCycle == true);
    assert(!res.cyclePath.empty());

    std::cout << "  -> Cycle Detection test passed! Cycle size: " << res.cyclePath.size() << "\n";
}

void testSampleRepoLatestSelection() {
    std::cout << "[Test] Running Sample Repo Max-SAT Version Preference test...\n";
    Repository repo = Repository::loadFromFile("data/sample_repo.json");

    ResolutionResult res = PackageResolver::resolve(repo, "webapp", VersionConstraint("^2.0.0"));
    assert(res.success == true);
    assert(res.hasCycle == false);

    bool foundPg871 = false;
    for (const auto& p : res.resolvedPackages) {
        if (p.name == "pg-driver") {
            assert(p.version.toString() == "8.7.1");
            foundPg871 = true;
        }
    }
    assert(foundPg871);
    std::cout << "  -> Sample Repo test passed! Selected latest pg-driver@8.7.1.\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "  Running Automated Test Suite for Package Resolver     \n";
    std::cout << "========================================================\n\n";

    testVersion();
    testConstraint();
    testSAT();
    testDiamondResolution();
    testConflictResolution();
    testCycleDetection();
    testSampleRepoLatestSelection();

    std::cout << "\n>>> ALL UNIT & INTEGRATION TESTS PASSED SUCCESSFULLY! <<<\n";
    return 0;
}
