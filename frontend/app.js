const state = {
  presets: [],
  currentPreset: null,
  selectedTarget: "",
  repoData: { packages: [] },
  lastResult: null,
  graphMode: "resolved",
  pan: { x: 0, y: 0, scale: 1 },
  dragging: false,
  dragStart: { x: 0, y: 0 },
};

const els = {
  presetDropdown: document.getElementById("presetDropdown"),
  presetTrigger: document.getElementById("presetTrigger"),
  presetMenu: document.getElementById("presetMenu"),
  targetDropdown: document.getElementById("targetDropdown"),
  targetTrigger: document.getElementById("targetTrigger"),
  targetMenu: document.getElementById("targetMenu"),
  constraintInput: document.getElementById("constraintInput"),
  resolveBtn: document.getElementById("resolveBtn"),
  repoCount: document.getElementById("repoCount"),
  openRepoBtn: document.getElementById("openRepoBtn"),
  repoModal: document.getElementById("repoModal"),
  closeRepoBtn: document.getElementById("closeRepoBtn"),
  toggleRawJsonBtn: document.getElementById("toggleRawJsonBtn"),
  resetRepoBtn: document.getElementById("resetRepoBtn"),
  repoVisualEditor: document.getElementById("repoVisualEditor"),
  repoRawEditor: document.getElementById("repoRawEditor"),
  rawJsonTextarea: document.getElementById("rawJsonTextarea"),
  applyRawJsonBtn: document.getElementById("applyRawJsonBtn"),
  newPkgName: document.getElementById("newPkgName"),
  newPkgVer: document.getElementById("newPkgVer"),
  newPkgDeps: document.getElementById("newPkgDeps"),
  newPkgConfs: document.getElementById("newPkgConfs"),
  addPkgSubmitBtn: document.getElementById("addPkgSubmitBtn"),
  repoList: document.getElementById("repoList"),
  statusVal: document.getElementById("statusVal"),
  installedVal: document.getElementById("installedVal"),
  prunedVal: document.getElementById("prunedVal"),
  cnfVal: document.getElementById("cnfVal"),
  timeVal: document.getElementById("timeVal"),
  alertBanner: document.getElementById("alertBanner"),
  modeResolvedBtn: document.getElementById("modeResolvedBtn"),
  modePruningBtn: document.getElementById("modePruningBtn"),
  zoomInBtn: document.getElementById("zoomInBtn"),
  zoomOutBtn: document.getElementById("zoomOutBtn"),
  zoomFitBtn: document.getElementById("zoomFitBtn"),
  svgContainer: document.getElementById("svgContainer"),
  graphViewport: document.getElementById("graphViewport"),
  nodePopover: document.getElementById("nodePopover"),
  popTitle: document.getElementById("popTitle"),
  popBody: document.getElementById("popBody"),
  popClose: document.getElementById("popClose"),
  tabPlan: document.getElementById("tab-plan"),
  tabCnf: document.getElementById("tab-cnf"),
  tabMetrics: document.getElementById("tab-metrics"),
  countPlan: document.getElementById("countPlan"),
  countCnf: document.getElementById("countCnf"),
};

async function init() {
  bindEvents();
  const res = await fetch("/api/repos");
  const data = await res.json();
  state.presets = data.repos || [];
  renderPresetMenu();

  if (state.presets.length > 0) {
    await loadPreset(state.presets[0].filename);
  }
}

