from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE

prs = Presentation()
prs.slide_width = Inches(13.333)
prs.slide_height = Inches(7.5)

BLACK = RGBColor(25, 25, 25)
GRAY = RGBColor(95, 95, 95)
ACCENT = RGBColor(37, 99, 235)
WHITE = RGBColor(255, 255, 255)

blank = prs.slide_layouts[6]


def make_slide():
    s = prs.slides.add_slide(blank)
    bg = s.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, prs.slide_width, prs.slide_height)
    bg.fill.solid()
    bg.fill.fore_color.rgb = WHITE
    bg.line.fill.background()
    return s


def add_title(s, text, subtitle=None):
    tb = s.shapes.add_textbox(Inches(1.0), Inches(0.6), Inches(11.3), Inches(0.75))
    p = tb.text_frame.paragraphs[0]
    p.text = text
    p.font.size = Pt(27)
    p.font.bold = True
    p.font.color.rgb = ACCENT

    if subtitle:
        sb = s.shapes.add_textbox(Inches(1.0), Inches(1.22), Inches(11.3), Inches(0.4))
        sp = sb.text_frame.paragraphs[0]
        sp.text = subtitle
        sp.font.size = Pt(14)
        sp.font.color.rgb = GRAY


def add_body(s, text, left=Inches(1.0), top=Inches(1.75), width=Inches(11.3), height=Inches(5.2), size=Pt(16)):
    tb = s.shapes.add_textbox(left, top, width, height)
    tf = tb.text_frame
    tf.word_wrap = True
    lines = text.strip().split("\n")
    for i, raw in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        line = raw.strip()
        if line.startswith(">"):
            p.text = line[1:].strip()
            p.font.bold = True
            p.font.size = size
            p.font.color.rgb = BLACK
            p.space_before = Pt(11)
            p.space_after = Pt(3)
        elif line == "":
            p.text = ""
            p.font.size = Pt(8)
        elif line.startswith("-"):
            p.text = "   " + line
            p.font.size = size
            p.font.color.rgb = GRAY
            p.space_after = Pt(2)
        elif line.startswith("|"):
            p.text = line[1:].strip()
            p.font.size = Pt(14)
            p.font.name = "Consolas"
            p.font.color.rgb = BLACK
            p.space_after = Pt(2)
        else:
            p.text = line
            p.font.size = size
            p.font.color.rgb = BLACK
            p.space_after = Pt(3)


# Slide 1: Title
s = make_slide()
bar = s.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.0), Inches(2.5), Inches(2.2), Inches(0.05))
bar.fill.solid()
bar.fill.fore_color.rgb = ACCENT
bar.line.fill.background()

tb = s.shapes.add_textbox(Inches(1.0), Inches(2.75), Inches(11.0), Inches(3.5))
tf = tb.text_frame
tf.word_wrap = True

p1 = tf.paragraphs[0]
p1.text = "Package Dependency Resolution\nManager"
p1.font.size = Pt(36)
p1.font.bold = True
p1.font.color.rgb = BLACK
p1.space_after = Pt(16)

p2 = tf.add_paragraph()
p2.text = "Mathematical Foundations: Graph Theory, CNF Reduction, and DPLL SAT Solving"
p2.font.size = Pt(18)
p2.font.color.rgb = GRAY
p2.space_after = Pt(28)

p3 = tf.add_paragraph()
p3.text = "MFCS Term Project  |  C++14 Engine + Web Workbench"
p3.font.size = Pt(14)
p3.font.bold = True
p3.font.color.rgb = ACCENT


# Slide 2: Why is Dependency Resolution Hard?
s = make_slide()
add_title(s, "Why Is Dependency Resolution Hard?", "Why a simple greedy approach fails")
add_body(s, """
Installing a package is not just downloading one file. Every package brings its own
constraints, version ranges, and incompatibilities.

>What makes it tricky:
- Transitive dependencies: A needs B, B needs C and D, and so on
- SemVer constraints: packages specify ranges like ^2.0.0, ~1.5.0, or >=1.2.0, <2.0.0
- At-Most-One rule: only one version of a given package can be installed at a time
- Diamond dependencies: A depends on B and C, and both B and C depend on D

>Computational Complexity:
- Deciding if a valid installation set exists is NP-complete (reducible from 3-SAT)
- With 50 packages and 10 versions each, brute force would check 10^50 combinations
- Greedy picking ("always grab the latest") often hits dead ends and misses valid solutions
- Real package managers (apt/libsolv, cargo, opam) use SAT solvers internally
""")


