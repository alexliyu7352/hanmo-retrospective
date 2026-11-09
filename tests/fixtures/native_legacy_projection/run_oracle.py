"""逐文件核CLI结果与独立旧答案；组件审计从原state/catalog重组，不回用候选codec payload。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

KNOWN = {
 'actor': ['id','prototype_id','display_name','birth_tick','created_tick','alive','available','hp','fatigue','merit','wallet_id','inventory_id','action_id','location'],
 'wallet': ['id','owner','balance','reserved'], 'inventory':['id','owner','capacity','revision','lots'],
 'lot':['id','item_id','quantity','owner','reserved_by'],
 'activity':['id','kind','actor_id','participants','start_tick','remaining_ticks','params'],
 'office_state':['id','holder_actor_id','enabled','appointment_id'], 'office_definition':[],
 'organization':['id','name_key','kind','leader_actor'], 'item':['id','weight'], 'fact':['kind']}
SOURCE_FIELDS=['ruleset_id','content_id','content_version','content_digest','engine_build','rng_backend','scenario_id','tick','revision']


def strict(path: Path):
    """拒绝重复JSON键，避免对照输入在Python被静默覆盖。"""
    def pairs(rows):
        """保留重复键失败，不让JSON对象读取掩盖冲突。"""
        result={}
        for key,value in rows:
            if key in result: raise ValueError('Duplicate key: '+key)
            result[key]=value
        return result
    return json.loads(path.read_bytes(),object_pairs_hook=pairs)


def canonical(value) -> bytes:
    """只规范化JSON排版，原数组顺序与Unicode值保持。"""
    return json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode()


def sha(value: bytes) -> str:
    """原字节摘要不是世界身份或授权依据。"""
    return hashlib.sha256(value).hexdigest()


def tagged(value):
    """独立协议编码，bool先于int，拒绝源域外值而不猜类型。"""
    if value is None: return {'t':'null'}
    if type(value) is bool: return {'t':'bool','v':value}
    if type(value) is int: return {'t':'int','v':str(value)}
    if type(value) is str: return {'t':'string','v':value}
    if type(value) is list: return {'t':'array','v':[tagged(row) for row in value]}
    if type(value) is dict and all(type(k) is str for k in value): return {'t':'dict','v':{k:tagged(v) for k,v in value.items()}}
    raise ValueError('Unsupported old source value')


def old_records(source: dict, ownership: list) -> dict:
    """按冻结合同从旧来源直接关联八域及元数据，不读取工件payload当成期望。"""
    state=source['state']; data=source['catalog']['data']; records={}; partitions=[]
    def partition(kind, identifier, record):
        """按固定共享模型的字段集合建立独立键账本，不复制字段值。"""
        partitions.append({'kind':kind,'id':identifier,'known':sorted(set(record)&set(KNOWN[kind])),'extras':sorted(set(record)-set(KNOWN[kind]))})
    for target,table,kind in [('actors','actors','actor'),('wallets','wallets','wallet'),('inventories','inventories','inventory'),('activities','actions','activity')]:
        records[target]={key:state[table][key] for key in sorted(state[table])}
        for key,record in records[target].items():
            partition(kind,key,record)
            if kind=='inventory':
                for lot in record['lots']: partition('lot',lot['id'],lot)
    for table,kind in [('organizations','organization'),('items','item')]:
        records[table]={key:data[table][key] for key in sorted(data[table])}
        for key,record in records[table].items(): partition(kind,key,record)
    records['offices']={}; needed=set()
    for key in sorted(state['offices']):
        office=state['offices'][key]; definition=data['offices'][key]
        records['offices'][key]={'state':office,'definition':definition}
        partition('office_state',key,office); partition('office_definition',key,definition)
        if office['appointment_id'] and office['appointment_id'] in state['facts']: needed.add(office['appointment_id'])
    for key in sorted(state['actors']):
        if key not in data['actors']:
            needed.add(key); creation=state['facts'][key]
            if creation['source'] in state['facts']: needed.add(creation['source'])
    records['facts']={key:{'id':key,'record':state['facts'][key]} for key in sorted(needed)}
    for key in records['facts']: partition('fact',key,state['facts'][key])
    refs={name:sorted(state[name]) for name in ['tasks','parties','armies']}
    refs['catalog_actors']=sorted(data['actors'])
    for name in ['places','settlements','routes','world_maps','items','offices','garrisons','roles']: refs[name]=sorted(data[name])
    coverage=[]
    for claim in ownership:
        status='deferred'
        if claim['scope']=='actor_field': status='typed' if claim['key'] in KNOWN['actor'] else 'preserved_legacy'
        elif claim['container']=='actions': status='typed' if claim['scope']=='envelope' else 'preserved_legacy'
        elif claim['container']=='facts': status='reference_only'
        elif claim['scope'] in ['root','envelope']:
            if claim['container'] in SOURCE_FIELDS+['actors','wallets','inventories','offices']: status='typed'
            elif claim['container'] in ['tasks','parties','armies']: status='reference_only'
        coverage.append({**claim,'status':status})
    provenance={'source_commit':source['source_commit'],'schema_sha256':source['catalog']['schema_sha256'],'rule_modules':source['rule_modules']}
    if 'pack_set_digest' in source['catalog']: provenance['pack_set_digest']=source['catalog']['pack_set_digest']
    records['metadata']={'source':{key:state[key] for key in SOURCE_FIELDS},'provenance':provenance,'references':refs,'coverage':{'ownership':coverage,'records':partitions}}
    return records


def verify(exports: Path,sources: Path,ownership: Path,output: Path,executable: Path|None=None,existing: Path|None=None) -> dict:
    """新运行只传input/queries给CLI；也可只验既有真实输出而不重复原生门。"""
    if output.exists(): raise ValueError('Require a fresh report directory')
    output.mkdir(parents=True); manifest=strict(exports/'cases.json'); claims=strict(ownership); rows=[]
    for case in manifest['cases']:
        name=case['id']
        if not name or Path(name).name!=name or name in ['.','..']: raise ValueError('Unsafe case id')
        folder=exports/name; before={p:sha(p.read_bytes()) for p in [folder/'artifact.json',folder/'queries.json',folder/'oracle.json',sources/name/'source.json']}
        for filename,digest in case['files'].items():
            if sha((folder/filename).read_bytes())!=digest: raise ValueError('Frozen case bytes changed: '+name+'/'+filename)
        if before[sources/name/'source.json']!=case['source_sha256']: raise ValueError('Old source changed: '+name)
        destination=output/name; destination.mkdir()
        if executable:
            result_path=destination/'result.json'; audit_path=destination/'audit.json'
            run=subprocess.run([str(executable),'actor-commitments','--input',str(folder/'artifact.json'),'--queries',str(folder/'queries.json'),'--output',str(result_path),'--audit',str(audit_path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            (destination/'stdout.log').write_text(run.stdout); (destination/'stderr.log').write_text(run.stderr)
            if run.returncode: raise RuntimeError('Native failure '+name+': '+str(run.returncode))
        else:
            result_path=existing/name/'result.json'
            if not result_path.exists(): result_path=existing/name/'stdout.json'
            audit_path=existing/name/'audit.json'
        result=strict(result_path); expected=strict(folder/'oracle.json'); audit=strict(audit_path)
        # 中央CLI可能为存储audit添加执行上下文；真正组件表仍取唯一嵌套audit对象。
        if 'record_component_sha256' not in audit and 'audit' in audit: audit=audit['audit']
        records=old_records(strict(sources/name/'source.json'),claims)
        hashes={group:{key:sha(canonical(tagged(record))) for key,record in values.items()} for group,values in records.items()}
        envelope=strict(folder/'artifact.json'); source=strict(sources/name/'source.json')
        bound=audit.get('input_sha256')==before[folder/'artifact.json'] and audit.get('payload_sha256')==envelope['payload_sha256'] and audit.get('profile')=='HM-NATIVE-LEGACY-ARTIFACT/1'
        lots={key:[lot['id'] for lot in row['lots']] for key,row in source['state']['inventories'].items()}
        legacy=audit.get('component_sha256')==hashes['inventories'] and audit.get('lot_ids')==lots
        row={'id':name,'binding_equal':bound,'legacy_inventory_equal':legacy,'queries_equal':result==expected,'components_equal':audit.get('record_component_sha256')==hashes,'installed_once':audit.get('install_count')==1,'input_unchanged':all(sha(p.read_bytes())==digest for p,digest in before.items())}
        row['passed']=all(row[key] for key in ['binding_equal','legacy_inventory_equal','queries_equal','components_equal','installed_once','input_unchanged'])
        rows.append(row)
        if not row['passed']: (destination/'expected-components.json').write_bytes(canonical(hashes)+b'\n')
    report={'format':'HM-NATIVE-LEGACY-ORACLE-CHECK/1','mode':'existing-output-check' if existing else 'actual-cli','oracle_passed_to_native':False,'cases':rows,'passed':sum(row['passed'] for row in rows),'total':len(rows),'scope':'artifact-only; not-complete-M1; old state and old ownership declaration oracle'}
    (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    if report['passed']!=report['total']: raise RuntimeError('Oracle mismatch; inspect '+str(output/'report.json'))
    return report


def main() -> None:
    """要求明确输入/原来源/拥有权证据和新输出；既有结果与实际执行互斥。"""
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['exports','sources','ownership','output']: parser.add_argument('--'+key,type=Path,required=True)
    mode=parser.add_mutually_exclusive_group(required=True); mode.add_argument('--executable',type=Path); mode.add_argument('--existing-results',type=Path)
    args=parser.parse_args(); print(json.dumps(verify(args.exports.resolve(),args.sources.resolve(),args.ownership.resolve(),args.output.resolve(),args.executable.resolve() if args.executable else None,args.existing_results.resolve() if args.existing_results else None),ensure_ascii=False))


if __name__=='__main__': main()
