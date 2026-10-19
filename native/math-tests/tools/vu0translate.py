#!/usr/bin/env python3
"""Translates VU0 microprograms of the retail microcode sets into C++ on the native build's VU0
(src/platform/native/math/vu0.h), an instruction pair at a time in the program's order:

    vu0translate.py            writes src/platform/native/math/microprograms.cpp

The pipelines' timing is worked out here, ahead of time, the way PCSX2's VU0 interpreter runs a program (pcsx2/VUops.cpp,
VU0microInterp.cpp: the decomp's reference), and each read is pointed at the value it gets:

- an instruction takes a cycle; an FMAC pipeline instruction (an upper one, or LQ, SQ, MOVE, MR32, FMAND, RINIT, RGET in the
  lower slot) reading a VF register field an FMAC instruction in the last 4 cycles writes stalls until it's written;
- the MAC flags an FMAND reads are those of the last FMAC pipeline instruction 4 cycles or more back (the flags' pipeline
  entries are flushed once 4 cycles old, before the stalled instruction runs);
- Q is written 7 cycles after DIV or SQRT, 13 after RSQRT (until then Q's old value is read); a DIV, SQRT, RSQRT or WAITQ
  waits for the one before it, and a Q read in the same pair sees the new Q (the wait comes first);
- a branch reading an integer register the instruction just before it wrote (an integer ALU instruction) reads its old value;
- a lower instruction reading the register its upper instruction writes reads the old value (both writing it: the lower one
  is dropped); an I bit's immediate is I after the upper instruction;
- a branch's or the E bit's delay slot runs, and at the end everything pending is written (Q).

Every path through a program (its branches' ways) is simulated; an instruction's translation (with the pipeline writes due
before it) has to come out the same on every path that reaches it, so a program is one function with its instructions in
order, labelled, branches as gotos. Anything outside what the translated programs use stops the translation."""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import vudis  # noqa: E402

ROOT = vudis.ROOT
OUTPUT = os.path.join(ROOT, 'src', 'platform', 'native', 'math', 'microprograms.cpp')

# The programs the maths calls: (set, address, what calls it)
PROGRAMS = [
    ('std', 0x0F0, 'Platform::Math::SinCos'),
    ('std', 0x1C0, 'Platform::Math::SlerpRotations'),
    ('std', 0x548, 'Platform::Math::JointMatrix (a rotation, no scale)'),
    ('std', 0x668, 'Platform::Math::JointMatrix (a rotation and a scale)'),
    ('std', 0x790, 'Platform::Math::JointMatrix, MultiplyByParent'),
    ('std', 0x818, 'Platform::Math::EulerRotation'),
    ('std', 0xA70, 'Platform::Math::TurnRotation'),
    ('std', 0xAB8, 'Platform::Math::StartRayTriangle'),
    ('cull', 0xA08, 'Platform::Math::ParticleBlockView'),
    ('cull', 0xB90, 'Platform::Math::ParticleBlockView (keeping the translation)'),
    ('cull', 0xCB0, 'Platform::Math::ViewDistance'),
]

FIELDS = 'xyzw'
FIELD_NAMES = ['Fx', 'Fy', 'Fz', 'Fw']
DEST_NAMES = {i: ''.join(c.upper() for bit, c in zip((8, 4, 2, 1), 'xyzw') if i & bit) for i in range(1, 16)}
FMAC_LATENCY = 4


class Unsupported(Exception):
    pass


def dest_name(dest):
    if dest == 0:
        raise Unsupported('empty dest')
    return DEST_NAMES[dest]


def bc_mask(bc):
    return 8 >> bc