function bindEvents() {
  // Custom dropdown triggers
  els.presetTrigger.addEventListener("click", (e) => {
    e.stopPropagation();
    toggleDropdown(els.presetDropdown, els.presetMenu, els.targetDropdown, els.targetMenu);
  });
  els.targetTrigger.addEventListener("click", (e) => {
    e.stopPropagation();
    toggleDropdown(els.targetDropdown, els.targetMenu, els.presetDropdown, els.presetMenu);
  });
  document.addEventListener("click", closeAllDropdowns);

  els.resolveBtn.addEventListener("click", () => runResolve());
  els.constraintInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") runResolve();
  });

  els.modeResolvedBtn.addEventListener("click", () => setGraphMode("resolved"));
  els.modePruningBtn.addEventListener("click", () => setGraphMode("pruning"));
  els.zoomInBtn.addEventListener("click", () => adjustZoom(1.2));
  els.zoomOutBtn.addEventListener("click", () => adjustZoom(0.83));
  els.zoomFitBtn.addEventListener("click", () => fitGraph());
  els.popClose.addEventListener("click", () => els.nodePopover.classList.add("hidden"));

  // SVG Pan/Zoom
  els.svgContainer.addEventListener("mousedown", (e) => {
    if (e.target.closest(".svg-node") || e.target.closest(".popover")) return;
    state.dragging = true;
    state.dragStart = { x: e.clientX - state.pan.x, y: e.clientY - state.pan.y };
  });
  window.addEventListener("mousemove", (e) => {
    if (!state.dragging) return;
    state.pan.x = e.clientX - state.dragStart.x;
    state.pan.y = e.clientY - state.dragStart.y;
    applyTransform();
  });
  window.addEventListener("mouseup", () => (state.dragging = false));
  els.svgContainer.addEventListener("wheel", (e) => {
    e.preventDefault();
    adjustZoom(e.deltaY < 0 ? 1.1 : 0.9);
  }, { passive: false });

  // Tabs
  document.querySelectorAll(".tab").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach((b) => b.classList.toggle("active", b === btn));
      document.querySelectorAll(".tab-pane").forEach((p) => p.classList.toggle("active", p.id === `tab-${btn.dataset.tab}`));
    });
  });

  // Repository Modal
  els.openRepoBtn.addEventListener("click", () => {
    renderRepoList();
    els.repoModal.classList.remove("hidden");
  });
  els.closeRepoBtn.addEventListener("click", () => els.repoModal.classList.add("hidden"));
  els.repoModal.addEventListener("click", (e) => {
    if (e.target === els.repoModal) els.repoModal.classList.add("hidden");
  });

  els.toggleRawJsonBtn.addEventListener("click", () => {
    const showingRaw = !els.repoRawEditor.classList.contains("hidden");
    els.repoRawEditor.classList.toggle("hidden", showingRaw);
    els.repoVisualEditor.classList.toggle("hidden", !showingRaw);
    els.toggleRawJsonBtn.textContent = showingRaw ? "Edit Raw JSON" : "Visual Editor";
    if (!showingRaw) {
      els.rawJsonTextarea.value = JSON.stringify(state.repoData, null, 2);
    }
  });

  els.applyRawJsonBtn.addEventListener("click", () => {
    try {
      const parsed = JSON.parse(els.rawJsonTextarea.value);
      if (!parsed || !Array.isArray(parsed.packages)) {
        alert('JSON must have a "packages" array.');
        return;
      }
      state.repoData = parsed;
      els.repoModal.classList.add("hidden");
      refreshTargetMenu();
      runResolve();
    } catch (err) {
      alert("Invalid JSON: " + err.message);
    }
  });

  els.resetRepoBtn.addEventListener("click", () => {
    if (state.currentPreset) loadPreset(state.currentPreset);
  });

  els.addPkgSubmitBtn.addEventListener("click", addPackage);
}

// ---------------- Custom Dropdowns ----------------

function toggleDropdown(wrap, menu, otherWrap, otherMenu) {
  otherWrap.classList.remove("open");
  otherMenu.classList.add("hidden");
  const isOpen = wrap.classList.toggle("open");
  menu.classList.toggle("hidden", !isOpen);
}

function closeAllDropdowns() {
  els.presetDropdown.classList.remove("open");
  els.presetMenu.classList.add("hidden");
  els.targetDropdown.classList.remove("open");
  els.targetMenu.classList.add("hidden");
}

function renderPresetMenu() {
  els.presetMenu.innerHTML = "";
  state.presets.forEach((p) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = `select-option ${p.filename === state.currentPreset ? "selected" : ""}`;
    btn.innerHTML = `<span>${esc(p.label)}</span><span class="opt-sub">${p.packageCount} pkgs</span>`;
    btn.addEventListener("click", () => {
      closeAllDropdowns();
      loadPreset(p.filename);
    });
    els.presetMenu.appendChild(btn);
  });
}

