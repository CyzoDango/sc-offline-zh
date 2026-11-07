"""Write the standalone item browser from a local catalog-dump TSV."""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('catalog',type=Path);a=ap.parse_args()
    rows=[s.split('\t') for s in a.catalog.read_text(encoding='utf-8-sig').splitlines()]
    labels={'undersuit':'贴身衣','helmet':'头盔','torso':'胸甲','arms':'臂甲','legs':'腿甲','backpack':'背包','primary':'长枪与工具','sidearm':'手枪','ammo':'弹药','grenade':'投掷物'}
    gear={};slot=''
    for line in (ROOT/'data/items.txt').read_text().splitlines():
        line=line.strip()
        if line.startswith('['):slot=line[1:-1]
        elif line and not line.startswith('#'):gear[line]=labels.get(slot,'装备')
    for r in rows:
        if r[0]!='I' or len(r[1])>=96:continue
        cls,file=r[1:3]
        if any(k in file for k in ['/consumables/','/food/','/drink/','/medical/']):gear[cls]='食物与医疗'
        elif any(k in file for k in ['/weapons/personal/','/weapons/fps/']):gear[cls]='武器与工具'
        elif any(k in file for k in ['/ships/power/','/ships/powerplant/','/ships/cooler/','/ships/shield/','/ships/quantum/','/ships/qdrive/']):gear[cls]='飞船组件'
        elif '/ships/weapons/guns/' in file:gear[cls]='舰载武器'
    valid={r[1] for r in rows if r[0]=='I'};gear={c:k for c,k in gear.items() if c in valid}
    (ROOT/'data/item_catalog.txt').write_text('# Verified local DataCore standalone item candidates; registry checked at runtime.\n'+''.join(c+'\t'+gear[c]+'\n' for c in sorted(gear)),encoding='utf8')
    print(len(gear),'item candidates')
if __name__=='__main__':main()
