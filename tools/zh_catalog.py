"""Inventory and apply reviewed UI translations without changing command IDs.

The launcher CLI output remains English because its GUI parses that protocol.
Only its GUI labels (after kGuiCommands) are translated; matching/parser strings
are explicitly excluded. Source comments and C/C++ escape sequences are retained.
"""
import ast, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|(?P<string>(?:u8|[uUL])?"(?:\\.|[^"\\])*")|\'(?:\\.|[^\'\\])*\'', re.S)
FORMAT = re.compile(r'%(?:\d+\$)?[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztLI]|I64)?[diuoxXfFeEgGaAcspn%]')

def selected_files():
    return [ROOT/'src/menu.cpp', *sorted((ROOT/'src/builtins').glob('*.cpp')), ROOT/'launcher/launcher.cpp']

def literals(path, text):
    launcher_start = text.find('static const GuiCommand kGuiCommands[]')
    launcher_end = text.find('int wmain(')
    for match in TOKEN.finditer(text):
        if not match.group('string'): continue
        token = match.group('string')
        if token.startswith(('L"', 'u"', 'U"', 'u8"')): continue
        try: value = ast.literal_eval(token)
        except (ValueError, SyntaxError): continue
        if not isinstance(value, str) or not re.search('[A-Za-z]', value): continue
        if value.startswith(('##', '#', '.', '/', '\\')): continue
        if path.parent.name == 'launcher':
            if not launcher_start <= match.start() < launcher_end: continue
            line_start = text.rfind('\n', 0, match.start()) + 1
            line_end = text.find('\n', match.end())
            line = text[line_start:line_end]
            # These are protocol parsing, paths, schemas and plugin identities.
            if re.search(r'\.find\(|\.rfind\(|strstr\(|Field\(|ReadConfig|ReadAll|WriteAll|Extract|key\s*==|key\s*!=|key\s*=|type\s*==|_stricmp\(|memcmp\(', line): continue
        yield match, value

def main():
    catalog_path = ROOT/'translations/zh-CN.json'
    if '--extract' in sys.argv:
        found = {}
        for path in selected_files():
            text = path.read_text(encoding='utf-8-sig')
            for match, value in literals(path, text):
                if len(value) > 450: continue
                if not (' ' in value or value[0].isupper()): continue
                if re.fullmatch(r'[A-Z_0-9]+', value) or value.endswith(('.cpp','.h','.png','.ttf','.ini','.txt','.dll')): continue
                found.setdefault(value, []).append(str(path.relative_to(ROOT)))
        target = ROOT/'translations/ui-inventory.json'
        target.parent.mkdir(exist_ok=True)
        target.write_text(json.dumps(found, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(list(found), ensure_ascii=False, indent=2))
        return
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    for en, zh in catalog.items():
        if FORMAT.findall(en) != FORMAT.findall(zh):
            raise ValueError(f'Format placeholders changed: {en!r} -> {zh!r}')
        if not zh: raise ValueError(f'Empty translation: {en}')
    used = set()
    counts = {}
    for path in selected_files():
        text = path.read_text(encoding='utf-8-sig')
        edits = []
        for match, value in literals(path, text):
            visible, marker, identity = value.partition('##')
            key = value if value in catalog else visible
            if key not in catalog: continue
            replacement = catalog[key]
            if key != value and marker: replacement += marker+identity
            edits.append((match.start(), match.end(), json.dumps(replacement, ensure_ascii=False)))
            used.add(key)
        for start, end, replacement in reversed(edits): text = text[:start]+replacement+text[end:]
        if edits:
            path.write_text(text, encoding='utf-8', newline='\n')
            counts[str(path.relative_to(ROOT))] = len(edits)
    print(json.dumps({'replacements': sum(counts.values()), 'files': counts, 'unused': sorted(set(catalog)-used)}, ensure_ascii=False, indent=2))

if __name__ == '__main__': main()
