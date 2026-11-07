"""Generate the compact star-map catalog from curated metadata, without game files.

The JSON contains identifiers, radii and display metadata, never textures or meshes.
Use --check in a source release to verify that the committed header is current.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def generate(data):
    q = lambda value: json.dumps(value, ensure_ascii=False)
    bodies, places = data['bodies'], data['places']
    lookup = {b['key']: i for i, b in enumerate(bodies)}
    lines = ['#pragma once',
             '// Metadata generated from local 4.10.2 DataCore and scene roots; no game assets.',
             'namespace sco_map {',
             'struct System {const char* key;const char* name;const char* native;double x,y,z;};',
             'inline constexpr System kSystems[]={' + ','.join(
                 '{' + ','.join(q(s[k]) for k in ('key', 'name', 'native')) + ',' +
                 ','.join(str(s[k]) for k in ('x', 'y', 'z')) + '}' for s in data['systems']) + '};',
             'struct Body {const char* key;const char* name;const char* entity;int system,parent;double radius;bool surface;int kind;unsigned a,b;double angle;};',
             'inline constexpr Body kBodies[]={']
    for b in bodies:
        lines.append(' {' + ','.join(q(b[k]) for k in ('key', 'name', 'entity')) +
                     f",{b['system']},{lookup.get(b['parent'], -1)},{b['radius']}," +
                     str(b['surface']).lower() + f",{b['kind']},0x{b['a']},0x{b['b']},{b['angle']:.9f}" + '},')
    lines += ['};', 'struct Place {const char* key;const char* name;int body;double lat,lon;bool station;};',
              'inline constexpr Place kPlaces[]={']
    for p in places:
        lines.append(' {' + q(p['key']) + ',' + q(p['name']) +
                     f",{p['body']},{p['lat']:.12f},{p['lon']:.12f}," + str(p['station']).lower() + '},')
    return '\n'.join(lines + ['};', '}']) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    source = ROOT/'translations/star-map.json'
    target = ROOT/'src/zh/star_catalog.generated.h'
    output = generate(json.loads(source.read_text(encoding='utf-8')))
    if args.check:
        if output != target.read_text(encoding='utf-8'):
            raise SystemExit('star-map header differs from curated metadata; regenerate it')
        print('star-map catalog matches curated metadata')
    else:
        target.write_text(output, encoding='utf-8')
        print('generated', target)


if __name__ == '__main__':
    main()
