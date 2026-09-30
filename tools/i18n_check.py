#!/usr/bin/env python3
"""Checks that every interface string has a Ukrainian translation.

Texts are written in the code as T("русский", "english") / F("русский {}", "english {}", ...);
Ukrainian lives in src/core/i18n_uk.cpp as {"русский", "українська"} pairs keyed by the Russian text.
This script collects the Russian texts of all T()/F() calls and compares them with the table.

    python tools/i18n_check.py            # exit code 1 if something is missing
    python tools/i18n_check.py --missing  # print the missing texts as table entries
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / 'src' / 'core' / 'i18n_uk.cpp'
SOURCES = [p for p in (ROOT / 'src').rglob('*') if p.suffix in ('.cpp', '.h') and p != TABLE]


def tokens(text):
    """Very small C++ tokenizer: string literals, identifiers and single punctuation characters (comments skipped)."""
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            i = text.find('\n', i)
            i = n if i < 0 else i
        elif text.startswith('/*', i):
            i = text.find('*/', i) + 2
        elif c == '"':
            j = i + 1
            while text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            yield ('str', text[i + 1:j])
            i = j + 1
        elif c == "'":
            j = i + 1
            while text[j] != "'":
                j += 2 if text[j] == '\\' else 1
            yield ('chr', text[i + 1:j])
            i = j + 1
        elif c.isalpha() or c == '_':
            m = re.match(r'[A-Za-z_]\w*', text[i:])
            yield ('id', m.group(0))
            i += len(m.group(0))
        elif c.isspace():
            i += 1
        else:
            if text.startswith('::', i) or text.startswith('->', i):
                yield ('op', text[i:i + 2])
                i += 2
            else:
                yield ('op', c)
                i += 1


def literal_arg(toks, k):
    """Concatenated body of the string-literal argument starting at toks[k]; (text, index after it) or (None, k)."""
    parts = []
    while k < len(toks) and toks[k][0] == 'str':
        parts.append(toks[k][1])
        k += 1
    return (''.join(parts), k) if parts else (None, k)


def calls(path):
    toks = list(tokens(path.read_text(encoding='utf-8')))
    for k, (kind, value) in enumerate(toks):
        if kind == 'id' and value in ('T', 'F') and k + 1 < len(toks) and toks[k + 1] == ('op', '('):
            if k + 2 < len(toks) and toks[k + 2] in (('id', 'const'), ('id', 'std')):
                continue  # the declaration in i18n.h
            if k > 0 and toks[k - 1][0] == 'op' and toks[k - 1][1] in ('.', '::', '->'):
                continue
            if k > 0 and toks[k - 1] in (('id', 'typename'), ('id', 'class')):
                continue
            ru, j = literal_arg(toks, k + 2)
            if ru is None or j >= len(toks) or toks[j] != ('op', ','):
                yield (None, f'{path.relative_to(ROOT)}: first argument of {value}() must be plain string literals')
                continue
            yield (ru, None)


def table_keys():
    toks = list(tokens(TABLE.read_text(encoding='utf-8')))
    keys = []
    for k in range(len(toks) - 1):
        if toks[k] == ('op', '{') and toks[k + 1][0] == 'str':
            key, j = literal_arg(toks, k + 1)
            if j < len(toks) and toks[j] == ('op', ','):
                keys.append(key)
    return keys


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    used, problems = [], []
    for path in sorted(SOURCES):
        for ru, problem in calls(path):
            if problem:
                problems.append(problem)
            elif ru not in used:
                used.append(ru)
    keys = table_keys()
    dup = sorted({k for k in keys if keys.count(k) > 1})
    missing = [k for k in used if k not in keys]
    unused = [k for k in keys if k not in used]
    if '--missing' in sys.argv:
        for k in missing:
            print(f'        {{"{k}", ""}},')
        return 0
    for p in problems:
        print('error:', p)
    for k in dup:
        print('error: duplicate Ukrainian entry:', k)
    for k in missing:
        print('error: no Ukrainian text for:', k)
    for k in unused:
        print('warning: unused Ukrainian entry:', k)
    print(f'i18n: {len(used)} texts, {len(keys)} Ukrainian entries, {len(missing)} missing')
    return 1 if problems or dup or missing else 0


if __name__ == '__main__':
    sys.exit(main())
