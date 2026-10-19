#!/usr/bin/env python3
"""The PS2 side's maths (src/platform/ps2/math.cpp, matrices.cpp, collisionmaths.cpp, bounds.cpp) made host C++ for the tests'
reference: every asm statement a call of Ref::Asm with its template's text (the C++ string literals as they are, macros and all)
and its operands, every function the file defines renamed Ref_<name>. The rest of the C++ (the control flow around the asm) is
the PS2 side's own.

    make_reference.py <out dir>"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
FILES = ['math.cpp', 'matrices.cpp', 'collisionmaths.cpp', 'bounds.cpp']
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'asm'}


def skip_string(text, i):
    """Index past the string or character literal starting at i"""
    quote = text[i]
    i += 1
    while text[i] != quote:
        if text[i] == '\\':
            i += 1
        i += 1
    return i + 1


def matching_paren(text, i):
    depth = 0
    while True:
        c = text[i]
        if c in '"\'':
            i = skip_string(text, i)
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1


def split_top(text, separator):
    parts = []
    depth = 0
    start = 0
    i = 0
    while i < len(text):
        c = text[i]
        if c in '"\'':
            i = skip_string(text, i)
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        elif c == separator and depth == 0:
            parts.append(text[start:i])
            start = i + 1
        i += 1
    parts.append(text[start:])
    return parts


def operands(text, output):
    result = []
    for part in split_top(text, ','):
        part = part.strip()
        if not part:
            continue
        match = re.match(r'"([^"]*)"\s*\((.*)\)$', part, re.S)
        if not match:
            raise SystemExit('operand: ' + part)
        constraint, expression = match.groups()
        if output:
            if not constraint.startswith('='):
                raise SystemExit('in-out operand: ' + part)
            result.append('Ref::Out(%s)' % expression)
        else:
            result.append('Ref::In(%s)' % expression)
    return result


def transform(text):
    out = []
    i = 0
    pattern = re.compile(r'\basm\s*(volatile\s*)?\(')
    while True:
        match = pattern.search(text, i)
        if not match:
            out.append(text[i:])
            break
        out.append(text[i:match.start()])
        open_paren = match.end() - 1
        close = matching_paren(text, open_paren)
        inner = text[open_paren + 1:close]
        parts = split_top(inner, ':')
        template = parts[0]
        ops = operands(parts[1], True) if len(parts) > 1 else []
        ops += operands(parts[2], False) if len(parts) > 2 else []
        out.append('Ref::Asm(%s, {%s})' % (template.strip(), ', '.join(ops)))
        i = close + 1
    return ''.join(out)


def defined_functions(text):
    """The functions the file defines: a signature (perhaps continued over lines ending with commas) before a line of {"""
    names = set()
    lines = text.split('\n')
    for index in range(1, len(lines)):
        if lines[index].strip() != '{':
            continue
        start = index - 1
        if lines[start].rstrip().endswith(';') or lines[start].lstrip().startswith(('//', '#')):
            continue
        while start > 0 and lines[start - 1].rstrip().endswith(','):
            start -= 1
        match = re.search(r'\b([A-Za-z_]\w*)\s*\(', lines[start])
        if match and match.group(1) not in KEYWORDS:
            names.add(match.group(1))
    return names


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    for name in FILES:
        text = open(os.path.join(ROOT, 'src', 'platform', 'ps2', name)).read()
        functions = defined_functions(text)
        text = transform(text)
        for function in sorted(functions, key=len, reverse=True):
            text = re.sub(r'\b%s\b' % function, 'Ref_' + function, text)
        header = ('// Made by native/math-tests/tools/make_reference.py from src/platform/ps2/%s: its asm run by the reference\n'
                  '// interpreter, its functions renamed Ref_<name> (%s)\n#include "refvu0.h"\n' % (name, ', '.join(sorted(functions))))
        with open(os.path.join(out_dir, 'ref_' + name), 'w') as f:
            f.write(header + text)


if __name__ == '__main__':
    main()
