#!/usr/bin/env python3
"""独立复核CI已生成的五件输出与原锁身份；不构建、不加载银行、不修改回执。"""
import argparse
import hashlib
import json
from pathlib import Path
import re

ARTIFACTS = {'hanmo_resident_journey','libhanmo_world_bridge.so',
             'libhanmo_resident.a','libhanmo_world.a','libhanmo_flecs.a'}
IDENTITIES = {'resident_bridge_identity.json','dependencies.lock.json','locked.cmake'}


def verify(folder, expected):
    """只在给定输出目录读固定文件名，回执中的旧绝对路径不授予额外文件读取。"""
    if not re.fullmatch(r'[0-9a-f]{40}',expected):raise ValueError('Full source SHA required')
    receipt_path=folder/'native-build-receipt.json'
    receipt=json.loads(receipt_path.read_text())
    if (receipt.get('format')!='HANMO-RECOVERY-NATIVE-BUILD/1'
        or receipt.get('status')!='BUILD_COMPLETE_RUNTIME_NOT_RUN'
        or any(receipt.get(key)!=expected for key in ('source_commit','source_commit_after','requested_source_commit'))
        or receipt.get('source_status_after')!=''
        or receipt.get('dependencies_unchanged_during_build') is not True
        or (folder/'source-commit.txt').read_text().strip()!=expected):raise ValueError('Source or build receipt mismatch')
    stages=receipt.get('stages',[])
    if ([x.get('name') for x in stages]!=['configure','build-hanmo_resident_journey','build-hanmo_world_bridge']
        or any(type(x.get('exit_code')) is not int or x['exit_code']!=0 for x in stages)):raise ValueError('Actual target stages not complete')
    def verify_rows(rows,names):
        if len(rows)!=len(names) or {Path(x['path']).name for x in rows}!=names:raise ValueError('Native output set mismatch')
        for item in rows:
            file=folder/Path(item['path']).name
            if (type(item['bytes']) is not int or item['bytes']<=0 or file.is_symlink()
                or not file.is_file() or file.stat().st_size!=item['bytes']):raise ValueError('Native output shape mismatch')
            digest=hashlib.sha256(file.read_bytes()).hexdigest()
            if digest!=item['sha256']:raise ValueError('Native output bytes mismatch')
    verify_rows(receipt.get('artifacts',[]),ARTIFACTS)
    verify_rows(receipt.get('identity_files',[]),IDENTITIES)
    bridge=json.loads((folder/'resident_bridge_identity.json').read_text())
    if not bridge or bridge!=receipt.get('bridge_identity'):raise ValueError('Actual resident identity mismatch')
    lock=json.loads((folder/'dependencies.lock.json').read_text())
    specs=dict(lock['libraries'],**{'godot-cpp':lock['godot_bridge']['binding']})
    dependencies=receipt.get('dependencies',{})
    if set(specs)!=set(dependencies) or any(spec['files']!=dependencies[name]['files'] for name,spec in specs.items()):raise ValueError('Locked dependency evidence mismatch')
    if receipt.get('engine',{}).get('sha256')!=lock['godot_bridge']['sdk']['engine_sha256']:raise ValueError('Fixed engine identity mismatch')
    return {'status':'BUILD_OUTPUTS_VERIFIED_RUNTIME_NOT_RUN','source_commit':expected,
            'artifact_count':5,'dependency_file_count':sum(len(spec['files']) for spec in specs.values()),
            'receipt_sha256':hashlib.sha256(receipt_path.read_bytes()).hexdigest()}


def main():
    """主机或目标容器都可核同一文件集合；缺任一真实产物即非零退出。"""
    parser=argparse.ArgumentParser();parser.add_argument('--directory',type=Path,required=True);parser.add_argument('--expected-source',required=True)
    args=parser.parse_args();print(json.dumps(verify(args.directory,args.expected_source),indent=2))

if __name__=='__main__':main()