# Slide 3: End-to-End Pipeline
s = make_slide()
add_title(s, "End-to-End Resolution Pipeline", "How our engine goes from a repository to an install plan")
add_body(s, """
>1. Graph Pruning (BFS Reachability)
- Traverse from the target package to collect only reachable candidates
- Throw away unrelated packages before building any logic formula

>2. Propositional Logic (CNF Reduction)
- Create a boolean variable X(P, v) for each candidate package-version
- Encode target goal, dependencies, mutual exclusions, and conflicts as CNF clauses

>3. SAT Solving (DPLL + Max-SAT Preference)
- Search for a truth assignment that satisfies every clause simultaneously
- Iteratively prefer newer semantic versions when multiple solutions exist

>4. Post-Resolution (Topological Sort or Conflict Diagnosis)
- If SAT: build the dependency DAG, check for cycles, output install order (Kahn's algorithm)
- If UNSAT: trace the colliding constraints and report the root cause
""", size=Pt(15))


# Slide 4: Concept 1 - Graph Pruning (BFS)
s = make_slide()
add_title(s, "Concept 1: Graph Pruning via BFS", "Shrinking the search space in O(V + E) time")
add_body(s, """
Repositories often hold hundreds or thousands of packages, most of which have nothing
to do with what the user is installing.

>How Reachability Pruning works:
- Treat the repository as a directed graph of package-version nodes
- Start a Breadth-First Search (BFS) queue with versions matching the target query
- For each popped package, look up all versions satisfying its dependencies and enqueue them
- Any package never visited by BFS is unreachable and excluded from the SAT formula

>Impact in our WebApp benchmark (18 packages):
- Target query: webapp (^2.0.0)
- Reachable candidates: 10 packages
- Pruned before SAT: 8 packages (44.4% reduction in variables and clauses)
""")


# Slide 5: Concept 2 - Propositional Logic & CNF Encoding
s = make_slide()
add_title(s, "Concept 2: Propositional Logic & CNF Encoding", "Reducing package rules into Boolean clauses")
add_body(s, """
Each candidate package-version (P, v) is represented by a boolean variable:
|   X(P, v) = TRUE   iff   package P at version v is installed

We encode four rule types into Conjunctive Normal Form (AND of ORs):

>1. Target Goal (at least one matching version of target must be installed)
|   ( X(webapp, 2.0)  v  X(webapp, 2.1) )

>2. Dependency Implication:  X(A) => (X(B1) v X(B2))  becomes:
|   ( ~X(webapp, 2.1)  v  X(pg-driver, 8.0)  v  X(pg-driver, 8.7.1) )

>3. At-Most-One Version (pairwise mutual exclusion for same package)
|   ( ~X(common, 1.2.0)  v  ~X(common, 1.3.0) )

>4. Explicit Conflict:  ~(X(A) ^ X(B))  becomes:
|   ( ~X(jwt-auth, 2.0.0)  v  ~X(legacy-security, 1.0.0) )
""", size=Pt(15))


# Slide 6: Concept 3 - DPLL SAT Solver
s = make_slide()
add_title(s, "Concept 3: Solving with the DPLL Algorithm", "Finding valid assignments without brute force")
add_body(s, """
Our custom C++ DPLL solver searches for a model that satisfies every CNF clause:

>1. Unit Propagation (Boolean Constraint Propagation)
- If a clause has one unassigned literal left and all others are FALSE, force it TRUE
- Example: once X(webapp, 2.1) is TRUE, (~X(webapp, 2.1) v X(express, 4.18)) forces express TRUE
- If a clause becomes completely FALSE, detect conflict and backtrack immediately

>2. Pure Literal Elimination (Negative)
- If a variable only appears negated in remaining clauses, safely set it FALSE
- Note: positive pure literals are NOT forced so unused leaf packages stay uninstalled

>3. Branching Heuristic (DLIS) & Backtracking
- Dynamic Largest Individual Sum picks the unassigned literal appearing most in active clauses
- Tries TRUE, backtracks to FALSE on contradiction

>4. Latest-Version Preference (Assumption Solving)
- Iteratively tests solve({X_latest = TRUE}) so newer compatible versions win (e.g. common@1.3.0)
""", size=Pt(14.5))


# Slide 7: Concept 4 - Topological Sort & Cycle Detection
s = make_slide()
add_title(s, "Concept 4: Topological Sort & Cycle Detection", "Turning a package set into a safe install sequence")
add_body(s, """
Knowing which packages to install is only half the job — dependencies must be installed
before the packages that depend on them.

>Topological Sort (Kahn's Algorithm):
- Build a directed graph from the SAT solution: edge (dependency -> dependent)
- Compute in-degree (number of unmet dependencies) for each selected package
- Repeatedly pop nodes with in-degree 0, append to install order, and decrement neighbors

>Sample Output (WebApp):
|   1. crypto-core@2.1.0  ->  2. router@1.3.5  ->  3. sql-parser@2.1.4
|   4. jwt-auth@2.0.0     ->  5. express@4.18.2 ->  6. pg-driver@8.7.1 ->  7. webapp@2.1.0

>Cycle Detection (3-Color DFS):
- If Kahn's algorithm finishes early, a circular dependency exists
- DFS (0=unvisited, 1=visiting, 2=visited) finds back-edges and prints the cycle path
|   Example: serviceA@1.0.0 -> serviceB@1.0.0 -> serviceC@1.0.0 -> serviceA@1.0.0
""", size=Pt(15))