class Op:
    """One half of an instruction pair: what it reads and writes for the timing, and its C++"""

    def __init__(self):
        self.pipe = 'none'      # none, fmac, fdiv, ialu, branch
        self.vf_write = 0       # PCSX2's VFwrite (0: none, or ACC)
        self.vf_write_mask = 0
        self.vf_reads = []      # [(reg, mask)] (PCSX2's VFread0, VFread1; reg 0 reads nothing)
        self.text = ''          # the disassembly
        self.emit = None        # function(sources) -> C++ statements; sources maps a read register to the one to read
        self.fdiv_cycles = 0    # DIV/SQRT (7), RSQRT (13): Q's new value (fdiv_value(sources) gives the expression)
        self.fdiv_value = None
        self.waitq = False
        self.vi_backup = None   # the integer register an integer ALU instruction writes (a branch right after reads its old value)
        self.vi_reads = []      # a branch's registers
        self.branch = None      # (kind, it, is, target)
        self.fmand = None       # (it, is)


def decode_upper(code, address):
    op = Op()
    ft = (code >> 16) & 31
    fs = (code >> 11) & 31
    fd = (code >> 6) & 31
    dest = (code >> 21) & 15
    low = code & 63
    op.text = vudis.upper(code)
    names = {0: 'Add', 1: 'Sub', 2: 'Madd', 3: 'Msub', 4: 'Max', 5: 'Mini', 6: 'Mul'}

    def fmac(write, reads, emit):
        op.pipe = 'fmac'
        op.vf_write, op.vf_write_mask = write
        op.vf_reads = reads
        op.emit = emit
        return op

    if low < 0x1C:
        name = names[low >> 2]
        bc = low & 3
        return fmac((fd, dest), [(fs, dest), (ft, bc_mask(bc))],
                    lambda s, n=name: ['vu.%sBc(%s, %s, %s, %s, %s);' % (n, dest_name(dest), fd, s(fs), s(ft), FIELD_NAMES[bc])])
    table = {0x1C: ('Mul', 'Q'), 0x1D: ('Max', 'I'), 0x1E: ('Mul', 'I'), 0x1F: ('Mini', 'I'), 0x20: ('Add', 'Q'),
             0x21: ('Madd', 'Q'), 0x22: ('Add', 'I'), 0x23: ('Madd', 'I'), 0x24: ('Sub', 'Q'), 0x25: ('Msub', 'Q'),
             0x26: ('Sub', 'I'), 0x27: ('Msub', 'I')}
    if low in table:
        name, kind = table[low]
        return fmac((fd, dest), [(fs, dest)],
                    lambda s: ['vu.%s%s(%s, %s, %s);' % (name, kind, dest_name(dest), fd, s(fs))])
    table = {0x28: 'Add', 0x29: 'Madd', 0x2A: 'Mul', 0x2B: 'Max', 0x2C: 'Sub', 0x2D: 'Msub', 0x2F: 'Mini'}
    if low in table:
        name = table[low]
        return fmac((fd, dest), [(fs, dest), (ft, dest)],
                    lambda s: ['vu.%s(%s, %s, %s, %s);' % (name, dest_name(dest), fd, s(fs), s(ft))])
    if low == 0x2E:
        return fmac((fd, 0xE), [(fs, 0xE), (ft, 0xE)], lambda s: ['vu.Opmsub(%s, %s, %s);' % (fd, s(fs), s(ft))])
    if low < 0x3C:
        raise Unsupported(op.text)
    special = (((code >> 6) & 31) << 2) | (code & 3)
    acc_names = {0: 'Adda', 1: 'Suba', 2: 'Madda', 3: 'Msuba', 6: 'Mula'}
    if special < 0x10 or 0x18 <= special < 0x1C:
        name = acc_names[special >> 2]
        bc = special & 3
        return fmac((0, dest), [(fs, dest), (ft, bc_mask(bc))],
                    lambda s: ['vu.%sBc(%s, %s, %s, %s);' % (name, dest_name(dest), s(fs), s(ft), FIELD_NAMES[bc])])
    table = {0x1C: ('Mula', 'Q'), 0x1E: ('Mula', 'I'), 0x20: ('Adda', 'Q'), 0x21: ('Madda', 'Q'), 0x22: ('Adda', 'I'),
             0x23: ('Madda', 'I'), 0x24: ('Suba', 'Q'), 0x25: ('Msuba', 'Q'), 0x26: ('Suba', 'I'), 0x27: ('Msuba', 'I')}
    if special in table:
        name, kind = table[special]
        return fmac((0, dest), [(fs, dest)], lambda s: ['vu.%s%s(%s, %s);' % (name, kind, dest_name(dest), s(fs))])
    table = {0x28: 'Adda', 0x29: 'Madda', 0x2A: 'Mula', 0x2C: 'Suba', 0x2D: 'Msuba'}
    if special in table:
        name = table[special]
        return fmac((0, dest), [(fs, dest), (ft, dest)],
                    lambda s: ['vu.%s(%s, %s, %s);' % (name, dest_name(dest), s(fs), s(ft))])
    if special == 0x2E:
        return fmac((0, 0xE), [(fs, 0xE), (ft, 0xE)], lambda s: ['vu.Opmula(%s, %s);' % (s(fs), s(ft))])
    if special == 0x1D:
        return fmac((ft, dest), [(fs, dest)], lambda s: ['vu.Abs(%s, %s, %s);' % (dest_name(dest), ft, s(fs))])
    if 0x10 <= special < 0x18:
        bits = (0, 4, 12, 15)[special & 3]
        name = 'Itof' if special < 0x14 else 'Ftoi'
        return fmac((ft, dest), [(fs, dest)], lambda s: ['vu.%s(%s, %s, %s, %s);' % (name, dest_name(dest), ft, s(fs), bits)])
    if special == 0x2F:
        op.emit = lambda s: []
        return op
    raise Unsupported(op.text)


