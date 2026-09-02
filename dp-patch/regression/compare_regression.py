#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Compare unpatched (ref) against patched (test) DPLR regression output.

Layout expected next to this file::

    case/{input.lmp,system.data}            the one deck that drove every run
    v3.0.0/ref/np1/{dump.lammpstrj,log.lammps}    # WITHOUT patches
    v3.0.0/test/np1/{dump.lammpstrj,log.lammps}   # WITH both patches
    v3.0.0/ref/np4/...   v3.0.0/test/np4/...
    v3.1.0/...

Every run used the same input deck and the same geometry, so they are stored once
under ``case/`` rather than in each leaf; their md5s are pinned in CASE_MD5 below
and verified before any comparison.

ref and test must agree: both patches are meant to be behaviour-preserving for a
plain ``run``. Comparison is always ref-vs-test at the SAME rank count.

Run it::

    python3 compare_regression.py            # all versions, table + exit status
    python3 compare_regression.py v3.1.3     # just one

Exit code is 0 when every version matches, 1 on a mismatch, and 2 if the record
itself is unusable -- the shared deck missing or altered, or no version
directories found. Stdlib only.
"""

from __future__ import annotations

import hashlib
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent

# The single deck and geometry behind all 24 runs. ref and test cannot have been
# driven by different inputs -- there is only one of each -- but a corrupted or
# swapped file would silently invalidate every comparison below, so the contents
# are pinned here and checked once before anything else runs.
CASE_MD5 = {
    "input.lmp": "797c06eeaff2817c06dbb95153276492",
    "system.data": "a9ea654cd944afd98f5d58909a3713cc",
}

# LAMMPS writes the dump with %g (6 significant figures) and thermo with 8, so
# "identical" below means identical to the precision LAMMPS records. Over 100
# steps any real difference in centroid placement amplifies well past that.
THERMO_START = re.compile(r"^\s*Per MPI rank memory allocation")
THERMO_END = re.compile(r"^\s*Loop time of")


def parse_dump(path: pathlib.Path):
    """Return (column names, {timestep: {atom id: [values]}})."""
    frames: dict[int, dict[int, list[str]]] = {}
    cols: list[str] | None = None
    ts: int | None = None
    mode = None
    with open(path) as fh:
        for line in fh:
            if line.startswith("ITEM:"):
                item = line[5:].strip()
                if item == "TIMESTEP":
                    mode = "ts"
                elif item.startswith("ATOMS"):
                    cols = item.split()[1:]
                    mode = "atoms"
                    frames[ts] = {}
                else:
                    mode = None
                continue
            if mode == "ts":
                ts = int(line.strip())
            elif mode == "atoms":
                f = line.split()
                frames[ts][int(f[0])] = f[1:]
    return cols, frames


def parse_thermo(path: pathlib.Path):
    """Return (header, rows) for the thermo table in a LAMMPS log."""
    header, rows, inblock = None, [], False
    with open(path) as fh:
        for line in fh:
            if THERMO_START.search(line):
                inblock = "await-header"
                continue
            if inblock == "await-header":
                header = line.split()
                inblock = True
                continue
            if inblock is True:
                if THERMO_END.search(line):
                    inblock = False
                    continue
                f = line.split()
                if not f:
                    continue
                try:
                    [float(x) for x in f]
                except ValueError:
                    continue          # warnings interleaved in the table
                rows.append(f)
    return header, rows


def _dev(a: str, b: str):
    fa, fb = float(a), float(b)
    d = abs(fa - fb)
    scale = max(abs(fa), abs(fb))
    return d, (d / scale if scale else 0.0)


def compare_dump(ref: pathlib.Path, test: pathlib.Path) -> dict:
    if ref.read_bytes() == test.read_bytes():
        return {"status": "IDENTICAL"}
    ca, fa = parse_dump(ref)
    cb, fb = parse_dump(test)
    if ca != cb:
        return {"status": "DIFF", "reason": f"columns {ca} vs {cb}"}
    if sorted(fa) != sorted(fb):
        return {"status": "DIFF", "reason": f"frames {sorted(fa)} vs {sorted(fb)}"}
    worst: dict[str, tuple] = {}
    for ts in sorted(fa):
        A, B = fa[ts], fb[ts]
        if sorted(A) != sorted(B):
            return {"status": "DIFF", "reason": f"atom ids differ at step {ts}"}
        for aid in A:
            for i, (x, y) in enumerate(zip(A[aid], B[aid])):
                name = ca[i + 1]
                if name == "element":
                    if x != y:
                        return {"status": "DIFF", "reason": f"element differs, id {aid}"}
                    continue
                if x == y:
                    continue
                d, r = _dev(x, y)
                if d > worst.get(name, (0.0,))[0]:
                    worst[name] = (d, r, ts, aid)
    if not worst:
        return {"status": "EQUAL", "frames": len(fa)}
    return {"status": "DIFF", "worst": worst}


def compare_thermo(ref: pathlib.Path, test: pathlib.Path) -> dict:
    ha, ra = parse_thermo(ref)
    hb, rb = parse_thermo(test)
    if ha != hb:
        return {"status": "DIFF", "reason": f"header {ha} vs {hb}"}
    if len(ra) != len(rb):
        return {"status": "DIFF", "reason": f"{len(ra)} vs {len(rb)} rows"}
    worst: dict[str, tuple] = {}
    for rowa, rowb in zip(ra, rb):
        for i, (x, y) in enumerate(zip(rowa, rowb)):
            if x == y:
                continue
            d, r = _dev(x, y)
            name = ha[i] if ha and i < len(ha) else f"col{i}"
            if d > worst.get(name, (0.0,))[0]:
                worst[name] = (d, r, rowa[0], None)
    if not worst:
        return {"status": "EQUAL", "rows": len(ra)}
    return {"status": "DIFF", "worst": worst}


def check_case(root: pathlib.Path = HERE) -> list[str]:
    """Verify the shared deck under ``case/`` against CASE_MD5.

    Every run below was driven by this one deck and this one geometry. If either
    file is missing or has changed, the output comparison proves nothing -- so
    this is checked first and reported as a hard failure, not as a mismatch.

    Returns a list of problems; empty means the deck is intact.
    """
    problems = []
    for name, want in CASE_MD5.items():
        path = root / "case" / name
        if not path.is_file():
            problems.append(f"case/{name} is missing")
            continue
        got = hashlib.md5(path.read_bytes()).hexdigest()
        if got != want:
            problems.append(f"case/{name} md5 {got} != expected {want}")
    return problems


def compare_case(version_dir: pathlib.Path, np: str) -> dict | None:
    """Compare ref vs test for one version at one rank count."""
    ref, test = version_dir / "ref" / np, version_dir / "test" / np
    if not ref.is_dir() or not test.is_dir():
        return None
    out = {
        "dump": compare_dump(ref / "dump.lammpstrj", test / "dump.lammpstrj"),
        "thermo": compare_thermo(ref / "log.lammps", test / "log.lammps"),
    }
    rank = {"IDENTICAL": 0, "EQUAL": 1, "DIFF": 2}
    out["verdict"] = max((v["status"] for v in out.values()),
                         key=lambda s: rank.get(s, 9))
    return out


def iter_versions(root: pathlib.Path = HERE):
    return sorted(p for p in root.iterdir() if p.is_dir() and p.name.startswith("v"))


def main(argv: list[str]) -> int:
    problems = check_case()
    if problems:
        for problem in problems:
            print(problem, file=sys.stderr)
        return 2

    wanted = argv[1:]
    versions = [v for v in iter_versions() if not wanted or v.name in wanted]
    if not versions:
        print("no version directories found", file=sys.stderr)
        return 2

    print(f"{'version':10} {'ranks':6} {'verdict':10} detail")
    print("-" * 72)
    failures = 0
    for vdir in versions:
        nps = sorted({p.name for side in ("ref", "test")
                      for p in (vdir / side).iterdir() if p.is_dir()})
        for np in nps:
            r = compare_case(vdir, np)
            if r is None:
                print(f"{vdir.name:10} {np:6} {'MISSING':10}")
                failures += 1
                continue
            detail = ""
            if r["verdict"] == "DIFF":
                failures += 1
                bits = []
                for what in ("dump", "thermo"):
                    rr = r.get(what, {})
                    if rr.get("status") != "DIFF":
                        continue
                    if "reason" in rr:
                        bits.append(f"{what}: {rr['reason']}")
                    else:
                        col, (d, rel, step, _) = max(rr["worst"].items(),
                                                     key=lambda kv: kv[1][0])
                        bits.append(f"{what} {col}: max|d|={d:.3e} rel={rel:.2e} @step {step}")
                detail = "; ".join(bits)
            print(f"{vdir.name:10} {np:6} {r['verdict']:10} {detail}")
    print("-" * 72)
    if failures:
        print(f"{failures} comparison(s) FAILED")
        return 1
    print(f"all {sum(1 for _ in versions)} version(s) match: patched == unpatched")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