function refreshTargetMenu(preferred = "") {
  const names = Array.from(new Set((state.repoData.packages || []).map((p) => p.name))).sort();
  if (preferred && names.includes(preferred)) {
    state.selectedTarget = preferred;
  } else if (!names.includes(state.selectedTarget)) {
    state.selectedTarget = names[0] || "";
  }

  els.targetTrigger.querySelector(".select-text").textContent = state.selectedTarget || "—";
  els.targetMenu.innerHTML = "";
  names.forEach((name) => {
    const count = (state.repoData.packages || []).filter((p) => p.name === name).length;
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = `select-option ${name === state.selectedTarget ? "selected" : ""}`;
    btn.innerHTML = `<span class="mono">${esc(name)}</span><span class="opt-sub">${count} ver</span>`;
    btn.addEventListener("click", () => {
      state.selectedTarget = name;
      els.targetTrigger.querySelector(".select-text").textContent = name;
      refreshTargetMenu(name);
      closeAllDropdowns();
      runResolve();
    });
    els.targetMenu.appendChild(btn);
  });
  els.repoCount.textContent = (state.repoData.packages || []).length;
}

// ---------------- Data & Resolution ----------------

async function loadPreset(filename) {
  state.currentPreset = filename;
  const meta = state.presets.find((p) => p.filename === filename);
  els.presetTrigger.querySelector(".select-text").textContent = meta ? meta.label : filename;
  renderPresetMenu();

  const res = await fetch(`/api/repos/${encodeURIComponent(filename)}`);
  state.repoData = await res.json();

  refreshTargetMenu(meta ? meta.defaultTarget : "");
  els.constraintInput.value = meta ? meta.defaultConstraint : "*";
  renderRepoList();
  await runResolve();
}

async function runResolve() {
  const targetPackage = state.selectedTarget;
  const constraint = els.constraintInput.value.trim() || "*";
  if (!targetPackage) return;

  els.resolveBtn.disabled = true;
  els.resolveBtn.textContent = "Resolving...";
  els.nodePopover.classList.add("hidden");

  try {
    const res = await fetch("/api/resolve", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ repoData: state.repoData, targetPackage, constraint }),
    });
    if (!res.ok) {
      const err = await res.json();
      alert(err.detail || "Resolution failed.");
      return;
    }
    state.lastResult = await res.json();
    renderAll();
  } catch (err) {
    alert("Cannot connect to backend: " + err.message);
  } finally {
    els.resolveBtn.disabled = false;
    els.resolveBtn.textContent = "Resolve";
  }
}

function addPackage() {
  const name = els.newPkgName.value.trim();
  const version = els.newPkgVer.value.trim();
  if (!name || !version) return alert("Enter package name and version.");

  const pkg = { name, version };
  const parsePairs = (str) => {
    const obj = {};
    str.split(",").forEach((s) => {
      const [k, ...rest] = s.split(":");
      if (k && k.trim()) obj[k.trim()] = rest.join(":").trim() || "*";
    });
    return obj;
  };
  const deps = parsePairs(els.newPkgDeps.value);
  const confs = parsePairs(els.newPkgConfs.value);
  if (Object.keys(deps).length) pkg.dependencies = deps;
  if (Object.keys(confs).length) pkg.conflicts = confs;

  state.repoData.packages.push(pkg);
  els.newPkgName.value = "";
  els.newPkgVer.value = "";
  els.newPkgDeps.value = "";
  els.newPkgConfs.value = "";

  renderRepoList();
  refreshTargetMenu();
  runResolve();
}

