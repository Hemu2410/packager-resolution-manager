import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any, Dict, Optional

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

BASE_DIR = Path(__file__).resolve().parent
DATA_DIR = BASE_DIR / "data"
FRONTEND_DIR = BASE_DIR / "frontend"
RESOLVER_EXE = BASE_DIR / "pkg-resolver.exe"

PRESET_METADATA = {
    "sample_repo.json": {
        "label": "Multi-Package WebApp (SemVer)",
        "description": "18-package microservice ecosystem with ^, ~ constraints and unused branches.",
        "defaultTarget": "webapp",
        "defaultConstraint": "^2.0.0",
    },
    "diamond_problem.json": {
        "label": "Diamond Dependency (SAT)",
        "description": "Classic diamond: app -> libA, libB -> common with overlapping version ranges.",
        "defaultTarget": "app",
        "defaultConstraint": "*",
    },
    "conflict_case.json": {
        "label": "Version Collision (UNSAT)",
        "description": "Incompatible diamond: auth requires crypto <2.0 while db requires crypto >=2.0.",
        "defaultTarget": "app",
        "defaultConstraint": "*",
    },
    "cyclic_dependency.json": {
        "label": "Circular Dependency (Cycle)",
        "description": "3-node cycle (serviceA -> serviceB -> serviceC -> serviceA) detected by DFS.",
        "defaultTarget": "serviceA",
        "defaultConstraint": "*",
    },
}


class ResolveRequest(BaseModel):
    repoName: Optional[str] = None
    repoData: Optional[Dict[str, Any]] = None
    targetPackage: str
    constraint: str = "*"


app = FastAPI(
    title="Constraint-Based Package Dependency Resolver API",
    description="FastAPI bridge between Web UI and C++14 SAT/Graph Engine",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/api/health")
def health_check():
    return {
        "status": "ok",
        "resolverExists": RESOLVER_EXE.exists(),
    }


@app.get("/api/repos")
def list_repos():
    if not DATA_DIR.exists():
        return {"repos": []}

    repos = []
    # Order presets nicely first, then any extra JSON files
    files = sorted(DATA_DIR.glob("*.json"), key=lambda p: (p.name not in PRESET_METADATA, p.name))
    for path in files:
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            pkgs = data.get("packages", data if isinstance(data, list) else [])
        except Exception:
            pkgs = []

        meta = PRESET_METADATA.get(path.name, {})
        pkg_names = sorted({p.get("name", "") for p in pkgs if isinstance(p, dict) and p.get("name")})
        repos.append({
            "filename": path.name,
            "label": meta.get("label", path.stem.replace("_", " ").title()),
            "description": meta.get("description", "Custom JSON package repository."),
            "packageCount": len(pkgs),
            "uniquePackageNames": pkg_names,
            "defaultTarget": meta.get("defaultTarget", pkg_names[0] if pkg_names else ""),
            "defaultConstraint": meta.get("defaultConstraint", "*"),
        })

    return {"repos": repos}


@app.get("/api/repos/{repo_name}")
def get_repo(repo_name: str):
    safe_name = os.path.basename(repo_name)
    target_path = DATA_DIR / safe_name
    if not target_path.exists() or not target_path.is_file():
        raise HTTPException(status_code=404, detail=f"Repository '{safe_name}' not found")
    with open(target_path, "r", encoding="utf-8") as f:
        return json.load(f)


@app.post("/api/resolve")
def resolve_dependencies(req: ResolveRequest):
    if not RESOLVER_EXE.exists():
        raise HTTPException(
            status_code=500,
            detail="pkg-resolver.exe not found. Please compile the C++ backend using build.ps1.",
        )

    target_pkg = req.targetPackage.strip()
    if not target_pkg:
        raise HTTPException(status_code=400, detail="targetPackage cannot be empty.")

    constraint = req.constraint.strip() or "*"
    temp_file_path: Optional[str] = None

    try:
        if req.repoData is not None:
            fd, temp_file_path = tempfile.mkstemp(suffix=".json", prefix="repo_")
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                json.dump(req.repoData, f, indent=2)
            repo_path = temp_file_path
            repo_packages = req.repoData.get("packages", [])
        elif req.repoName:
            safe_name = os.path.basename(req.repoName)
            candidate = DATA_DIR / safe_name
            if not candidate.exists():
                raise HTTPException(status_code=404, detail=f"Repository '{safe_name}' not found.")
            repo_path = str(candidate)
            with open(candidate, "r", encoding="utf-8") as f:
                loaded = json.load(f)
                repo_packages = loaded.get("packages", loaded if isinstance(loaded, list) else [])
        else:
            raise HTTPException(status_code=400, detail="Provide either repoName or repoData.")

        proc = subprocess.run(
            [str(RESOLVER_EXE), "resolve", repo_path, target_pkg, constraint, "--json"],
            cwd=str(BASE_DIR),
            capture_output=True,
            text=True,
            timeout=15,
        )

        # Exit code 0 = SAT, 2 = UNSAT, 1 = CLI/parse error
        if proc.returncode not in (0, 2):
            err_msg = proc.stderr.strip() or proc.stdout.strip() or "Unknown resolver error"
            raise HTTPException(status_code=400, detail=err_msg)

        try:
            result = json.loads(proc.stdout)
        except json.JSONDecodeError as exc:
            raise HTTPException(
                status_code=500,
                detail=f"Invalid JSON from C++ resolver: {exc}. Output: {proc.stdout[:300]}",
            )

        result["repoPackages"] = repo_packages
        return result

    finally:
        if temp_file_path and os.path.exists(temp_file_path):
            try:
                os.remove(temp_file_path)
            except OSError:
                pass


if FRONTEND_DIR.exists():
    app.mount("/static", StaticFiles(directory=str(FRONTEND_DIR)), name="static")


@app.get("/")
def serve_index():
    index_path = FRONTEND_DIR / "index.html"
    if not index_path.exists():
        raise HTTPException(status_code=404, detail="Frontend index.html not found.")
    return FileResponse(str(index_path))


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("server:app", host="127.0.0.1", port=8000, reload=False)
