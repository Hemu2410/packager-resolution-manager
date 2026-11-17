# Constraint-Based Software Package Dependency Resolution

> **MFCS (Mathematical Foundations of Computer Science) Term Project**  
> A full-stack dependency resolution engine combining **C++14** (Graph Theory + DPLL SAT Solver), **FastAPI**, and an **Interactive Web Workbench**.

---

## Overview

Modern package managers (`apt`/`libsolv`, `cargo`, `opam`, `pub`) solve a **NP-complete** combinatorial problem every time you install a package: finding a set of compatible package versions that satisfy all transitive dependencies, semantic version ranges, and mutual exclusion rules.

This project implements the entire resolution pipeline from scratch with **zero external C++ dependencies**:

1. **Graph Pruning (BFS):** Extracts only packages transitively reachable from the install target in $O(V + E)$ time, pruning unrelated packages before any SAT formula is built.
2. **Propositional Logic (CNF Reduction):** Encodes install goals, dependency implications, At-Most-One version exclusions, and explicit conflicts into Conjunctive Normal Form.
3. **Custom DPLL SAT Solver + Version Preference:** Solves the CNF formula using Unit Propagation (BCP), negative Pure Literal Elimination, DLIS branching, and iterative assumption-based SemVer upgrades.
4. **Topological Sort & Cycle Detection:** Uses Kahn's algorithm to produce a valid installation order (dependencies first) and 3-color DFS to detect and trace circular dependencies.
5. **Conflict Diagnosis (UNSAT):** Explains *why* an installation failed (e.g., diamond version collisions or missing packages) and suggests fixes.

---

## Project Structure

```
packagedependencyresolution/
├── include/
│   ├── core/
│   │   ├── Version.hpp            # SemVer parser & comparator (Major.Minor.Patch-pre)
│   │   ├── VersionConstraint.hpp  # Range matching (=, !=, >, >=, <, <=, ^, ~, *)
│   │   ├── Package.hpp            # Package, Dependency, and Conflict models
│   │   ├── Repository.hpp         # Package registry & JSON loader
│   │   └── SimpleJSON.hpp         # Zero-dependency JSON parser & serializer
│   ├── graph/
│   │   ├── DependencyGraph.hpp    # BFS reachability subgraph pruning
│   │   └── TopologicalSort.hpp    # Kahn's algorithm & 3-color DFS cycle detection
│   ├── sat/
│   │   ├── Literal.hpp            # Signed Boolean literals (+X, -X)
│   │   ├── Clause.hpp             # Disjunctive clauses & ternary evaluation
│   │   ├── CNFFormula.hpp         # Variable mapping & DIMACS .cnf export/import
│   │   └── DPLLSolver.hpp         # DPLL solver (BCP, pure negative literals, DLIS)
│   └── resolver/
│       ├── DependencyEncoder.hpp  # Translates package constraints to CNF clauses
│       ├── ConflictExplainer.hpp  # UNSAT root-cause & collision diagnostics
│       └── PackageResolver.hpp    # Master pipeline, SemVer upgrade pass & JSON output
├── src/
│   └── main.cpp                   # CLI tool (resolve, explain, export-dimacs, demo)
├── tests/
│   └── test_resolver.cpp          # 7 unit & integration tests
├── data/
│   ├── sample_repo.json           # 18-package microservice ecosystem (^, ~ SemVer)
│   ├── diamond_problem.json       # Classic diamond dependency test
│   ├── conflict_case.json         # Incompatible version collision (UNSAT)
│   └── cyclic_dependency.json     # 3-node circular dependency graph
├── frontend/
│   ├── index.html                 # Interactive Web Workbench UI
│   ├── styles.css                 # Clean responsive styling
│   └── app.js                     # SVG DAG/Pruning graph, CNF inspector & repo editor
├── server.py                      # FastAPI + Uvicorn bridge to pkg-resolver.exe
├── build.ps1                      # Windows PowerShell build & test script
├── Makefile                       # Linux/macOS/MinGW Makefile
└── generate_ppt.py                # Generates Package_Dependency_Resolution_MFCS.pptx
```

---

## Quickstart

### 1. Build the C++ Engine & Run Tests

**Windows (PowerShell + MinGW `g++`):**
```powershell
.\build.ps1
```