function renderRepoList() {
  const pkgs = state.repoData.packages || [];
  els.repoCount.textContent = pkgs.length;
  els.repoList.innerHTML = "";
  pkgs.forEach((p, idx) => {
    const deps = p.dependencies ? Object.entries(p.dependencies).map(([k, v]) => `${k} ${v}`).join(", ") : "none";
    const confs = p.conflicts ? Object.entries(p.conflicts).map(([k, v]) => `${k} ${v}`).join(", ") : "";
    const row = document.createElement("div");
    row.className = "repo-row";
    row.innerHTML = `
      <div>
        <strong class="mono">${esc(p.name)}@${esc(p.version)}</strong>
        <div style="font-size:12px; color:var(--muted);">
          Deps: ${esc(deps)} ${confs ? `· <span style="color:var(--red)">Conflicts: ${esc(confs)}</span>` : ""}
        </div>
      </div>
      <button class="btn btn-sm" title="Delete">Remove</button>
    `;
    row.querySelector("button").addEventListener("click", () => {
      state.repoData.packages.splice(idx, 1);
      renderRepoList();
      refreshTargetMenu();
      runResolve();
    });
    els.repoList.appendChild(row);
  });
}

function renderAll() {
  const r = state.lastResult;
  if (!r) return;
  const m = r.metrics || {};
  const total = m.totalRepoPackages || 0;
  const pruned = m.prunedCount || 0;
  const prunedPct = total > 0 ? Math.round((100 * pruned) / total) : 0;

  if (!r.success) {
    els.statusVal.textContent = "UNSAT";
    els.statusVal.className = "stat-value status-pill val-unsat";
  } else if (r.hasCycle) {
    els.statusVal.textContent = "SAT (Cycle)";
    els.statusVal.className = "stat-value status-pill val-cycle";
  } else {
    els.statusVal.textContent = "SAT";
    els.statusVal.className = "stat-value status-pill val-sat";
  }

  els.installedVal.textContent = r.success ? `${(r.resolvedPackages || []).length}` : "0";
  els.prunedVal.textContent = `${prunedPct}% (${pruned}/${total})`;
  els.cnfVal.textContent = `${m.numVariables || 0}v / ${m.numClauses || 0}c`;
  els.timeVal.textContent = `${Number(m.timeMs || 0).toFixed(2)} ms`;

  els.countPlan.textContent = r.success ? (r.resolvedPackages || []).length : 0;
  els.countCnf.textContent = (r.clauses || []).length;

  if (!r.success) {
    const c = r.conflict || {};
    els.alertBanner.className = "alert-banner alert-unsat";
    els.alertBanner.innerHTML = `
      <h4>Installation Impossible (UNSAT)</h4>
      <div style="white-space:pre-wrap;">${esc(c.rootCause || "Constraints cannot be satisfied.")}</div>
      ${c.suggestedFix ? `<div style="margin-top:8px;"><strong>Suggested Fix:</strong> ${esc(c.suggestedFix)}</div>` : ""}
    `;
    setGraphMode("pruning", false);
  } else if (r.hasCycle) {
    els.alertBanner.className = "alert-banner alert-cycle";
    els.alertBanner.innerHTML = `
      <h4>Circular Dependency Detected</h4>
      <div>A topological installation order does not exist because of this cycle: <span class="mono">${(r.cyclePath || []).map(esc).join(" → ")}</span></div>
    `;
  } else {
    els.alertBanner.className = "alert-banner hidden";
  }

  renderGraph();
  renderPlanTab();
  renderCnfTab();
  renderMetricsTab();
}

function setGraphMode(mode, redraw = true) {
  state.graphMode = mode;
  els.modeResolvedBtn.classList.toggle("active", mode === "resolved");
  els.modePruningBtn.classList.toggle("active", mode === "pruning");
  if (redraw) renderGraph();
}

// ---------------- Clean SVG Graph ----------------

function renderGraph() {
  const r = state.lastResult;
  if (!r) return;
  els.graphViewport.innerHTML = "";

  if (state.graphMode === "resolved") {
    if (!r.success) {
      drawEmptyMsg("No valid install graph (UNSAT). Switch to 'BFS Pruning View' to inspect candidates.");
      return;
    }
    drawResolvedGraph(r);
  } else {
    drawPruningGraph(r);
  }
}

