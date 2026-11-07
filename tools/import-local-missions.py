"""Import only the user-provided archive's mission XML into a private local install.

Does not execute anything or download files. The destination must be a new scripts
folder. Imported third-party XML is deliberately excluded from release packaging.
"""
import argparse
import hashlib
import json
from pathlib import Path,PurePosixPath
import re
import xml.etree.ElementTree as ET
import zipfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--destination',type=Path,required=True)
    args=parser.parse_args();dest=args.destination.resolve()
    if dest.exists():raise ValueError('destination must be a new scripts folder')
    prefix='sc-offline/data/scripts/'
    accepted=[];seen=set();total=0;roots=[]
    with zipfile.ZipFile(args.archive) as archive:
        for info in archive.infolist():
            if not info.filename.startswith(prefix) or not info.filename.endswith('.xml'):continue
            rel=PurePosixPath(info.filename.removeprefix(prefix))
            if '\\' in str(rel) or rel.is_absolute() or '..' in rel.parts or ':' in str(rel) or not str(rel).startswith('Libs/Subsumption/'):
                raise ValueError('unsafe script path')
            if str(rel).lower() in seen:raise ValueError('duplicate script path')
            seen.add(str(rel).lower());total+=info.file_size
            if info.file_size>2_000_000 or total>32_000_000 or len(seen)>1024:raise ValueError('script import exceeds bounds')
            blob=archive.read(info)
            if b'<!DOCTYPE' in blob.upper() or b'<!ENTITY' in blob.upper():raise ValueError('XML entities are not accepted')
            tree=ET.fromstring(blob)
            if tree.tag!='Subsumption':raise ValueError('unexpected XML root')
            for node in list(tree):
                if node.tag in ('Mission','Subactivity') and node.get('ID'):
                    if not re.fullmatch(r'[0-9a-fA-F-]{36}',node.get('ID')):raise ValueError('invalid mission identifier')
                    if node.tag=='Mission':roots.append({'path':str(rel),'id':node.get('ID'),'name':node.get('Name')})
            accepted.append((rel,blob,hashlib.sha256(blob).hexdigest()))
    if not accepted:raise ValueError('no supported local mission XML found')
    dest.mkdir(parents=True)
    for rel,blob,_ in accepted:
        target=dest.joinpath(*rel.parts)
        if not target.resolve().is_relative_to(dest):raise ValueError('script escaped destination')
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
    with args.archive.open('rb') as stream:source_hash=hashlib.file_digest(stream,'sha256').hexdigest()
    receipt={'archive':str(args.archive.resolve()),'archive_sha256':source_hash,'script_files':len(accepted),'bytes':total,
             'mission_roots':roots,'files':{str(rel):sha for rel,_,sha in accepted},'game_verified':False,'redistributable_package_includes_scripts':False}
    (dest.parent/'local-missions-import.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('imported',len(accepted),'XML files with',len(roots),'mission roots into the private local install; no game executed')


if __name__=='__main__':main()