def decode_lower(code, address):
    op = Op()
    ft = (code >> 16) & 31
    fs = (code >> 11) & 31
    fd = (code >> 6) & 31
    it, is_, id_ = ft & 15, fs & 15, fd & 15
    dest = (code >> 21) & 15
    fsf = (code >> 21) & 3
    ftf = (code >> 23) & 3
    op.text = vudis.lower(code, address)
    top = code >> 25
    target = (address + 8 + vudis.imm11(code) * 8) & 0xFFF

    def ialu(reg, emit):
        op.pipe = 'ialu'
        op.vi_backup = reg if reg != 0 else None
        op.emit = emit
        return op

    if top == 0x40:
        low = code & 63
        if low in (0x30, 0x31, 0x34, 0x35):
            name = {0x30: 'Iadd', 0x31: 'Isub', 0x34: 'Iand', 0x35: 'Ior'}[low]
            return ialu(id_, lambda s: ['vu.%s(%s, %s, %s);' % (name, id_, is_, it)])
        if low == 0x32:
            imm5 = (code >> 6) & 31
            imm5 = imm5 - 32 if imm5 & 16 else imm5
            return ialu(it, lambda s: ['vu.Iaddi(%s, %s, %s);' % (it, is_, imm5)])
        if low >= 0x3C:
            sub = (code >> 6) & 31
            kind = code & 3
            key = (kind, sub)
            if key == (0, 0x0C):
                if dest == 0:
                    op.emit = lambda s: []
                    return op
                op.pipe = 'fmac' if ft != 0 else 'none'
                op.vf_write, op.vf_write_mask = ft, dest
                op.vf_reads = [(fs, dest)]
                op.emit = lambda s: ['vu.Move(%s, %s, %s);' % (dest_name(dest), ft, s(fs))]
                return op
            if key == (1, 0x0C):
                op.pipe = 'fmac'
                op.vf_write, op.vf_write_mask = ft, dest
                op.vf_reads = [(fs, ((dest >> 1) | ((dest << 3) & 8)))]
                op.emit = lambda s: ['vu.Mr32(%s, %s, %s);' % (dest_name(dest), ft, s(fs))]
                return op
            if key == (0, 0x0E):
                op.pipe = 'fdiv'
                op.vf_reads = [(fs, bc_mask(fsf)), (ft, bc_mask(ftf))]
                op.fdiv_cycles = 7
                op.fdiv_value = lambda s: 'vu.DivValue(%s, %s, %s, %s)' % (s(fs), FIELD_NAMES[fsf], s(ft), FIELD_NAMES[ftf])
                return op
            if key == (1, 0x0E):
                op.pipe = 'fdiv'
                op.vf_reads = [(ft, bc_mask(ftf))]
                op.fdiv_cycles = 7
                op.fdiv_value = lambda s: 'vu.SqrtValue(%s, %s)' % (s(ft), FIELD_NAMES[ftf])
                return op
            if key == (2, 0x0E):
                op.pipe = 'fdiv'
                op.vf_reads = [(fs, bc_mask(fsf)), (ft, bc_mask(ftf))]
                op.fdiv_cycles = 13
                op.fdiv_value = lambda s: 'vu.RsqrtValue(%s, %s, %s, %s)' % (s(fs), FIELD_NAMES[fsf], s(ft), FIELD_NAMES[ftf])
                return op
            if key == (3, 0x0E):
                op.pipe = 'fdiv'
                op.waitq = True
                op.emit = lambda s: []
                return op
            if key == (2, 0x10):
                op.pipe = 'fmac'
                op.vf_reads = [(fs, bc_mask(fsf))]
                op.emit = lambda s: ['vu.Rinit(%s, %s);' % (s(fs), FIELD_NAMES[fsf])]
                return op
            if key == (1, 0x10):
                op.pipe = 'fmac'
                op.vf_write, op.vf_write_mask = ft, dest
                op.emit = lambda s: ['vu.Rget(%s, %s);' % (dest_name(dest), ft)]
                return op
            if key == (3, 0x0B):
                op.emit = lambda s: []
                return op
        raise Unsupported(op.text)
    imm11 = vudis.imm11(code)
    if top == 0x00:
        op.pipe = 'fmac'
        op.vf_write, op.vf_write_mask = ft, dest
        op.emit = lambda s: ['vu.Lq(%s, %s, %s, %s);' % (dest_name(dest), ft, is_, imm11)]
        return op
    if top == 0x01:
        op.pipe = 'fmac'
        op.vf_reads = [(fs, dest)]
        op.emit = lambda s: ['vu.Sq(%s, %s, %s, %s);' % (dest_name(dest), s(fs), it, imm11)]
        return op
    if top in (0x08, 0x09):
        imm15 = ((code >> 10) & 0x7800) | (code & 0x7FF)
        name = ('Iaddiu', 'Isubiu')[top - 8]
        return ialu(it, lambda s: ['vu.%s(%s, %s, 0x%X);' % (name, it, is_, imm15)])
    if top == 0x1A:
        op.pipe = 'fmac'
        op.fmand = (it, is_)
        return op
    if top in (0x20, 0x28, 0x29, 0x2C, 0x2D, 0x2E, 0x2F):
        op.pipe = 'branch'
        kind = {0x20: 'b', 0x28: 'ibeq', 0x29: 'ibne', 0x2C: 'ibltz', 0x2D: 'ibgtz', 0x2E: 'iblez', 0x2F: 'ibgez'}[top]
        op.branch = (kind, it, is_, target)
        return op
    raise Unsupported(op.text)


