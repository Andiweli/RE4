#!/usr/bin/env python3
"""Try removing the codeless steering constructs and keep each removal that leaves the unit identical.

  sweep_cdiff.py [src/<path> ...]      (default: every source with a codeless asm or a register pin)

Every codeless asm statement (`asm("" ...);`, `asm volatile("");`) is tried deleted, and every
`register T x asm("rN")` pin is tried without `register` and the `asm("rN")`. A `// COMPILER-DIFF:`
comment on the construct's line, or a comment block starting with one right above it, goes with it.
Every candidate is compiled alone with variant.py (the unit's production command, build/ untouched);
the ones that stay IDENTICAL are then applied together and re-checked, one at a time if the
combination breaks. Files compiled into more than one unit (#include "x.cpp") and the SDK sources
(src/lib, from dolsdk2004 and the vendors) are skipped. Rerun it after a compiler or header change;
a clean rebuild with `dtk shasum` is still the final check.
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True,
                      cwd=os.path.dirname(os.path.abspath(__file__))).stdout.strip()
VARIANT = os.path.join(ROOT, 'tools', 'research', 'kit', 'variant.py')
CODELESS = re.compile(r'^\s*(__)?asm(__)?\s*(volatile\s*|__volatile__\s*)?\(\s*""[^;]*\)\s*;\s*(//.*)?$')
PIN = re.compile(r'\bregister\s+([^;=]*?)\s+asm\s*\(\s*"[rf]\d+"\s*\)')


def units():
    out = {}
    for m in re.finditer(r'^build build/G4BE08/src/(\S+)\.o: \S+ (src/\S+\.(?:cpp|c))', open(os.path.join(ROOT, 'build.ninja')).read(), re.M):
        out[m.group(2)] = m.group(1)
    return out


def included_sources():
    inc = set()
    for dp, _, fn in os.walk(os.path.join(ROOT, 'src')):
        for n in fn:
            p = os.path.join(dp, n)
            for m in re.finditer(r'#include\s+"([^"]+\.(?:cpp|c))"', open(p, errors='replace').read()):
                for base in (dp, os.path.join(ROOT, 'src')):
                    q = os.path.normpath(os.path.join(base, m.group(1)))
                    if os.path.exists(q):
                        inc.add(os.path.relpath(q, ROOT))
    return inc


def tag_block_above(lines, i):
    """Line indices of the comment block right above line i when it starts with a COMPILER-DIFF tag."""
    j = i - 1
    while j >= 0 and lines[j].lstrip().startswith('//'):
        j -= 1
    block = list(range(j + 1, i))
    return block if block and 'COMPILER-DIFF' in lines[block[0]] else []


def candidates(lines):
    """[(description, {line index: new text or None to delete})], one per construct."""
    out = []
    for i, line in enumerate(lines):
        code, sep, comment = line.partition('//')
        if CODELESS.match(line):
            edit = {i: None}
        elif PIN.search(code):
            new = PIN.sub(r'\1', code).rstrip()
            if sep and 'COMPILER-DIFF' not in comment:
                new += ' //' + comment
            edit = {i: new}
        else:
            continue
        for k in tag_block_above(lines, i):
            edit[k] = None
        out.append((f'line {i + 1}', edit))
    return out


def apply(lines, edits):
    merged = {}
    for e in edits:
        merged.update(e)
    return [merged.get(i, l) for i, l in enumerate(lines) if merged.get(i, l) is not None]


def identical(unit, src_path, lines):
    base = os.path.basename(src_path)
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, base)
        open(p, 'w').write('\n'.join(lines))
        r = subprocess.run([sys.executable, VARIANT, unit, p, '--no-diff'], capture_output=True, text=True, cwd=ROOT)
    return 'VERDICT: IDENTICAL' in r.stdout


def sweep(src, unit):
    path = os.path.join(ROOT, src)
    lines = open(path).read().split('\n')
    cands = candidates(lines)
    ok = [e for _, e in cands if identical(unit, src, apply(lines, [e]))]
    if not ok:
        return 0, len(cands)
    if not identical(unit, src, apply(lines, ok)):
        kept = []
        for e in ok:
            if identical(unit, src, apply(lines, kept + [e])):
                kept.append(e)
        ok = kept
    if ok:
        open(path, 'w').write('\n'.join(apply(lines, ok)))
    return len(ok), len(cands)


def main():
    umap = units()
    skip = included_sources()
    srcs = sys.argv[1:] or sorted(s for s in umap if not s.startswith('src/lib/') and s not in skip)
    total_ok = total = 0
    for src in srcs:
        if src in skip or src not in umap:
            print(f'{src}: skipped ({"compiled into several units" if src in skip else "no unit"})')
            continue
        n, c = sweep(src, umap[src])
        total_ok += n
        total += c
        if c:
            print(f'{src}: {n} of {c} removed', flush=True)
    print(f'TOTAL {total_ok} of {total} candidates removed')


if __name__ == '__main__':
    main()