function drawResolvedGraph(r) {
  const nodes = r.resolvedPackages || [];
  if (!nodes.length) return drawEmptyMsg("No packages resolved.");

  const tree = r.dependencyTree || {};
  const root = nodes.find((n) => n.name === r.targetPackage) || nodes[0];
  const cycleNodes = new Set(r.cyclePath || []);
  const cycleEdges = new Set();
  for (let i = 0; i < (r.cyclePath || []).length - 1; i++) {
    cycleEdges.add(`${r.cyclePath[i]}->${r.cyclePath[i + 1]}`);
  }

  const levelMap = { [root.id]: 0 };
  const q = [{ id: root.id, lv: 0 }];
  while (q.length) {
    const { id, lv } = q.shift();
    (tree[id] || []).forEach((child) => {
      if (levelMap[child] === undefined) {
        levelMap[child] = lv + 1;
        q.push({ id: child, lv: lv + 1 });
      }
    });
  }
  nodes.forEach((n) => { if (levelMap[n.id] === undefined) levelMap[n.id] = 0; });

  const levels = {};
  nodes.forEach((n) => {
    const lv = levelMap[n.id];
    (levels[lv] = levels[lv] || []).push(n);
  });

  const W = 166, H = 56, GX = 38, GY = 76;
  const pos = {};
  Object.keys(levels).map(Number).sort((a, b) => a - b).forEach((lv) => {
    const row = levels[lv];
    const startX = -(row.length * W + (row.length - 1) * GX) / 2;
    row.forEach((n, i) => { pos[n.id] = { x: startX + i * (W + GX), y: lv * (H + GY), pkg: n }; });
  });

  Object.entries(tree).forEach(([src, dsts]) => {
    dsts.forEach((dst) => {
      if (pos[src] && pos[dst]) {
        const isCycle = cycleEdges.has(`${src}->${dst}`);
        drawEdge(pos[src], pos[dst], W, H, isCycle ? "cycle" : "normal");
      }
    });
  });

  const order = r.installationOrder || [];
  nodes.forEach((n) => {
    const p = pos[n.id];
    const kind = cycleNodes.has(n.id) ? "cycle" : (n.id === root.id ? "root" : "ok");
    const step = order.indexOf(n.id);
    drawNode(p.x, p.y, W, H, n, kind, step >= 0 ? `${step + 1}` : "");
  });

  fitGraph();
}

function drawPruningGraph(r) {
  const candidates = r.candidates || [];
  const pruned = r.prunedPackages || [];
  const selectedIds = new Set(r.success ? (r.resolvedPackages || []).map((p) => p.id) : []);

  if (!candidates.length && !pruned.length) return drawEmptyMsg("Empty repository.");

  const byName = {};
  candidates.forEach((c) => (byName[c.name] = byName[c.name] || []).push(c));

  const levelMap = {};
  const q = [];
  candidates.forEach((c) => {
    if (c.name === r.targetPackage) { levelMap[c.id] = 0; q.push({ pkg: c, lv: 0 }); }
  });

  const edges = [];
  while (q.length) {
    const { pkg, lv } = q.shift();
    Object.keys(pkg.dependencies || {}).forEach((depName) => {
      (byName[depName] || []).forEach((d) => {
        edges.push([pkg.id, d.id]);
        if (levelMap[d.id] === undefined) { levelMap[d.id] = lv + 1; q.push({ pkg: d, lv: lv + 1 }); }
      });
    });
  }
  candidates.forEach((c) => { if (levelMap[c.id] === undefined) levelMap[c.id] = 0; });

  const levels = {};
  let maxLv = 0;
  candidates.forEach((c) => {
    maxLv = Math.max(maxLv, levelMap[c.id]);
    (levels[levelMap[c.id]] = levels[levelMap[c.id]] || []).push(c);
  });

  const W = 160, H = 54, GX = 28, GY = 72;
  const pos = {};
  Object.keys(levels).map(Number).sort((a, b) => a - b).forEach((lv) => {
    const row = levels[lv];
    const startX = -(row.length * W + (row.length - 1) * GX) / 2;
    row.forEach((c, i) => { pos[c.id] = { x: startX + i * (W + GX), y: lv * (H + GY), pkg: c }; });
  });

  if (pruned.length) {
    const py = (maxLv + 1) * (H + GY) + 34;
    const startX = -(pruned.length * W + (pruned.length - 1) * GX) / 2;
    const lbl = svgEl("text", { x: 0, y: py - 14, "text-anchor": "middle", fill: "#64748b", "font-size": 11, "font-weight": 600 });
    lbl.textContent = `PRUNED BY BFS (${pruned.length} unreachable)`;
    els.graphViewport.appendChild(lbl);
    pruned.forEach((p, i) => { pos[p.id] = { x: startX + i * (W + GX), y: py, pkg: p }; });
  }

  edges.forEach(([s, d]) => {
    if (pos[s] && pos[d]) {
      drawEdge(pos[s], pos[d], W, H, selectedIds.has(s) && selectedIds.has(d) ? "normal" : "muted");
    }
  });

  candidates.forEach((c) => {
    const kind = selectedIds.has(c.id) ? (c.name === r.targetPackage ? "root" : "ok") : "candidate";
    drawNode(pos[c.id].x, pos[c.id].y, W, H, c, kind, "");
  });

  pruned.forEach((p) => {
    drawNode(pos[p.id].x, pos[p.id].y, W, H, p, "pruned", "");
  });

  fitGraph();
}

