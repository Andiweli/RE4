#!/usr/bin/env python3
"""Names for the archive entries the enemy modules address with ARC(n) / EM_ARC(x, n).

usage: arc_names.py <disc1.iso>        writes include/arc/<module>.h and rewrites the literals in src/

A rerun names only the literals left in src/ and keeps the names already in the headers, so an
entry renamed by hand (edit the #define and its uses) stays renamed.

For each module under src/ that indexes its own archive (em->subArc: ARC(n), and EM_ARC(x, n) where
x's subArc is the enemy's), every literal index gets a #define in include/arc/<module>.h:

  <MODULE>_<TYPE>_<NAME>   TYPE is MOT (FCV motion), SEQ (sequence table), BIN (model), TPL
                           (textures), EFF or ENT (empty entry), read from the module's archive on
                           the disc. NAME is the routine that plays the motion when exactly one
                           does (tools/motion/refs.py), the model role from the character table
                           (tools/motion/character.py), or the one function every use of the entry
                           is in. A name shared by several entries ends in the index; an entry with
                           no evidence is only the index in hex, to be renamed once its use is known.

The literals are replaced by the names, and the header is included on the blank line after the
file's include block, so no line moves (the asserts' line numbers depend on them). Defines, not
enums: a macro never reaches the compiler, so it cannot shift declaration numbering.
"""
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'motion'))
import archive  # noqa: E402
import character  # noqa: E402
import gen_refs  # noqa: E402
import refs  # noqa: E402

SRC = os.path.join(ROOT, 'src')
OUT = os.path.join(ROOT, 'include', 'arc')
NUM = r'(0x[0-9A-Fa-f]+|\d+)'
ARC_USE = re.compile(r'\bARC\(\s*' + NUM + r'\s*\)')
EM_ARC_USE = re.compile(r'\bEM_ARC\(\s*([^,()]+?)\s*,\s*' + NUM + r'\s*\)')
TYPE = {'FCV': 'MOT', 'SEQ': 'SEQ', 'BIN': 'BIN', 'TPL': 'TPL', 'EFF': 'EFF', '': 'ENT'}
ROLE = {'body': 'BODY', 'head': 'HEAD', 'right hand': 'HAND_R', 'left hand': 'HAND_L', 'hair': 'HAIR',
        'eyes': 'EYES', 'face': 'FACE', 'costume': 'COSTUME'}
# the modules' set-up functions (Em1eSet, em31Init, ...) say nothing about an entry
GENERIC = {'SET', 'INIT', 'MODEL_INIT', 'MOVE'}


def em_arc_is_module(first, text):
    """EM_ARC(x, n) indexes the module's archive: x is the enemy, or the file sets x->subArc from an
    enemy's (the grab routines: pl->subArc = em->subArc), as tools/motion/gen_refs.py classifies."""
    if first in ('this', 'em', 'pEm'):
        return True
    return bool(re.search(r'subArc\s*=\s*[\w.>()-]*->subArc\b', text))


def routine_name(func):
    """em31_R1_Walk -> WALK, plem39_Knife4Atk -> PL_KNIFE4_ATK, cRoutine::moveBlast -> MOVE_BLAST."""
    s = func.split('::')[-1].lstrip('~')
    prefix = ''
    if re.match(r'pl(em|boat)', s):
        prefix, s = 'PL_', s[2:]
    elif re.match(r'sub([A-Z]|em)', s):
        prefix, s = 'SUB_', s[3:]
    s = re.sub(r'^(em[0-9a-f]{2}|boat|pl0[ef])_?', '', s, flags=re.I)
    s = re.sub(r'^R\d_', '', s)
    s = re.sub(r'(?<=[a-z0-9])(?=[A-Z])', '_', s)
    s = re.sub(r'_+', '_', s).strip('_').upper()
    return prefix + s if s else None


def module_uses():
    """{module: {index: set((file, function))}} of the literal indices."""
    uses = collections.defaultdict(lambda: collections.defaultdict(set))
    for dp, _, fn in os.walk(SRC):
        for name in sorted(fn):
            if not name.endswith('.cpp'):
                continue
            path = os.path.join(dp, name)
            f = gen_refs.File(path)
            module = f.module
            for m in ARC_USE.finditer(f.text):
                uses[module][int(m.group(1), 0)].add((path, f.function_at(f.line_of(m.start()))[0]))
            for m in EM_ARC_USE.finditer(f.text):
                if em_arc_is_module(m.group(1), f.text):
                    uses[module][int(m.group(2), 0)].add((path, f.function_at(f.line_of(m.start()))[0]))
    return uses


