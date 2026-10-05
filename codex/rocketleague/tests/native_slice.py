"""Extract unmodified native functions for linkers without ELF section GC.

This is a test build aid only. It copies source bodies verbatim, never substitutes
their logic. Normal ELF fixtures include the entire native translation unit.
"""
import re

def function(source, name):
    match = re.search(r'^([\w* ]+\b' + re.escape(name) + r'\([^;]*?\)\s*)\{', source, re.M)
    if not match:
        raise ValueError('Missing native function: '+name)
    # Ignore braces inside comments and quoted literals while finding the body.
    token = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for part in token.finditer(source, match.end()-1):
        if part.group() == '{': depth += 1
        elif part.group() == '}':
            depth -= 1
            if depth == 0:
                return source[match.start():part.end()]+'\n'
    raise ValueError('Unclosed native function: '+name)

def slice_source(source, names, globals=()):
    headers = '\n'.join(x for x in re.findall(r'^#include[^\n]+', source, re.M) if '.inc.c' not in x)+'\n'
    definitions = []
    for name in globals:
        match = re.search(r'^(?:(?:static|extern) )?[A-Za-z_]\w*[ \t]+'+re.escape(name)+r'(?:\[[^\]\n]*\])*[ \t]*(?:=[^;]*)?;', source, re.M)
        if not match: raise ValueError('Missing native global: '+name)
        definitions.append(match.group()+'\n')
    return headers+''.join(definitions)+''.join(function(source,name) for name in names)