function drawEdge(src, dst, w, h, kind) {
  const x1 = src.x + w / 2, y1 = src.y + h;
  const x2 = dst.x + w / 2, y2 = dst.y;
  let d;
  if (y2 <= y1) {
    const off = 55;
    d = `M ${src.x + w} ${src.y + h / 2} C ${src.x + w + off} ${src.y}, ${dst.x + w + off} ${dst.y + h}, ${dst.x + w} ${dst.y + h / 2}`;
  } else {
    const midY = (y1 + y2) / 2;
    d = `M ${x1} ${y1} C ${x1} ${midY}, ${x2} ${midY}, ${x2} ${y2 - 2}`;
  }
  const color = kind === "cycle" ? "#dc2626" : kind === "muted" ? "#94a3b8" : "#3b82f6";
  els.graphViewport.appendChild(svgEl("path", {
    d,
    fill: "none",
    stroke: color,
    "stroke-width": kind === "muted" ? 1.2 : 1.8,
    "stroke-dasharray": kind === "muted" ? "3,3" : "none",
    "marker-end": `url(#arrow-${kind})`,
  }));
}

function drawNode(x, y, w, h, pkg, kind, stepNum) {
  const styles = {
    root: { fill: "#eff6ff", stroke: "#2563eb", name: "#1e3a8a", ver: "#3b82f6", badgeBg: "#2563eb", badgeText: "#ffffff" },
    ok: { fill: "#ffffff", stroke: "#16a34a", name: "#0f172a", ver: "#16a34a", badgeBg: "#dcfce7", badgeText: "#166534" },
    cycle: { fill: "#fef2f2", stroke: "#dc2626", name: "#7f1d1d", ver: "#dc2626", badgeBg: "#fee2e2", badgeText: "#991b1b" },
    candidate: { fill: "#ffffff", stroke: "#cbd5e1", name: "#334155", ver: "#64748b", badgeBg: "#f1f5f9", badgeText: "#475569" },
    pruned: { fill: "#f8fafc", stroke: "#cbd5e1", name: "#94a3b8", ver: "#94a3b8", badgeBg: "#f1f5f9", badgeText: "#94a3b8" },
  };
  const st = styles[kind] || styles.candidate;

  const g = svgEl("g", { class: "svg-node", transform: `translate(${x}, ${y})` });
  g.appendChild(svgEl("rect", {
    width: w, height: h, rx: 10, ry: 10,
    fill: st.fill, stroke: st.stroke, "stroke-width": 1.6,
    "stroke-dasharray": kind === "pruned" ? "4,3" : "none",
    filter: kind === "pruned" ? "none" : "url(#node-shadow)",
  }));

  const nameLabel = svgEl("text", {
    x: 12, y: 24,
    fill: st.name,
    "font-size": 13, "font-weight": 700,
    "font-family": "-apple-system, BlinkMacSystemFont, Segoe UI, sans-serif",
  });
  nameLabel.textContent = pkg.name.length > 15 ? pkg.name.slice(0, 14) + "…" : pkg.name;
  g.appendChild(nameLabel);

  const verLabel = svgEl("text", {
    x: 12, y: 42,
    fill: st.ver,
    "font-size": 11.5, "font-weight": 600,
    "font-family": "Cascadia Code, Consolas, monospace",
  });
  verLabel.textContent = `v${pkg.version}`;
  g.appendChild(verLabel);

  if (stepNum) {
    const cx = w - 18, cy = 18;
    g.appendChild(svgEl("circle", { cx, cy, r: 9.5, fill: st.badgeBg }));
    const stepText = svgEl("text", {
      x: cx, y: cy + 3.5,
      "text-anchor": "middle", fill: st.badgeText,
      "font-size": 10, "font-weight": 700,
      "font-family": "Cascadia Code, Consolas, monospace",
    });
    stepText.textContent = stepNum;
    g.appendChild(stepText);
  }

  g.addEventListener("click", (e) => {
    e.stopPropagation();
    const id = pkg.id || `${pkg.name}@${pkg.version}`;
    const deps = pkg.dependencies ? Object.entries(pkg.dependencies).map(([k, v]) => `${k} ${v}`).join("<br>") : "None";
    const confs = pkg.conflicts ? Object.entries(pkg.conflicts).map(([k, v]) => `${k} ${v}`).join("<br>") : "";
    els.popTitle.textContent = id;
    els.popBody.innerHTML = `
      <div><strong>Dependencies:</strong><br><span class="mono">${deps}</span></div>
      ${confs ? `<div style="margin-top:6px; color:var(--red);"><strong>Conflicts:</strong><br><span class="mono">${confs}</span></div>` : ""}
    `;
    els.nodePopover.classList.remove("hidden");
  });

  els.graphViewport.appendChild(g);
}