class Instruction:
    def __init__(self, image, address):
        self.address = address
        low, up = struct.unpack('<2I', image[address])
        self.ibit = bool(up >> 31)
        self.ebit = bool(up >> 30 & 1)
        if up >> 27 & 7:
            raise Unsupported('M, D or T bit at %03X' % address)
        self.upper = decode_upper(up, address)
        self.immediate = low if self.ibit else None
        self.lower = None if self.ibit else decode_lower(low, address)
        self.text = vudis.disassemble(image, address)


class Entry:
    def __init__(self, start, upper, lower, snapshot):
        self.start = start
        self.writes = [w for w in (upper, lower) if w is not None]
        self.snapshot = snapshot


class State:
    def __init__(self):
        self.cycle = 0
        self.fmac = []          # pending FMAC entries, oldest first
        self.fdiv = None        # (start, cycles, variable)
        self.mac_visible = 'vu.mac'
        self.backup_cycles = 0
        self.backup_reg = None
        self.backup_var = None
        self.branch = None      # (countdown, target, condition variable or None)
        self.ebit = 0

    def copy(self):
        other = State()
        other.__dict__.update(self.__dict__)
        other.fmac = list(self.fmac)
        return other


def stall_fmac(state, reads):
    for entry in state.fmac:
        if state.cycle - entry.start >= FMAC_LATENCY:
            continue
        for reg, mask in reads:
            if reg == 0:
                continue
            for wreg, wmask in entry.writes:
                if wreg == reg and wmask & mask:
                    state.cycle = max(state.cycle, entry.start + FMAC_LATENCY)


