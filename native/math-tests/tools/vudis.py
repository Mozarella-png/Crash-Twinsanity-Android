#!/usr/bin/env python3
"""VU0 microcode: the three microcode sets the retail executable loads into VU0 (read from the split's .vutext,
assets/vutext.textbin.bin, through their DMA chains' MPG codes), and a disassembler of them.

    vudis.py                       the sets' layout
    vudis.py std 0x1C0 [0x548]     a set's instructions from an address (to another, or to its end: the E bit's delay slot)
    vudis.py --header out.h        the sets as a C++ header (the tests' reference interpreter runs them)

Nothing here is taken from another disassembler: the encodings are the VU's (Sony's VU User's Manual, as PCSX2's VUops.cpp
decodes them)."""
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
VUTEXT = os.path.join(ROOT, 'assets', 'vutext.textbin.bin')
VUTEXT_ADDRESS = 0x2D9D90
# The DMA chains to VIF0 the PS2 side's vu0programs.cpp names (g_StandardVu0Programs, g_CullingVu0Programs, g_DecalVu0Programs)
CHAINS = {'std': 0x2E6110, 'cull': 0x2E4EB0, 'decal': 0x2E5BD0}
SET_NUMBERS = {'std': 1, 'cull': 2, 'decal': 3}


def load_sets():
    data = open(VUTEXT, 'rb').read()
    sets = {}
    for name, chain in CHAINS.items():
        offset = chain - VUTEXT_ADDRESS
        tag = struct.unpack_from('<4I', data, offset)
        qwc = tag[0] & 0xFFFF
        tag_id = (tag[0] >> 28) & 7
        assert tag_id == 1, 'a CNT tag'
        end = offset + 16 + qwc * 16
        # The tag's upper half is VIF codes, then its data
        position = offset + 8
        loads = []
        while position < end:
            code = struct.unpack_from('<I', data, position)[0]
            position += 4
            command = (code >> 24) & 0x7F
            if command == 0:
                continue
            assert command == 0x4A, 'only MPG codes: %x' % code
            count = (code >> 16) & 0xFF or 256
            address = (code & 0xFFFF) * 8
            assert position % 8 == 0
            loads.append((address, data[position:position + count * 8]))
            position += count * 8
        # The next tag: END
        assert (struct.unpack_from('<I', data, end)[0] >> 28) & 7 == 7
        sets[name] = loads
    return sets