function drawEmptyMsg(msg) {
  const t = svgEl("text", { x: 0, y: 0, "text-anchor": "middle", fill: "#64748b", "font-size": 14 });
  t.textContent = msg;
  els.graphViewport.appendChild(t);
  fitGraph();
}

function fitGraph() {
  const box = els.graphViewport.getBBox();
  const rect = els.svgContainer.getBoundingClientRect();
  if (!box.width || !box.height) {
    state.pan = { x: rect.width / 2, y: rect.height / 2, scale: 1 };
    return applyTransform();
  }
  const pad = 42;
  const scale = Math.min(1.15, Math.max(0.4, Math.min((rect.width - pad * 2) / box.width, (rect.height - pad * 2) / box.height)));
  state.pan = {
    x: rect.width / 2 - (box.x + box.width / 2) * scale,
    y: rect.height / 2 - (box.y + box.height / 2) * scale,
    scale,
  };
  applyTransform();
}

function adjustZoom(factor) {
  const rect = els.svgContainer.getBoundingClientRect();
  const cx = rect.width / 2, cy = rect.height / 2;
  const old = state.pan.scale;
  const next = Math.min(2.2, Math.max(0.3, old * factor));
  state.pan.x = cx - ((cx - state.pan.x) / old) * next;
  state.pan.y = cy - ((cy - state.pan.y) / old) * next;
  state.pan.scale = next;
  applyTransform();
}

function applyTransform() {
  els.graphViewport.setAttribute("transform", `translate(${state.pan.x}, ${state.pan.y}) scale(${state.pan.scale})`);
}

// ---------------- Tabs ----------------

