#!/usr/bin/env python3
"""Boot a build of the game whose code or data moved and report whether it reaches gameplay.

  reloc_test.py <disc1.iso> [--build build/G4BE08] [--timeout 400]

Extracts disc 1 once (dtk disc extract) next to the ISO, copies the tree, puts the build's main.dol
and every rebuilt REL into the copy (the loose Rel/*.rel files and the RELs embedded in the em/*.drs
archives, through tools/drs.py rebuild), boots it in headless Dolphin (tools/motion/dolphin.py) and
drives the title menu. pG, pRK and pPL are read from the build's main.elf, so they follow the move.
OK means the player model is animating in the opening event. The disc 2 island RELs are not used
before that and are left out.
"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import time

ROOT = subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True,
                      cwd=os.path.dirname(os.path.abspath(__file__))).stdout.strip()
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from motion import dolphin  # noqa: E402

NM = os.path.join(ROOT, 'build', 'binutils', 'powerpc-eabi-nm')
DTK = os.path.join(ROOT, 'build', 'tools', 'dtk')


def make_disc(iso, build):
    base = os.path.splitext(os.path.realpath(iso))[0] + '_extracted'
    if not os.path.isdir(base):
        subprocess.run([DTK, 'disc', 'extract', '-q', iso, base], check=True)
    out = base + '_test'
    shutil.rmtree(out, ignore_errors=True)
    subprocess.run(['cp', '-r', '--reflink=auto', base, out], check=True)
    shutil.copy(os.path.join(build, 'main.dol'), os.path.join(out, 'sys', 'main.dol'))
    n = 0
    for cfg in sorted(glob.glob(os.path.join(ROOT, 'config', 'G4BE08', 'modules', '*', 'rel.json'))):
        c = json.load(open(cfg))
        new = os.path.join(build, c['name'], c['name'] + '.rel')
        loose = os.path.join(out, c['object'])
        if os.path.exists(loose):
            shutil.copy(new, loose)
            n += 1
        arc = os.path.join(os.path.dirname(loose), c['name'] + '.drs')
        if os.path.exists(arc):
            subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'drs.py'), 'rebuild', arc, new, arc + '.new'], check=True,
                           capture_output=True)
            os.replace(arc + '.new', arc)
            n += 1
    print(f'disc tree {out}: main.dol and {n} RELs from {build}', flush=True)
    return out


def boot(disc, elf, timeout):
    sym = {}
    for line in subprocess.run([NM, elf], capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3 and p[2] in ('pG', 'pPL', 'pRK'):
            sym[p[2]] = int(p[0], 16)
    dolphin.PG_ADDR, dolphin.PPL_ADDR = sym['pG'], sym['pPL']
    lo, hi = dolphin.MEM1_BASE, dolphin.MEM1_BASE + dolphin.MEM1_SIZE

    def title_state(mem):
        pg, prk = mem.u32(sym['pG']), mem.u32(sym['pRK'])
        if not (lo <= pg < hi and lo <= prk < hi):
            return (0, 0)
        return (mem.u32(pg + 0x54), mem.read(prk + 0x17, 1)[0])

    dol = dolphin.Dolphin(os.path.join(disc, 'sys', 'main.dol'), max_seconds=timeout + 60)
    t0 = time.time()
    try:
        while title_state(dol.mem)[1] != 1:
            if time.time() - t0 > 150:
                return 'FAIL: the title menu was not reached (see /tmp/mot/dolphin_run.log)'
            dol.press('START')
        time.sleep(2)
        dol.press('D_UP', wait=0.6)
        dol.press('A', wait=0.6)
        time.sleep(4)
        frames = set()
        while time.time() - t0 < timeout:
            st = dolphin.player_state(dol.mem)
            if st and st[1]:
                frames.add(st[2])
                if len(frames) >= 3:
                    return f'OK: the player is animating after {time.time() - t0:.0f} s'
                time.sleep(0.5)
                continue
            dol.press('B' if title_state(dol.mem)[0] & 0x1000 else 'A', wait=0.6)
            time.sleep(1.5)
        return 'FAIL: no player motion within the timeout'
    finally:
        dol.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('iso')
    ap.add_argument('--build', default=os.path.join(ROOT, 'build', 'G4BE08'))
    ap.add_argument('--timeout', type=int, default=400)
    a = ap.parse_args()
    disc = make_disc(a.iso, os.path.abspath(a.build))
    print(boot(disc, os.path.join(os.path.abspath(a.build), 'main.elf'), a.timeout))


if __name__ == '__main__':
    main()
