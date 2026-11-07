"""Generate isolated offline fleet recipes from catalog-dump's local TSV.

Only class names, dimensions and references are retained; no models, textures,
loadouts or community dictionary are copied. Re-run for a new game build.
"""
import argparse
import collections
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
def q(v): return json.dumps(v, ensure_ascii=False)
def base(name):
    return re.split(r'_(?:PU|EA|AI|AC|Unmanned|Derelict|Wreck|Template|Mission|Display|BMBRCK|TEMP)(?:_|$)', name, maxsplit=1, flags=re.I)[0]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('catalog',type=Path)
    a=ap.parse_args()
    lines=[s.split('\t') for s in a.catalog.read_text(encoding='utf-8-sig').splitlines()]
    ships=sorted((r for r in lines if r[0]=='V'),key=lambda r:r[1].lower())
    npcs={r[1]:r[2] for r in lines if r[0]=='N'}
    crews=collections.defaultdict(list)
    for r in lines:
        if r[0]=='C':crews[r[1]].append((r[2],r[3]))
    old=[s.strip() for s in (ROOT/'translations/ships-upstream-0.7.7.txt').read_text().splitlines() if s.strip() and not s.startswith('#')]
    native={r[1].lower() for r in ships}
    added=[r[1] for r in ships if r[1].lower() not in {s.lower() for s in old}]
    # Keep historical candidates (registry checked at runtime), canonicalize current ones.
    catalog=[r[1] for r in ships]+[s for s in old if s.lower() not in native]
    (ROOT/'data/ships.txt').write_text('# Local 4.10.2 / CL12913470 vehicle records plus historical candidates.\n# Unregistered classes are excluded by the game thread.\n'+'\n'.join(sorted(catalog,key=str.lower))+'\n')
    defaults={base(r[1]).lower():r[6] for r in ships if r[6] in crews}
    manifests={r[6] for r in ships if r[6] in crews}|{'CrewManifest.PU_Human_Civilian_Light_wTurret'}
    valid={m for m in manifests if all(n.removeprefix('EntityClassDefinition.') in npcs and npcs[n.removeprefix('EntityClassDefinition.')] for _,n in crews[m])}
    roles=sorted({n for m in valid for _,n in crews[m]})
    ni={n:i for i,n in enumerate(roles)}; mi={m:i for i,m in enumerate(sorted(valid))}
    packdir=ROOT/'data/builtin/zh_fleet/datacore';packdir.mkdir(parents=True,exist_ok=True)
    # This generator owns only these files; refuse stale names rather than removing others.
    prefix='format = 1\natomic = true\n'
    factions=[('E','Faction.Faction_Special_HostileToAll','Faction_LEGACY.Hostile'),
              ('N','Faction.Faction_Lawful_Civilian','Faction_LEGACY.Neutral'),
              ('F','Faction.Faction_Special_FriendlyToAll','Faction_LEGACY.EA_FriendToAll')]
    def record(name,kind,clone):
        return f'\n[[record]]\nstruct = {q(kind)}\nname = {q(name)}\nclone = {{ record = {q(clone)} }}\n\n[[copy_array]]\nrecord = {q(name)}\nfield = {q("Components" if kind=="EntityClassDefinition" else "crew")}\n'
    def component(rec,src,kind,ident,sets):
        result = f'\n[[instance]]\nid = {q(ident)}\nstruct = {q(kind)}\nclone = {{ record = {q(src)}, field = {q("Components["+kind+"]")} }}\nset = {{ '+', '.join(q(k)+' = '+v for k,v in sets.items() if v!='null')+' }\n'+f'\n[[set]]\nrecord = {q(rec)}\nfield = {q("Components["+kind+"]")}\npointer = {q("@"+ident)}\n'
        for key,value in sets.items():
            if value=='null': result+=f'\n[[set]]\ninstance = {q("@"+ident)}\nfield = {q(key)}\npointer = "null"\n'
        return result
    text=prefix
    for n,i in ni.items():
        for suffix,new,legacy in factions:
            rec=f'EntityClassDefinition.SCOZH_N{i:04d}_{suffix}'
            text+=record(rec,'EntityClassDefinition',n)
            text+=component(rec,n,'SEntityComponentHostilityParams',f'n{i}{suffix}',{'factionSC':'{ ref = '+q(new)+' }','faction':'{ ref = '+q(legacy)+' }'})
    for m,i in mi.items():
        for suffix,_,_ in factions:
            rec=f'CrewManifest.SCOZH_C{i:04d}_{suffix}'
            text+=record(rec,'CrewManifest',m)
            for path,n in crews[m]:
                text+=f'\n[[set]]\nrecord = {q(rec)}\nfield = {q(path)}\nvalue = {{ ref = {q(f"EntityClassDefinition.SCOZH_N{ni[n]:04d}_{suffix}")} }}\n'
    (packdir/'00_crew.toml').write_text(text,encoding='utf8')
    meta=[];unready=[];text=prefix;part=1
    for i,r in enumerate(ships):
        cls=r[1];dims=[float(v or 0) for v in r[2:5]]
        manifest=r[6] if r[6] in valid else defaults.get(base(cls).lower(),'CrewManifest.PU_Human_Civilian_Light_wTurret')
        ready=manifest in valid and bool(r[7]) 
        meta.append([cls,*dims,i,ready])
        if not ready:unready.append(cls);continue
        for relation in range(4):
            for crew in range(2):
                suffix='O' if relation==0 else factions[relation-1][0]
                rec=f'EntityClassDefinition.SCOZH_S{i:04d}_{suffix}{crew}'
                text+=record(rec,'EntityClassDefinition','EntityClassDefinition.'+cls)
                hs={} if relation==0 else {'emptyFactionSC':'{ ref = '+q(factions[relation-1][1])+' }','emptyFaction':'{ ref = '+q(factions[relation-1][2])+' }'}
                if hs:text+=component(rec,'EntityClassDefinition.'+cls,'SEntityComponentHostilityCompositeParams',f'h{i}{suffix}{crew}',hs)
                # Cloning the component before clearing keeps the original templates intact.
                cm='null' if crew==0 else ('{ ref = '+q(manifest if relation==0 else f'CrewManifest.SCOZH_C{mi[manifest]:04d}_{suffix}')+' }')
                text+=component(rec,'EntityClassDefinition.'+cls,'SEntityComponentCrewManifestParams',f'c{i}{suffix}{crew}',{'crewManifest':cm})
        if len(text.encode())>2_500_000:
            (packdir/f'{part:02d}_ships.toml').write_text(text,encoding='utf8');part+=1;text=prefix
    if text!=prefix:(packdir/f'{part:02d}_ships.toml').write_text(text,encoding='utf8')
    generated=ROOT/'src/zh/fleet_metadata.generated.h'
    generated.write_text('#pragma once\n// Generated from local 4.10.2 CL12913470; identifiers and dimensions only.\nnamespace sco_zh {\nstruct HullMeta {const char* cls;float width,length,height;int recipe;bool ready;};\ninline constexpr HullMeta kHullMetadata[]={\n'+''.join('{'+q(c)+f',{w}f,{l}f,{h}f,{i},'+('true' if ok else 'false')+'},\n' for c,w,l,h,i,ok in meta)+'};\n}\n',encoding='utf8')
    report={'game':'4.10.2 CL12913470','local_vehicle_records':len(ships),'candidate_classes':len(catalog),'added':added,'recipe_ready':sum(r[-1] for r in meta),'unsupported_recipes':unready,'crew_manifests':len(valid),'crew_role_classes':len(roles),'files':len(list(packdir.glob('*.toml')))}
    (ROOT/'docs/fleet-catalog-audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