def set_image(loads):
    """The micro memory a set writes: address -> 8 bytes"""
    image = {}
    for address, code in loads:
        for index in range(len(code) // 8):
            image[address + index * 8] = code[index * 8:index * 8 + 8]
    return image


BC = 'xyzw'


def dest(code):
    return ''.join(c for bit, c in zip((24, 23, 22, 21), 'xyzw') if code >> bit & 1)


def f32(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def upper(code):
    ft = (code >> 16) & 31
    fs = (code >> 11) & 31
    fd = (code >> 6) & 31
    d = dest(code)
    flags = ''.join(f for bit, f in ((31, 'i'), (30, 'e'), (29, 'm'), (28, 'd'), (27, 't')) if code >> bit & 1)
    flags = '[%s]' % flags if flags else ''
    op = code & 63
    bc = BC[code & 3]
    if op < 0x3C:
        names = {0: 'add', 1: 'sub', 2: 'madd', 3: 'msub', 4: 'max', 5: 'mini', 6: 'mul'}
        if op < 0x1C:
            return '%s%s%s.%s vf%02d, vf%02d, vf%02d%s' % (names[op >> 2], bc, flags, d, fd, fs, ft, bc)
        table = {0x1C: ('mulq', 'q'), 0x1D: ('maxi', 'i'), 0x1E: ('muli', 'i'), 0x1F: ('minii', 'i'),
                 0x20: ('addq', 'q'), 0x21: ('maddq', 'q'), 0x22: ('addi', 'i'), 0x23: ('maddi', 'i'),
                 0x24: ('subq', 'q'), 0x25: ('msubq', 'q'), 0x26: ('subi', 'i'), 0x27: ('msubi', 'i')}
        if op in table:
            name, reg = table[op]
            return '%s%s.%s vf%02d, vf%02d, %s' % (name, flags, d, fd, fs, reg)
        table = {0x28: 'add', 0x29: 'madd', 0x2A: 'mul', 0x2B: 'max', 0x2C: 'sub', 0x2D: 'msub', 0x2E: 'opmsub', 0x2F: 'mini'}
        if op in table:
            return '%s%s.%s vf%02d, vf%02d, vf%02d' % (table[op], flags, d, fd, fs, ft)
        return '?upper %08x' % code
    special = (((code >> 6) & 31) << 2) | (code & 3)
    if special < 0x10 or 0x18 <= special < 0x1C:
        names = {0: 'adda', 1: 'suba', 2: 'madda', 3: 'msuba', 6: 'mula'}
        return '%s%s%s.%s ACC, vf%02d, vf%02d%s' % (names[special >> 2], bc, flags, d, fs, ft, bc)
    if 0x10 <= special < 0x18:
        name = ('itof', 'ftoi')[(special - 0x10) >> 2] + ('0', '4', '12', '15')[special & 3]
        return '%s%s.%s vf%02d, vf%02d' % (name, flags, d, ft, fs)
    table = {0x1C: ('mulaq', 'q'), 0x1E: ('mulai', 'i'), 0x20: ('addaq', 'q'), 0x21: ('maddaq', 'q'), 0x22: ('addai', 'i'),
             0x23: ('maddai', 'i'), 0x24: ('subaq', 'q'), 0x25: ('msubaq', 'q'), 0x26: ('subai', 'i'), 0x27: ('msubai', 'i')}
    if special in table:
        name, reg = table[special]
        return '%s%s.%s ACC, vf%02d, %s' % (name, flags, d, fs, reg)
    table = {0x28: 'adda', 0x29: 'madda', 0x2A: 'mula', 0x2C: 'suba', 0x2D: 'msuba', 0x2E: 'opmula'}
    if special in table:
        return '%s%s.%s ACC, vf%02d, vf%02d' % (table[special], flags, d, fs, ft)
    if special == 0x1D:
        return 'abs%s.%s vf%02d, vf%02d' % (flags, d, ft, fs)
    if special == 0x1F:
        return 'clipw%s.xyz vf%02d, vf%02d.w' % (flags, fs, ft)
    if special == 0x2F:
        return 'nop%s' % flags
    return '?upper %08x' % code


def imm11(code):
    value = code & 0x7FF
    return value - 0x800 if value & 0x400 else value


def lower(code, address):
    ft = (code >> 16) & 31
    fs = (code >> 11) & 31
    fd = (code >> 6) & 31
    it, is_, id_ = ft & 15, fs & 15, fd & 15
    d = dest(code)
    op = code >> 25
    target = lambda: '0x%03X' % ((address + 8 + imm11(code) * 8) & 0xFFF)
    if op == 0x40:
        low = code & 63
        if low == 0x30:
            return 'iadd vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if low == 0x31:
            return 'isub vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if low == 0x32:
            imm5 = (code >> 6) & 31
            imm5 = imm5 - 32 if imm5 & 16 else imm5
            return 'iaddi vi%02d, vi%02d, %d' % (it, is_, imm5)
        if low == 0x34:
            return 'iand vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if low == 0x35:
            return 'ior vi%02d, vi%02d, vi%02d' % (id_, is_, it)
        if low >= 0x3C:
            sub = (code >> 6) & 31
            kind = code & 3
            fsf, ftf = BC[(code >> 21) & 3], BC[(code >> 23) & 3]
            tables = {
                0: {0x0C: ('move.%s vf%02d, vf%02d' % (d, ft, fs)) if d else 'nop', 0x0D: 'lqi.%s vf%02d, (vi%02d++)' % (d, ft, is_),
                    0x0E: 'div Q, vf%02d.%s, vf%02d.%s' % (fs, fsf, ft, ftf), 0x0F: 'mtir vi%02d, vf%02d.%s' % (it, fs, fsf),
                    0x10: 'rnext.%s vf%02d, R' % (d, ft), 0x19: 'mfp.%s vf%02d, P' % (d, ft), 0x1A: 'xtop vi%02d' % it,
                    0x1B: 'xgkick vi%02d' % is_, 0x1C: 'esadd P, vf%02d' % fs, 0x1D: 'eatanxy P, vf%02d' % fs,
                    0x1E: 'esqrt P, vf%02d.%s' % (fs, fsf), 0x1F: 'esin P, vf%02d.%s' % (fs, fsf)},
                1: {0x0C: 'mr32.%s vf%02d, vf%02d' % (d, ft, fs), 0x0D: 'sqi.%s vf%02d, (vi%02d++)' % (d, fs, it),
                    0x0E: 'sqrt Q, vf%02d.%s' % (ft, ftf), 0x0F: 'mfir.%s vf%02d, vi%02d' % (d, ft, is_),
                    0x10: 'rget.%s vf%02d, R' % (d, ft), 0x1A: 'xitop vi%02d' % it, 0x1C: 'ersadd P, vf%02d' % fs,
                    0x1D: 'eatanxz P, vf%02d' % fs, 0x1E: 'ersqrt P, vf%02d.%s' % (fs, fsf), 0x1F: 'eatan P, vf%02d.%s' % (fs, fsf)},
                2: {0x0D: 'lqd.%s vf%02d, (--vi%02d)' % (d, ft, is_), 0x0E: 'rsqrt Q, vf%02d.%s, vf%02d.%s' % (fs, fsf, ft, ftf),
                    0x0F: 'ilwr.%s vi%02d, (vi%02d)' % (d, it, is_), 0x10: 'rinit R, vf%02d.%s' % (fs, fsf),
                    0x1C: 'eleng P, vf%02d' % fs, 0x1D: 'esum P, vf%02d' % fs, 0x1E: 'ercpr P, vf%02d.%s' % (fs, fsf),
                    0x1F: 'eexp P, vf%02d.%s' % (fs, fsf)},
                3: {0x0B: 'nop', 0x0D: 'sqd.%s vf%02d, (--vi%02d)' % (d, fs, it), 0x0E: 'waitq',
                    0x0F: 'iswr.%s vi%02d, (vi%02d)' % (d, it, is_), 0x10: 'rxor R, vf%02d.%s' % (fs, fsf),
                    0x1C: 'erleng P, vf%02d' % fs, 0x1E: 'waitp'},
            }
            return tables[kind].get(sub, '?lower %08x' % code)
        return '?lower %08x' % code
    imm = imm11(code)
    if op == 0x00:
        return 'lq.%s vf%02d, %d(vi%02d)' % (d, ft, imm, is_)
    if op == 0x01:
        return 'sq.%s vf%02d, %d(vi%02d)' % (d, fs, imm, it)
    if op == 0x04:
        return 'ilw.%s vi%02d, %d(vi%02d)' % (d, it, imm, is_)
    if op == 0x05:
        return 'isw.%s vi%02d, %d(vi%02d)' % (d, it, imm, is_)
    if op in (0x08, 0x09):
        imm15 = ((code >> 10) & 0x7800) | (code & 0x7FF)
        return '%s vi%02d, vi%02d, 0x%X' % (('iaddiu', 'isubiu')[op - 8], it, is_, imm15)
    imm24 = code & 0xFFFFFF
    imm12 = ((code >> 10) & 0x800) | (code & 0x7FF)
    table = {0x10: 'fceq vi01, 0x%06X' % imm24, 0x11: 'fcset 0x%06X' % imm24, 0x12: 'fcand vi01, 0x%06X' % imm24,
             0x13: 'fcor vi01, 0x%06X' % imm24, 0x14: 'fseq vi%02d, 0x%03X' % (it, imm12), 0x15: 'fsset 0x%03X' % imm12,
             0x16: 'fsand vi%02d, 0x%03X' % (it, imm12), 0x17: 'fsor vi%02d, 0x%03X' % (it, imm12),
             0x18: 'fmeq vi%02d, vi%02d' % (it, is_), 0x1A: 'fmand vi%02d, vi%02d' % (it, is_),
             0x1B: 'fmor vi%02d, vi%02d' % (it, is_), 0x1C: 'fcget vi%02d' % it,
             0x20: 'b %s' % target(), 0x21: 'bal vi%02d, %s' % (it, target()), 0x24: 'jr vi%02d' % is_,
             0x25: 'jalr vi%02d, vi%02d' % (it, is_), 0x28: 'ibeq vi%02d, vi%02d, %s' % (it, is_, target()),
             0x29: 'ibne vi%02d, vi%02d, %s' % (it, is_, target()), 0x2C: 'ibltz vi%02d, %s' % (is_, target()),
             0x2D: 'ibgtz vi%02d, %s' % (is_, target()), 0x2E: 'iblez vi%02d, %s' % (is_, target()),
             0x2F: 'ibgez vi%02d, %s' % (is_, target())}
    return table.get(op, '?lower %08x' % code)


def disassemble(image, address):
    low, up = struct.unpack('<2I', image[address])
    text_upper = upper(up)
    if up >> 31:
        text_lower = 'loi 0x%08X (%r)' % (low, f32(low))
    else:
        text_lower = lower(low, address)
    return '%03X: %08X %08X  %-44s %s' % (address, up, low, text_upper, text_lower)


def main():
    sets = load_sets()
    if len(sys.argv) >= 3 and sys.argv[1] == '--header':
        with open(sys.argv[2], 'w') as out:
            out.write('// Made by native/math-tests/tools/vudis.py from assets/vutext.textbin.bin: the VU0 microcode sets\n')
            out.write('#pragma once\n#include <cstdint>\n\nstruct MicrocodeLoad { uint32_t set; uint32_t address; uint32_t count; '
                      'const uint32_t* words; };\n\n')
            loads = []
            for name, parts in sets.items():
                for index, (address, code) in enumerate(parts):
                    words = struct.unpack('<%dI' % (len(code) // 4), code)
                    label = 'g_%s%d' % (name, index)
                    out.write('inline const uint32_t %s[] = {\n' % label)
                    for start in range(0, len(words), 8):
                        out.write('    ' + ', '.join('0x%08X' % w for w in words[start:start + 8]) + ',\n')
                    out.write('};\n')
                    loads.append((SET_NUMBERS[name], address, len(code) // 8, label))
            out.write('inline const MicrocodeLoad g_MicrocodeLoads[] = {\n')
            for load in loads:
                out.write('    {%d, 0x%X, %d, %s},\n' % load)
            out.write('};\n')
        return
    if len(sys.argv) < 3:
        for name, parts in sets.items():
            print(name, ', '.join('0x%03X-0x%03X' % (a, a + len(c)) for a, c in parts))
        return
    image = set_image(sets[sys.argv[1]])
    address = int(sys.argv[2], 16)
    stop = int(sys.argv[3], 16) if len(sys.argv) > 3 else None
    ending = None
    while address in image:
        print(disassemble(image, address))
        up = struct.unpack('<2I', image[address])[1]
        if stop is None and ending is None and up >> 30 & 1:
            ending = address + 8
        elif ending is not None and address >= ending:
            break
        address += 8
        if stop is not None and address >= stop:
            break


if __name__ == '__main__':
    main()