# Slide 8: Concept 5 - UNSAT Conflict Explanation
s = make_slide()
add_title(s, "Concept 5: Conflict Explanation (UNSAT)", "Explaining why an installation is impossible")
add_body(s, """
When no assignment satisfies the CNF formula, returning just "UNSAT" is not helpful.
Our ConflictExplainer analyzes the candidate graph and clause origins to pinpoint why:

>Checks performed on UNSAT:
- Missing target package or no versions matching the requested constraint
- Direct dependency with zero matching versions in the repository
- Shared dependency collision across distinct packages (Diamond conflict)
- Explicit conflict rules blocking required packages

>Example (conflict_case.json):
- auth@1.0.0 requires crypto < 2.0.0     (matches: 1.0.0, 1.5.0)
- db@1.0.0   requires crypto >= 2.0.0    (matches: 2.0.0, 2.1.0)
- Intersection = Empty Set  ->  UNSATISFIABLE
- Tool outputs the colliding constraint chain and suggests aligning version ranges
""")


# Slide 9: Architecture & Tech Stack
s = make_slide()
add_title(s, "System Architecture & Tech Stack", "C++14 core engine bridged to a live web workbench")
add_body(s, """
>1. Core Engine — C++14 (Zero External Dependencies)
- core/     SemVer parser, range matcher (^, ~, >=, <), repository registry, JSON parser
- graph/    BFS reachability pruning, Kahn's topological sort, 3-color DFS cycle detection
- sat/      Literal, Clause, CNF formula, DIMACS .cnf exporter, DPLL solver (BCP + DLIS)
- resolver/ CNF encoder, Max-SAT version upgrader, conflict explainer, CLI (pkg-resolver.exe)

>2. API Bridge — Python FastAPI + Uvicorn (server.py)
- REST endpoints (/api/repos, /api/resolve) invoking pkg-resolver.exe --json

>3. Interactive Web Frontend (HTML / CSS / JS + SVG)
- Scenario preset selector, target query bar, and live JSON/visual repository editor
- Interactive SVG dependency graph with Resolved DAG view and BFS Pruning view
- Inspectable Install Order, Boolean Variables, CNF Clauses, DIMACS download, and Solver Stats
""", size=Pt(14.5))


# Slide 10: Benchmark Results
s = make_slide()
add_title(s, "Benchmark Results", "Verified across 7 automated unit & integration tests")

add_body(s, """
>1. Diamond Problem (SAT)
app -> libA, libB -> common
Repo: 9 pkgs | Pruned: 22.2%
Variables: 7 | Clauses: 11
Selected: common@1.3.0 (latest valid)
""", left=Inches(0.9), top=Inches(1.7), width=Inches(2.8), height=Inches(4.8), size=Pt(14))

add_body(s, """
>2. Version Collision (UNSAT)
auth (crypto <2.0) vs db (>=2.0)
Repo: 7 pkgs | Variables: 7
Decisions: 1 | Backtracks: 1
Status: UNSATISFIABLE
Root cause: crypto collision
""", left=Inches(3.9), top=Inches(1.7), width=Inches(2.8), height=Inches(4.8), size=Pt(14))

add_body(s, """
>3. WebApp Ecosystem (SAT)
18-pkg repo with ^ and ~ rules
Pruned: 44.4% (18 -> 10 cands)
Variables: 10 | Clauses: 14
Selected: 7 pkgs (pg-driver@8.7.1)
Time: < 1 ms, valid DAG order
""", left=Inches(6.9), top=Inches(1.7), width=Inches(2.8), height=Inches(4.8), size=Pt(14))

add_body(s, """
>4. Circular Dependency
serviceA -> B -> C -> A
SAT Status: SATISFIABLE
Topological Sort: Blocked
3-color DFS detected cycle:
serviceA -> B -> C -> A
""", left=Inches(9.9), top=Inches(1.7), width=Inches(2.7), height=Inches(4.8), size=Pt(14))


# Slide 11: Thank You
s = make_slide()
bar = s.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(1.0), Inches(3.0), Inches(2.0), Inches(0.04))
bar.fill.solid()
bar.fill.fore_color.rgb = ACCENT
bar.line.fill.background()

tb = s.shapes.add_textbox(Inches(1.0), Inches(3.2), Inches(11.0), Inches(2.5))
tf = tb.text_frame
tf.word_wrap = True
p = tf.paragraphs[0]
p.text = "Thank You"
p.font.size = Pt(36)
p.font.bold = True
p.font.color.rgb = BLACK
p.space_after = Pt(14)

p2 = tf.add_paragraph()
p2.text = "Questions?"
p2.font.size = Pt(20)
p2.font.color.rgb = GRAY

prs.save("Package_Dependency_Resolution_MFCS.pptx")
print("Updated Package_Dependency_Resolution_MFCS.pptx successfully.")
