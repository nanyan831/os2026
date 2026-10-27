#!/usr/bin/env python3
"""Audit protected framework regions, current-source evidence and delivery links."""
import hashlib
import json
from pathlib import Path
import re
import struct
import verify_lab3

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT.parent / "report"
LOGS = REPORT / "logs"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def regions(tree):
    result = {}
    for name in ("kern/trap/trap.h", "kern/sync/sync.h", "libs/atomic.h"):
        result[name] = digest((tree / name).read_bytes())
    assembly = (tree / "kern/trap/trapentry.S").read_text()
    save = re.search(r"(?ms)^    \.macro SAVE_ALL.*?^    \.endm", assembly)
    if not save:
        raise RuntimeError("SAVE_ALL not found")
    result["SAVE_ALL"] = digest(save.group().encode())
    trap = (tree / "kern/trap/trap.c").read_text()
    result["trap.c-before-function2"] = digest(trap.split("//lab3 function2")[0].encode())
    result["trap.c-after-function2"] = digest(trap[trap.index("static inline void trap_dispatch"):].encode())
    for file, functions in (("default_pmm.c", ("basic_check", "default_check")),
                            ("best_fit_pmm.c", ("basic_check", "best_fit_check"))):
        text = (tree / "kern/mm" / file).read_text()
        for function in functions:
            match = re.search(r"(?ms)^static void\s+" + function + r"\(void\)\s*\{.*?^\}", text)
            if not match:
                raise RuntimeError(f"protected function not found: {file}:{function}")
            result[f"{file}:{function}"] = digest(match.group().encode())
    return result


def main():
    checks = {}
    baseline_file = LOGS / "framework-protected-baseline.json"
    original = ROOT.parent.parent / "lab3/lab3_new"
    if original.exists():
        baseline = regions(original)
        if not baseline_file.exists():
            baseline_file.write_text(json.dumps(baseline, indent=2) + "\n")
        checks["original-framework-baseline"] = json.loads(baseline_file.read_text()) == baseline
    baseline = json.loads(baseline_file.read_text())
    checks["protected-framework-regions"] = regions(ROOT) == baseline

    tests = json.loads((LOGS / "official-tests-baseline.json").read_text(encoding="utf-8-sig"))
    checks["official-five-test-files"] = all(
        verify_lab3.sha(ROOT / item["Path"]) == item["SHA256"].lower() for item in tests)
    source = verify_lab3.source_manifest()
    harness = {name: verify_lab3.sha(ROOT / "tools" / name) for name in
               ("test_lab3_extra.c", "test_lab3_extra_context.S", "qemu_compat/qemu-system-riscv64")}
    matrices = {}
    for label in ("qemu6", "qemu7"):
        state = json.loads((LOGS / f"verification-{label}.json").read_text())
        results = state["results"]
        matrices[label] = {"passed": sum(r["passed"] for r in results), "total": len(results)}
        checks[f"{label}-current-source"] = state["source_sha256"] == source
        checks[f"{label}-current-harness"] = state["harness_sha256"] == harness
        checks[f"{label}-six-cases"] = len(results) == 6 and all(r["passed"] for r in results)
        checks[f"{label}-unchanged-test-evidence"] = all(
            state["official_test_sha256"][Path(item["Path"]).name] == item["SHA256"].lower()
            for item in tests)

    checks["no-injected-source-directory"] = not (ROOT / "kern/test").exists() and not (ROOT / "kern/extra").exists()
    init = (ROOT / "kern/init/init.c").read_text()
    checks["no-injected-init-markers"] = "test_lab3.h" not in init and "lab3_extra_check" not in init
    checks["no-unfilled-kernel-markers"] = not any(
        "YOUR CODE" in p.read_text() for p in (ROOT / "kern").rglob("*")
        if p.is_file() and p.suffix in (".c", ".h", ".S"))

    bad_links = []
    documents = [ROOT.parent / "README.md", *REPORT.rglob("*.md")]
    links_checked = 0
    for document in documents:
        for match in re.finditer(r"\]\(([^)]+)\)", document.read_text()):
            target = match.group(1).strip("<>")
            if re.match(r"(?:https?://|mailto:|#)", target):
                continue
            target = target.split("#")[0]
            resolved = (document.parent / target).resolve()
            if not resolved.exists() and resolved != REPORT / "audit.json":
                bad_links.append(f"{document.relative_to(ROOT.parent)} -> {target}")
            links_checked += 1
    checks["local-document-links"] = not bad_links

    images = {}
    for path in sorted((REPORT / "images").glob("*.png")):
        data = path.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            continue
        width, height = struct.unpack(">II", data[16:24])
        images[path.name] = {"width": width, "height": height, "sha256": digest(data)}
    checks["five-terminal-screenshots"] = len(images) == 5 and all(
        item["width"] >= 1000 and item["height"] >= 500 for item in images.values())

    workspace = json.loads((LOGS / "workspace-baseline.json").read_text(encoding="utf-8-sig"))
    base = ROOT.parent.parent
    missing_workspace = []
    if (base / "labcodes").exists():
        for item in workspace:
            path = base / item["Tree"].replace("\\", "/") / item["Path"].replace("\\", "/")
            if not path.is_file() or verify_lab3.sha(path) != item["SHA256"].lower():
                missing_workspace.append(str(path))
        checks["prior-workspace-files-preserved"] = not missing_workspace

    result = {"passed": all(checks.values()), "checks": checks, "matrices": matrices,
              "local_links_checked": links_checked, "broken_links": bad_links,
              "screenshots": images, "prior_workspace_mismatches": missing_workspace,
              "source_sha256": source}
    (REPORT / "audit.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    for name, passed in checks.items():
        print(f"{'PASS' if passed else 'FAIL'} {name}")
    print(f"Audit: {sum(checks.values())}/{len(checks)}")
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
