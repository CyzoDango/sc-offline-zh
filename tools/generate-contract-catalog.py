"""Generate script-gated contract metadata from sco-contract-catalog's local TSV.

Retains the established scriptless list. New entries require known templates and
nonempty script dependencies, including nested subcontract templates. No scripts
are copied into the distributable program or source.
"""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PREFIX='Libs/Subsumption/Missions/PU/'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('catalog',type=Path)
    args=parser.parse_args()
    rows=[line.split('\t') for line in args.catalog.read_text(encoding='utf-8-sig').splitlines()]
    templates={row[1]:set(filter(None,row[2].split('|'))) for row in rows if row[0]=='T'}
    def dependencies(name,visited=None):
        visited=set() if visited is None else visited
        if name in visited:return set()
        if name not in templates:raise ValueError('unknown template '+name)
        visited.add(name);out=set()
        for entry in templates[name]:
            out.update(dependencies(entry[1:],visited) if entry.startswith('@') else {entry})
        return out
    base=ROOT/'translations/contracts-scriptless-4.10.2.txt'
    entries={}
    for line in base.read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#'):
            ident,name,scripts=line.split();assert scripts=='-';entries[ident]=(name,[])
    added=0
    for row in rows:
        if row[0]!='G' or row[1] in entries:continue
        try:
            required=dependencies(row[3])
            for nested in filter(None,row[5].split('|')):
                required.update(dependencies(nested.removeprefix('@')))
        except ValueError:continue
        if not required:continue
        paths=sorted(required)
        if any(not path.startswith(PREFIX) or not path.endswith('.xml') or '..' in path or any(c.isspace() for c in path) for path in paths):continue
        if not row[2] or any(c.isspace() for c in row[2]):continue
        entries[row[1]]=(row[2],[path.removeprefix(PREFIX) for path in paths]);added+=1
    text='# Contract IDs and script references only, generated from local 4.10.2 metadata.\n# Extra contracts remain hidden unless every required local script is registered.\n'
    for ident,(name,paths) in entries.items():text+=f'{ident} {name} '+('|'.join(paths) if paths else '-')+'\n'
    (ROOT/'data/contract_scripts.txt').write_text(text,encoding='utf-8')
    print('contracts:',len(entries),'established scriptless:',len(entries)-added,'script-gated additions:',added)


if __name__=='__main__':main()