def flush(state, out):
    """PCSX2's _vuTestPipes: the FMAC entries 4 cycles old (in order) and a finished FDIV written"""
    while state.fmac and state.cycle - state.fmac[0].start >= FMAC_LATENCY:
        entry = state.fmac.pop(0)
        state.mac_visible = entry.snapshot
        out.append(('mac', entry.snapshot))
    if state.fdiv is not None and state.cycle - state.fdiv[0] >= state.fdiv[1]:
        out.append(('stmt', 'vu.q = %s;' % state.fdiv[2]))
        state.fdiv = None


def simulate(image, start):
    """Every path: a list of (address, [statements]) per instruction run, and the instructions"""
    instructions = {}
    paths = []

    def instruction(address):
        if address not in instructions:
            if address not in image:
                raise Unsupported('runs off the set at %03X' % address)
            instructions[address] = Instruction(image, address)
        return instructions[address]

    def run(state, address, trace, decisions):
        while True:
            if len(trace) > 1000:
                raise Unsupported('a loop')
            inst = instruction(address)
            out = []
            state.cycle += 1
            before = state.cycle - 1
            up, low = inst.upper, inst.lower
            if inst.ebit:
                state.ebit = 2
            if up.pipe == 'fmac':
                stall_fmac(state, up.vf_reads)
            if low is not None:
                if low.pipe == 'fmac':
                    stall_fmac(state, low.vf_reads)
                elif low.pipe == 'fdiv':
                    stall_fmac(state, low.vf_reads)
                    if state.fdiv is not None:
                        state.cycle = max(state.cycle, state.fdiv[0] + state.fdiv[1])
            flush(state, out)
            if state.backup_cycles > 0:
                state.backup_cycles -= min(state.cycle - before, state.backup_cycles)

            # The lower instruction reads the old value of a register the upper one writes; both writing one drops the lower
            sources = {}
            drop_lower = False
            if low is not None and up.vf_write != 0:
                if low.vf_write == up.vf_write:
                    drop_lower = True
                elif any(reg == up.vf_write for reg, _ in low.vf_reads):
                    sources[up.vf_write] = 'Vu0::Temporary'
            upper_source = lambda reg: str(reg)
            lower_source = lambda reg: str(sources.get(reg, reg))
            out.append(('stmt', '// ' + inst.text))
            if sources:
                out.append(('stmt', 'vu.vf[Vu0::Temporary] = vu.vf[%d]; // the lower instruction reads it as it was' % up.vf_write))
            for line in up.emit(upper_source):
                out.append(('stmt', line))
            if inst.ibit:
                out.append(('stmt', 'vu.i = 0x%08X;' % inst.immediate))
            elif drop_lower:
                out.append(('stmt', '// (the lower instruction writes the upper one\'s register: dropped)'))
            else:
                if low.vi_backup is not None:
                    if not (state.backup_cycles and state.backup_reg == low.vi_backup):
                        var = 'vi%02dBefore%03X' % (low.vi_backup, address)
                        state.backup_var = var
                        state.backup_reg = low.vi_backup
                        out.append(('decl', 'u16 %s;' % var))
                        out.append(('backup', '%s = vu.vi[%d];' % (var, low.vi_backup), var))
                    state.backup_cycles = 2
                if low.fmand is not None:
                    out.append(('fmand', 'vu.Fmand(%d, %d, %%s);' % low.fmand, state.mac_visible))
                elif low.fdiv_value is not None:
                    var = 'q%03X' % address
                    out.append(('decl', 'u32 %s;' % var))
                    out.append(('stmt', '%s = %s;' % (var, low.fdiv_value(lower_source))))
                elif low.branch is not None:
                    if state.branch is not None:
                        raise Unsupported('a branch in a delay slot at %03X' % address)
                    kind, it, is_, target = low.branch
                    if kind == 'b':
                        state.branch = (2, target, None)
                    else:
                        def value(reg):
                            if state.backup_cycles > 0 and state.backup_reg == reg:
                                out.append(('use', state.backup_var))
                                return 'static_cast<s16>(%s)' % state.backup_var
                            return 'static_cast<s16>(vu.vi[%d])' % reg
                        condition = {'ibeq': '%s == %s', 'ibne': '%s != %s'}
                        if kind in condition:
                            text = condition[kind] % (value(it), value(is_))
                        else:
                            text = {'ibltz': '%s < 0', 'ibgtz': '%s > 0', 'iblez': '%s <= 0', 'ibgez': '%s >= 0'}[kind] % value(is_)
                        var = 'taken%03X' % address
                        out.append(('decl', 'bool %s;' % var))
                        out.append(('stmt', '%s = %s;' % (var, text)))
                        state.branch = (2, target, var)
                elif low.emit is not None:
                    for line in low.emit(lower_source):
                        out.append(('stmt', line))
            # The pipelines' new entries
            if up.pipe == 'fmac' or (low is not None and low.pipe == 'fmac'):
                snapshot = 'mac%03X' % address
                upper_write = (up.vf_write, up.vf_write_mask) if up.pipe == 'fmac' else None
                lower_write = (low.vf_write, low.vf_write_mask) if (low is not None and low.pipe == 'fmac') else None
                state.fmac.append(Entry(state.cycle, upper_write, lower_write, snapshot))
                out.append(('decl', 'u32 %s;' % snapshot))
                out.append(('snapshot', '%s = vu.mac;' % snapshot, snapshot))
            if low is not None and low.fdiv_cycles:
                state.fdiv = (state.cycle, low.fdiv_cycles, 'q%03X' % address)
            trace.append((address, out))
            # Branches and the end
            next_address = address + 8
            jump = None
            if state.branch is not None:
                countdown, target, var = state.branch
                if countdown == 1:
                    jump = (target, var)
                    state.branch = None
                else:
                    state.branch = (countdown - 1, target, var)
            if state.ebit:
                state.ebit -= 1
                if state.ebit == 0:
                    end = []
                    if state.fdiv is not None:
                        end.append(('stmt', 'vu.q = %s;' % state.fdiv[2]))
                    if state.branch is not None or jump is not None:
                        raise Unsupported('a branch at the end')
                    trace.append(('end', end))
                    paths.append((decisions, trace))
                    return
            if jump is not None:
                target, var = jump
                if var is None:
                    address = target
                    trace.append(('goto', target))
                    continue
                taken = state.copy()
                run(taken, target, trace + [('if', var, target, True)], decisions + [(var, True)])
                trace.append(('if', var, target, False))
                decisions = decisions + [(var, False)]
            address = next_address

    run(State(), start, [], [])
    return instructions, paths