def evidence(module):
    """{index: set(routines)} of the motions (and their sequence tables) the module's code plays."""
    ev = collections.defaultdict(set)
    for r in refs.REFS:
        if r['class'] == 'em' and r['stem'] == module:
            ev[r['index']].add(r['function'])
            if r['seq'] is not None:
                ev[r['seq']].add(r['function'])
    return ev


def model_roles(module):
    roles = {}
    for a in character.attachments(module) or []:
        role = ROLE.get(a.role)
        if role is None:
            continue
        roles.setdefault(a.bin + archive.ARC_INDEX_BASE, role)
        roles.setdefault(a.tpl + archive.ARC_INDEX_BASE, role)
    return roles


def names_for(module, idx_uses, arc):
    ev = evidence(module)
    roles = model_roles(module)
    base = {}
    for i, sites in idx_uses.items():
        tag = arc.entry(i - archive.ARC_INDEX_BASE).tag
        kind = TYPE[tag]
        funcs = {fn for _, fn in sites}
        word = None
        if kind in ('BIN', 'TPL') and i in roles:
            word = roles[i]
        elif kind in ('MOT', 'SEQ') and len(ev.get(i, ())) == 1:
            word = routine_name(next(iter(ev[i])))
        elif len(funcs) == 1 and '?' not in funcs:
            word = routine_name(next(iter(funcs)))
        if word in GENERIC:
            word = None
        base[i] = (kind, word)
    count = collections.Counter(v for v in base.values() if v[1])
    out = {}
    for i, (kind, word) in sorted(base.items()):
        up = module.upper()
        if word is None:
            out[i] = f'{up}_{kind}_{i:03X}'
        elif count[(kind, word)] > 1:
            out[i] = f'{up}_{kind}_{word}_{i:03X}'
        else:
            out[i] = f'{up}_{kind}_{word}'
    return out


def existing_names(module):
    """{index: name} already in include/arc/<module>.h (a rerun keeps them, renamed ones included)."""
    path = os.path.join(OUT, f'{module}.h')
    if not os.path.exists(path):
        return {}
    return {int(m.group(2), 16): m.group(1)
            for m in re.finditer(r'^#define\s+(\w+)\s+(0x[0-9A-Fa-f]+)\s*$', open(path).read(), re.M)}


def write_header(module, names):
    os.makedirs(OUT, exist_ok=True)
    guard = f'ARC_{module.upper()}_H'
    lines = [f'// Entries of {module}\'s archive that its code addresses with ARC() / EM_ARC(), generated by',
             '// tools/arc_names.py. MOT is a motion, SEQ a sequence table, BIN a model, TPL textures, ENT an',
             '// empty entry. The word after the type is the routine that plays the motion or the model\'s',
             '// role; a hex number means the entry has no name yet.',
             f'#ifndef {guard}', f'#define {guard}', '']
    w = max(len(n) for n in names.values())
    lines += [f'#define {n:<{w}} 0x{i:03X}' for i, n in sorted(names.items())]
    lines += ['', f'#endif  // {guard}', '']
    open(os.path.join(OUT, f'{module}.h'), 'w').write('\n'.join(lines))


def rewrite(path, module, names):
    text = open(path).read()

    def arc(m):
        return f'ARC({names[int(m.group(1), 0)]})'

    def em_arc(m):
        if not em_arc_is_module(m.group(1), text):
            return m.group(0)
        return f'EM_ARC({m.group(1)}, {names[int(m.group(2), 0)]})'

    new = EM_ARC_USE.sub(em_arc, ARC_USE.sub(arc, text))
    lines = new.split('\n')
    last_inc = max(i for i, l in enumerate(lines[:200]) if l.startswith('#include'))
    if f'#include "arc/{module}.h"' not in lines:
        if lines[last_inc + 1] != '':
            sys.exit(f'{path}: no blank line after the include block to put the #include on')
        lines[last_inc + 1] = f'#include "arc/{module}.h"'
    open(path, 'w').write('\n'.join(lines))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    arcs = {a.stem: a for a in archive.open_source(sys.argv[1])}
    for module, idx in sorted(module_uses().items()):
        if module not in arcs:
            sys.exit(f'{module}: no {module}.drs on the disc')
        names = {**names_for(module, idx, arcs[module]), **existing_names(module)}
        write_header(module, names)
        files = sorted({path for sites in idx.values() for path, _ in sites})
        for path in files:
            rewrite(path, module, names)
        unnamed = sum(1 for n in names.values() if re.fullmatch(r'[A-Z0-9]+_[A-Z]+_[0-9A-F]{3}', n))
        print(f'{module}: {len(names)} entries, {len(names) - unnamed} named, {len(files)} files')


if __name__ == '__main__':
    main()