function renderPlanTab() {
  const r = state.lastResult;
  if (!r.success) {
    els.tabPlan.innerHTML = `<p style="color:var(--muted);">No install plan — see the conflict explanation above.</p>`;
    return;
  }

  const orderHtml = r.hasCycle
    ? `<p style="color:var(--amber);">Cannot compute topological order due to cycle.</p>`
    : `<ol class="order-list">${(r.installationOrder || []).map((id, i) => `<li><span class="step-badge">${i + 1}</span><span>${esc(id)}</span></li>`).join("")}</ol>`;

  const rowsHtml = (r.resolvedPackages || []).map((p) => {
    const deps = p.dependencies ? Object.entries(p.dependencies).map(([k, v]) => `${k} ${v}`).join(", ") : "—";
    return `<tr><td class="mono">${esc(p.name)}</td><td class="mono" style="color:var(--green); font-weight:600;">${esc(p.version)}</td><td class="mono" style="color:var(--muted);">${esc(deps)}</td></tr>`;
  }).join("");

  els.tabPlan.innerHTML = `
    <div class="plan-grid">
      <div>
        <div class="section-label">Topological Install Order</div>
        ${orderHtml}
      </div>
      <div>
        <div class="section-label">Resolved Packages</div>
        <table class="table">
          <thead><tr><th>Package</th><th>Version</th><th>Dependencies</th></tr></thead>
          <tbody>${rowsHtml}</tbody>
        </table>
      </div>
    </div>
  `;
}

function renderCnfTab() {
  const r = state.lastResult;
  const vars = r.variables || [];
  const clauses = r.clauses || [];

  const varHtml = vars.map((v) => {
    const cls = v.assigned === true ? "var-chip var-true" : "var-chip";
    const val = v.assigned === true ? "T" : v.assigned === false ? "F" : "?";
    return `<span class="${cls}">X${v.id}: ${esc(v.name)} = ${val}</span>`;
  }).join("");

  const rows = clauses.map((c) => `
    <tr>
      <td class="mono">${c.index}</td>
      <td><span class="tag">${esc(c.type)}</span></td>
      <td class="mono">${esc(c.humanFormula)}</td>
      <td style="color:var(--muted);">${esc(c.description)}</td>
    </tr>
  `).join("");

  els.tabCnf.innerHTML = `
    <div class="cnf-top">
      <div class="section-label" style="margin:0;">Boolean Variables (${vars.length}) &amp; CNF Clauses (${clauses.length})</div>
      <button id="dlCnfBtn" class="btn btn-sm">Download DIMACS .cnf</button>
    </div>
    <div class="var-pills">${varHtml}</div>
    <table class="table">
      <thead><tr><th>#</th><th>Rule</th><th>Clause</th><th>Meaning</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>
  `;

  document.getElementById("dlCnfBtn").addEventListener("click", () => {
    if (!r.dimacs) return;
    const blob = new Blob([r.dimacs], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${r.targetPackage || "formula"}.cnf`;
    a.click();
    URL.revokeObjectURL(url);
  });
}

function renderMetricsTab() {
  const m = (state.lastResult && state.lastResult.metrics) || {};
  const rows = [
    ["Total Repository Packages", m.totalRepoPackages || 0],
    ["Reachable Candidates (BFS)", m.candidatePackagesCount || 0],
    ["Pruned Packages", m.prunedCount || 0],
    ["Boolean Variables (N)", m.numVariables || 0],
    ["CNF Clauses (M)", m.numClauses || 0],
    ["DPLL Decisions (DLIS)", m.decisions || 0],
    ["Unit Propagations (BCP)", m.unitPropagations || 0],
    ["Backtracks", m.backtracks || 0],
    ["Solve Time", `${Number(m.timeMs || 0).toFixed(2)} ms`],
  ];
  els.tabMetrics.innerHTML = `
    <table class="table" style="max-width:520px;">
      <thead><tr><th>Metric</th><th>Value</th></tr></thead>
      <tbody>${rows.map(([k, v]) => `<tr><td>${esc(k)}</td><td class="mono"><strong>${esc(v)}</strong></td></tr>`).join("")}</tbody>
    </table>
  `;
}

function svgEl(tag, attrs = {}) {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
  Object.entries(attrs).forEach(([k, v]) => el.setAttribute(k, v));
  return el;
}

function esc(s) {
  return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

init();