def resolve_path(trace):
    """The FMANDs' MAC flags on a path (the last flushed snapshot before each), and the snapshots and old integer values it
    needs"""
    needed = set()
    used_backups = set()
    visible = 'vu.mac'
    resolved = []
    for step in trace:
        if step[0] in ('end', 'goto', 'if'):
            resolved.append(step)
            continue
        address, out = step
        new_out = []
        for item in out:
            if item[0] == 'mac':
                visible = item[1]
                continue
            if item[0] == 'fmand':
                needed.add(visible)
                new_out.append(('stmt', item[1] % visible))
                continue
            if item[0] == 'use':
                used_backups.add(item[1])
                continue
            new_out.append(item)
        resolved.append((address, new_out))
    return resolved, needed, used_backups


def finish_path(resolved, needed, used_backups):
    """Dead stores out: a MAC snapshot only where an FMAND on some path reads it, an old integer value where a branch does"""
    lines = []
    for step in resolved:
        if step[0] in ('end', 'goto', 'if'):
            lines.append(step)
            continue
        address, out = step
        kept = []
        for item in out:
            if item[0] == 'snapshot':
                if item[2] in needed:
                    kept.append(('stmt', item[1]))
            elif item[0] == 'backup':
                if item[2] in used_backups:
                    kept.append(('stmt', item[1]))
            elif item[0] == 'decl':
                name = item[1].split()[1].rstrip(';')
                if name.startswith('mac') and name not in needed:
                    continue
                if name.startswith('vi') and name not in used_backups:
                    continue
                kept.append(item)
            else:
                kept.append(item)
        lines.append((address, kept))
    return lines