**Linux / macOS / Make:**
```bash
make
./test_suite
```

This compiles `pkg-resolver.exe` (CLI) and `test_suite.exe`, then executes all 7 unit and integration tests.

### 2. Launch the Web Frontend

Requires Python 3 with `fastapi` and `uvicorn` installed:
```powershell
python server.py
```
Open **http://127.0.0.1:8000** in your browser.

---

## Web Workbench Features

* **Scenario Presets & Target Query:** Switch between *Multi-Package WebApp*, *Diamond Dependency (SAT)*, *Version Collision (UNSAT)*, and *Circular Dependency (Cycle)*, or specify custom SemVer constraints (`*`, `^2.0.0`, `>=1.2.0, <2.0.0`).
* **Interactive SVG Dependency Graph:**
  * **Resolved Graph:** Displays the selected package DAG with directed edges, numbered topological install badges, and highlighted cycle paths.
  * **BFS Pruning View:** Displays the entire repository, highlighting reachable candidates vs. packages pruned by BFS.
  * Click any node to inspect its dependencies and conflicts; pan/zoom with mouse drag and scroll.
* **3-Tab Inspector:**
  * **Install Order & Packages:** Step-by-step installation timeline and resolved manifest.
  * **SAT Formula (CNF):** Boolean variable assignments ($X_1 \dots X_N$), human-readable CNF clauses, and **Download DIMACS `.cnf`**.
  * **Solver Details:** DPLL decisions, unit propagations (BCP), backtracks, pruning %, and solve time.
* **Live Repository Editor:** Click **Edit Repository** to add/remove package versions visually or edit the raw JSON and re-solve immediately.

---

## CLI Usage

```powershell
# Run all built-in demonstration scenarios
.\pkg-resolver.exe demo

# Resolve a package (human-readable output + ASCII dependency tree)
.\pkg-resolver.exe resolve data/sample_repo.json webapp "^2.0.0"

# Resolve with machine-readable JSON output
.\pkg-resolver.exe resolve data/sample_repo.json webapp "^2.0.0" --json

# Diagnose why an installation fails (UNSAT)
.\pkg-resolver.exe explain data/conflict_case.json app any

# Export constraints to standard DIMACS CNF format (for MiniSat / Z3)
.\pkg-resolver.exe export-dimacs data/sample_repo.json webapp "^2.0.0" problem.cnf
```

---

## API Endpoints (`server.py`)

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/` | Serves the web frontend (`frontend/index.html`) |
| `GET` | `/api/health` | Checks server status and `pkg-resolver.exe` availability |
| `GET` | `/api/repos` | Lists all repository presets in `data/` |
| `GET` | `/api/repos/{name}` | Returns the raw JSON of a preset repository |
| `POST` | `/api/resolve` | Runs `pkg-resolver.exe --json` with `{ repoName \| repoData, targetPackage, constraint }` |

---

## Repository JSON Format

```json
{
  "packages": [
    {
      "name": "webapp",
      "version": "2.1.0",
      "dependencies": {
        "express": "^4.18.0",
        "jwt-auth": "^2.0.0"
      }
    },
    {
      "name": "jwt-auth",
      "version": "2.0.0",
      "dependencies": {
        "crypto-core": ">=2.0.0"
      },
      "conflicts": {
        "legacy-security": "*"
      }
    }
  ]
}
```

---

## Mathematical Formulation

For each reachable package-version $(P, v)$, we define a Boolean variable $X_{P, v} \in \{0, 1\}$:

1. **Target Goal:** $\bigvee_{v \in \text{Sat}(T, \mathcal{C}_T)} X_{T, v}$
2. **Dependency Implication:** $X_{P, u} \implies \bigvee_{w \in \text{Sat}(Q, \mathcal{C})} X_{Q, w} \;\equiv\; \left(\neg X_{P, u} \lor \bigvee_{w} X_{Q, w}\right)$
3. **At-Most-One Version:** $\forall u \ne v: \; (\neg X_{P, u} \lor \neg X_{P, v})$
4. **Explicit Conflict:** $\neg(X_{P, u} \land X_{R, w}) \;\equiv\; (\neg X_{P, u} \lor \neg X_{R, w})$
