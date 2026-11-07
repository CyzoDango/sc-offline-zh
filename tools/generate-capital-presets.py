"""Build an isolated, opt-in Javelin ballistic loadout from local metadata.

Uses the client's existing weapons and interior. It does not create missing models
or a completed capital ship, and does not alter any original database instances.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE='EntityClassDefinition.AEGS_Javelin'
CLASS='AEGS_Javelin_PU_ZH_Ballistic'
RECIPE=2000


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dcb',type=Path,required=True)
    parser.add_argument('--tool',type=Path,required=True)
    args=parser.parse_args()
    raw=subprocess.check_output([str(args.tool),'show',str(args.dcb),SOURCE],encoding='utf-8')
    fields=dict(re.findall(r'^\s*(\S+) = (.+)$',raw,re.M))
    component='Components[1]'
    if not re.search(r'^  Components\[1\] = -> SEntityComponentDefaultLoadoutParams\[',raw,re.M):
        raise ValueError('local loadout component changed')
    manual=component+'.loadout'
    changes={path:'BEHR_JavelinBallisticCannon_S7' for path,value in fields.items()
             if path.startswith(manual+'.') and path.endswith('.entityClassName') and value=='"BEHR_LaserCannon_S7"'}
    if len(changes)!=14:raise ValueError('expected the inspected 14 laser cannon ports; refusing another build')
    q=lambda value:json.dumps(value,ensure_ascii=False)
    out=['format = 1\natomic = true\n']
    def instance(ident,kind,path):
        out.append(f'\n[[instance]]\nid = {q(ident)}\nstruct = {q(kind)}\nclone = {{ record = {q(SOURCE)}, field = {q(path)} }}\n')
    def pointer(ident,field,target):
        out.append(f'\n[[set]]\ninstance = {q("@"+ident)}\nfield = {q(field)}\npointer = {q("@"+target)}\n')
    def array(ident):
        out.append(f'\n[[copy_array]]\ninstance = {q("@"+ident)}\nfield = "entries"\n')
    instance('cap-loadout-component','SEntityComponentDefaultLoadoutParams',component)
    manuals={path for path,value in fields.items() if value.startswith('-> SItemPortLoadoutManualParams[')
             and any(changed.startswith(path+'.') for changed in changes)}
    names={path:'cap-manual-'+str(i) for i,path in enumerate(sorted(manuals,key=len))}
    if manual not in names:raise ValueError('no manual loadout root')
    for path in sorted(manuals,key=len):
        instance(names[path],'SItemPortLoadoutManualParams',path);array(names[path])
        if path==manual:pointer('cap-loadout-component','loadout',names[path])
        else:
            parent=max((p for p in manuals if p!=path and path.startswith(p+'.')),key=len)
            pointer(names[parent],path[len(parent)+1:],names[path])
    for path,value in changes.items():
        parent=max((p for p in manuals if path.startswith(p+'.')),key=len)
        out.append(f'\n[[set]]\ninstance = {q("@"+names[parent])}\nfield = {q(path[len(parent)+1:])}\nvalue = {q(value)}\n')
    header=ROOT/'src/zh/fleet_metadata.generated.h'
    source_header=header.read_text(encoding='utf-8')
    match=re.search(r'\{"AEGS_Javelin",([^\n]+),([0-9]+),(true|false)\},',source_header)
    if not match:raise ValueError('original hull metadata is missing')
    dimensions,base_recipe=match[1],int(match[2])
    for relation in 'OENF':
        for crew in (0,1):
            rec=f'EntityClassDefinition.SCOZH_S{RECIPE:04d}_{relation}{crew}'
            source=f'EntityClassDefinition.SCOZH_S{base_recipe:04d}_{relation}{crew}'
            out.append(f'\n[[record]]\nstruct = "EntityClassDefinition"\nname = {q(rec)}\nclone = {{ record = {q(source)} }}\n')
            out.append(f'\n[[copy_array]]\nrecord = {q(rec)}\nfield = "Components"\n')
            out.append(f'\n[[set]]\nrecord = {q(rec)}\nfield = "Components[SEntityComponentDefaultLoadoutParams]"\npointer = "@cap-loadout-component"\n')
    rec='EntityClassDefinition.'+CLASS
    out.append(f'\n[[record]]\nstruct = "EntityClassDefinition"\nname = {q(rec)}\nclone = {{ record = {q(SOURCE)} }}\n')
    out.append(f'\n[[copy_array]]\nrecord = {q(rec)}\nfield = "Components"\n')
    out.append(f'\n[[set]]\nrecord = {q(rec)}\nfield = "Components[SEntityComponentDefaultLoadoutParams]"\npointer = "@cap-loadout-component"\n')
    (ROOT/'data/builtin/zh_fleet/datacore/99_capital.toml').write_text(''.join(out),encoding='utf-8')
    source_header=re.sub(r'^\{"'+CLASS+r'",[^\n]+\},\n','',source_header,flags=re.M)
    at=source_header.rfind('};')
    header.write_text(source_header[:at]+f'{{"{CLASS}",{dimensions},{RECIPE},true}},\n'+source_header[at:],encoding='utf-8')
    ships=ROOT/'data/ships.txt';text=ships.read_text(encoding='utf-8')
    if CLASS not in text.splitlines():ships.write_text(text.rstrip()+'\n'+CLASS+'\n',encoding='utf-8')
    audit={'source':SOURCE,'new_class':CLASS,'game_build':'4.10.2 / CL12913470','changed_laser_ports':len(changes),
           'weapon_after':'BEHR_JavelinBallisticCannon_S7','changes':changes,'original_instances_modified':False,
           'new_geometry_created':False,'in_game_verified':False}
    (ROOT/'docs/capital-loadout-audit.json').write_text(json.dumps(audit,indent=2)+'\n',encoding='utf-8')
    print('Javelin experimental ballistic variant:',len(changes),'ports replaced; original data preserved')


if __name__=='__main__':main()