def translate(set_name, start, caller, image):
    instructions, paths = simulate(image, start)
    # An instruction's translation has to be the same on every path through it
    per_address = {}
    ends = []
    decls = []
    resolved_paths = []
    needed = set()
    used_backups = set()
    for decisions, trace in paths:
        resolved, path_needed, path_backups = resolve_path(trace)
        resolved_paths.append(resolved)
        needed |= path_needed
        used_backups |= path_backups
    for resolved in resolved_paths:
        for step in finish_path(resolved, needed, used_backups):
            if step[0] in ('goto', 'if'):
                continue
            if step[0] == 'end':
                ends.append(tuple(step[1]))
                continue
            address, out = step
            body = tuple(item[1] for item in out if item[0] == 'stmt')
            for item in out:
                if item[0] == 'decl' and item[1] not in decls:
                    decls.append(item[1])
            if address in per_address and per_address[address] != body:
                raise Unsupported('%s %03X: the instruction at %03X differs between paths:\n%s\n%s' % (
                    set_name, start, address, '\n'.join(per_address[address]), '\n'.join(body)))
            per_address[address] = body
    if len(set(ends)) != 1:
        raise Unsupported('the paths end differently')
    end = ends[0]

    # The instructions in address order; a branch's way out after its delay slot, the end after the E bit's delay slot
    order = sorted(per_address)
    branches = {}
    end_after = None
    for decisions, trace in paths:
        previous = None
        for step in trace:
            if step[0] == 'if':
                _, var, target, taken = step
                branches[previous] = (var, target)
            elif step[0] == 'goto':
                branches[previous] = (None, step[1])
            elif step[0] == 'end':
                end_after = previous if end_after is None else end_after
                if previous != end_after:
                    raise Unsupported('more than one end')
            else:
                previous = step[0]
    targets = {target for _, target in branches.values()}
    name = 'Microprogram%s%03X' % (set_name.capitalize(), start)
    lines = ['// %s at 0x%03X (%s)' % ({'std': 'The standard set\'s program', 'cull': 'The culling set\'s program',
                                       'decal': 'The decal set\'s program'}[set_name], start, caller),
             'void %s(Vu0& vu)' % name, '{']
    for decl in decls:
        lines.append('    ' + decl)
    if decls:
        lines.append('')
    for index, address in enumerate(order):
        if address in targets or (index > 0 and order[index - 1] != address - 8):
            lines.append('L%03X:' % address)
        for statement in per_address[address]:
            lines.append('    ' + statement)
        if address in branches:
            var, target = branches[address]
            if var is None:
                lines.append('    goto L%03X;' % target)
            else:
                lines.append('    if (%s)' % var)
                lines.append('    {')
                lines.append('        goto L%03X;' % target)
                lines.append('    }')
        if address == end_after:
            lines.append('    // The end: what\'s still in the pipelines written')
            for kind, statement in end:
                lines.append('    ' + statement)
            lines.append('    return;')
        elif index + 1 < len(order) and order[index + 1] != address + 8 and address not in branches:
            lines.append('    goto L%03X;' % (address + 8))
    lines.append('}')
    # The instructions the program can run (for the check that its set's code is loaded)
    ranges = []
    for address in order:
        if ranges and ranges[-1][1] == address // 8:
            ranges[-1][1] += 1
        else:
            ranges.append([address // 8, address // 8 + 1])
    return name, lines, ranges


def main():
    sets = vudis.load_sets()
    images = {name: vudis.set_image(loads) for name, loads in sets.items()}
    out = ['// Made by native/math-tests/tools/vu0translate.py from the retail executable\'s VU0 microcode (assets/vutext.textbin.bin):',
           '// do not edit. Each of the maths\' VU0 microprograms an instruction pair at a time on the native VU0, with the pipelines\'',
           '// timing as PCSX2\'s VU0 interpreter runs it worked out ahead of time (see the tool and native/MATH.md): a read of Q or of',
           '// the MAC flags reads the variable holding the value it gets, a lower instruction reading its upper instruction\'s',
           '// register reads its old value from Vu0::Temporary, branches go to their targets after their delay slots.',
           '',
           '#include "microprograms.h"',
           '',
           'namespace NativeMath',
           '{',
           'namespace',
           '{']
    table = []
    for set_name, start, caller in PROGRAMS:
        name, lines, ranges = translate(set_name, start, caller, images[set_name])
        # The sets whose code is the same over every instruction the program runs
        valid = []
        for other, image in images.items():
            if all(all(a * 8 in image and image[a * 8] == images[set_name][a * 8] for a in range(first, end))
                   for first, end in ranges):
                valid.append(vudis.SET_NUMBERS[other])
        out.extend(lines)
        out.append('')
        out.append('const MicroInstructionRange %sRanges[] = {%s};' % (
            name, ', '.join('{0x%X, 0x%X}' % (first, end) for first, end in ranges)))
        out.append('')
        table.append((start, sum(1 << n for n in valid), name, len(ranges)))
    for set_name, loads in sets.items():
        out.append('const MicroInstructionRange %sLoads[] = {%s};' % (
            set_name.capitalize(), ', '.join('{0x%X, 0x%X}' % (a // 8, (a + len(c)) // 8) for a, c in loads)))
    out.append('')
    out.append('const Microprogram s_Programs[] = {')
    for start, mask, name, count in table:
        out.append('    {0x%03X, 0x%X, %sRanges, %d, %s},' % (start, mask, name, count, name))
    out.append('};')
    out.append('')
    out.append('const MicrocodeSetLoads s_SetLoads[] = {')
    for set_name, loads in sets.items():
        out.append('    {%d, %sLoads, %d},' % (vudis.SET_NUMBERS[set_name], set_name.capitalize(), len(loads)))
    out.append('};')
    out.append('}')
    out.append('')
    out.append('const Microprogram* FindMicroprogram(u32 address)')
    out.append('{')
    out.append('    for (const Microprogram& program : s_Programs)')
    out.append('    {')
    out.append('        if (program.address == address)')
    out.append('        {')
    out.append('            return &program;')
    out.append('        }')
    out.append('    }')
    out.append('')
    out.append('    return nullptr;')
    out.append('}')
    out.append('')
    out.append('const MicrocodeSetLoads* MicrocodeSetLoadsOf(u32 set)')
    out.append('{')
    out.append('    for (const MicrocodeSetLoads& loads : s_SetLoads)')
    out.append('    {')
    out.append('        if (loads.set == set)')
    out.append('        {')
    out.append('            return &loads;')
    out.append('        }')
    out.append('    }')
    out.append('')
    out.append('    return nullptr;')
    out.append('}')
    out.append('')
    out.append('const Microprogram* Microprograms(u32* count)')
    out.append('{')
    out.append('    *count = sizeof(s_Programs) / sizeof(s_Programs[0]);')
    out.append('    return s_Programs;')
    out.append('}')
    out.append('}')
    with open(OUTPUT, 'w') as f:
        f.write('\n'.join(out) + '\n')
    print('wrote', OUTPUT)


if __name__ == '__main__':
    main()
