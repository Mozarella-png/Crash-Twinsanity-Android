#!/usr/bin/env python3
"""The PCSX2 probe end to end (see probe.h): cases made by math_tests, a scratch copy of the PS2 build (a detached git worktree in
<scratch>, never this tree) with the cases and probe_ps2.cpp built in and Main calling RunMathProbe first, that ELF run once in
PCSX2 by tools/run_pcsx2.py (muted, in the background, closing only the PCSX2 it started) and frozen at frame 30, the results read
out of its memory over PINE and checked against the native functions.

    native/math-tests/pcsx2/run_probe.py <scratch folder> [math_tests binary]

Use sparingly: it boots PCSX2."""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
PYTHON = os.path.join(ROOT, '.venv', 'bin', 'python')


def run(command, cwd=ROOT):
    print('$', ' '.join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def main():
    scratch = os.path.abspath(sys.argv[1])
    tests = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'build', 'native', 'math-tests', 'math_tests')
    os.makedirs(scratch, exist_ok=True)
    inputs = os.path.join(scratch, 'probe_inputs.bin')
    results = os.path.join(scratch, 'probe_results.bin')
    run([tests, '--probe-inputs', inputs])
    data = open(inputs, 'rb').read()
    count = int.from_bytes(data[4:8], 'little')

    tree = os.path.join(scratch, 'probe-tree')
    if not os.path.isdir(tree):
        run(['git', 'worktree', 'add', '--detach', tree, 'HEAD'])
        for name in ('SLES_525.68', 'local.json'):
            shutil.copy2(os.path.join(ROOT, name), tree)
        run([PYTHON, 'tools/split.py'], cwd=tree)
    probe = os.path.join(tree, 'src', 'mathprobe')
    os.makedirs(probe, exist_ok=True)
    for name in ('probe.h', 'probe_common.cpp'):
        shutil.copy2(os.path.join(HERE, name), probe)
    with open(os.path.join(probe, 'probe_ps2.cpp'), 'w') as f:
        f.write('#define PROBE_CASES %d\n' % count + open(os.path.join(HERE, 'probe_ps2.cpp')).read())
    with open(os.path.join(probe, 'probe_file.cpp'), 'w') as f:
        f.write('#include "common.h"\nextern "C" { alignas(16) extern const u8 g_ProbeFile[]; alignas(16) const u8 g_ProbeFile[] = {\n')
        for start in range(0, len(data), 32):
            f.write(','.join(str(b) for b in data[start:start + 32]) + ',\n')
        f.write('}; }\n')
    main_cpp = os.path.join(tree, 'src', 'main.cpp')
    text = open(main_cpp).read()
    if 'RunMathProbe' not in text:
        text = text.replace('extern "C" int Main(u32 argc, char** argv)\n{\n    RunStaticConstructors();\n'
                            '    Platform::System::Initialise();\n',
                            'extern "C" void RunMathProbe();\nextern "C" int Main(u32 argc, char** argv)\n{\n'
                            '    RunStaticConstructors();\n    Platform::System::Initialise();\n    RunMathProbe();\n')
        open(main_cpp, 'w').write(text)
    run([PYTHON, 'configure.py'], cwd=tree)
    run([PYTHON, 'tools/build.py', '-j10'], cwd=tree)

    symbols = {}
    for line in open(os.path.join(tree, 'build', 'SLES_525.68.map')):
        match = re.match(r'\s+0x([0-9a-f]+)\s+(g_ProbeResults|g_ProbeDone)\s*$', line)
        if match:
            symbols[match.group(2)] = int(match.group(1), 16)
    result_bytes = count * (32 * 16 + 16 + 64 + 32 + 1536)
    iso = json.load(open(os.path.join(tree, 'local.json')))['disc_image']
    done = os.path.join(scratch, 'probe_done.bin')
    run([PYTHON, 'tools/run_pcsx2.py', 'build/SLES_525.68.elf', iso, '--map', 'build/SLES_525.68.map', '--manual',
         '--timeout', '150', '--at-frame', '30:freeze', '--freeze-range', '%X:%d:%s' % (symbols['g_ProbeResults'], result_bytes, results),
         '--freeze-dump', 'g_ProbeDone:1:' + done, '--snapshots', os.path.join(scratch, 'snapshots')], cwd=tree)
    if open(done, 'rb').read() != (0x600DF00D).to_bytes(4, 'little'):
        raise SystemExit('the probe didn\'t finish in PCSX2')
    subprocess.run([tests, '--probe-check', inputs, results])


if __name__ == '__main__':
    main()
