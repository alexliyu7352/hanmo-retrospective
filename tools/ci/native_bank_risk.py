#!/usr/bin/env python3
"""只读固定成功CI五产物，在其原头根链接具名银行探针；不重建任何正式target。"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess

REPOSITORY = '（原私有仓库）'
PROBE = 'naval3-original-active-continue'
DIRECT_PROBE = 'naval3-original-active-direct'
SCENARIO = 'dockyard::scenario.dock_maintenance'
PROBES = {
    PROBE: ([], 'ORIGINAL_NAVAL3_ACTIVE_BANK_RESUME_RISK_ONLY'),
    DIRECT_PROBE: (['direct'], 'ORIGINAL_NAVAL3_DIRECT_ASSET_COLD_TRANSACTION_ONLY'),
}
WORKFLOW = '.github/workflows/native-ubuntu2004.yml'


def digest(path):
    """计算实际原件字节身份，输入/正式库的后验使用相同算法。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def request(path):
    """请求只接受本仓库、固定40位源与一个具名probe，不接URL或任意命令。"""
    value=json.loads(path.read_text())
    if (set(value)!={'format','repository','native_run_id','source_commit','probe_id'}
        or value['format']!='HANMO-NATIVE-BANK-RISK/1'
        or value['repository']!=REPOSITORY or value['probe_id'] not in PROBES
        or type(value['native_run_id']) is not int or value['native_run_id']<=0
        or not re.fullmatch('[0-9a-f]{40}',value['source_commit'])):
        raise ValueError('Invalid bounded native bank request')
    return value


def select(req, run, artifacts):
    """区分工作流头与实际产物源；成功同仓原流程、唯一归属原件和后续五件独验缺一不可。"""
    repository=run.get('repository',{});head_repository=run.get('head_repository',{})
    repository_id=repository.get('id');event=run.get('event');workflow_head=run.get('head_sha')
    if (run.get('id')!=req['native_run_id'] or repository.get('full_name')!=REPOSITORY
        or type(repository_id) is not int or repository_id<=0
        or head_repository.get('id')!=repository_id or head_repository.get('full_name')!=REPOSITORY
        or not isinstance(workflow_head,str) or not re.fullmatch('[0-9a-f]{40}',workflow_head)
        or run.get('path')!=WORKFLOW or run.get('status')!='completed'
        or run.get('conclusion')!='success' or event not in ('push','workflow_dispatch')
        or (event=='push' and workflow_head!=req['source_commit'])):
        raise ValueError('Requested native run not successfully complete at exact bounded source')
    if artifacts.get('total_count')!=len(artifacts.get('artifacts',[])):
        raise ValueError('Artifact listing incomplete')
    name='native-ubuntu2004-'+req['source_commit']
    found=[x for x in artifacts['artifacts'] if x.get('name')==name]
    if (len(found)!=1 or found[0].get('expired') is not False
        or type(found[0].get('id')) is not int or found[0]['id']<=0
        or found[0].get('size_in_bytes',0)<=0):
        raise ValueError('Unique live native artifact required')
    linked=found[0].get('workflow_run',{})
    if (linked.get('id')!=run['id'] or linked.get('repository_id')!=repository_id
        or linked.get('head_repository_id')!=repository_id or linked.get('head_sha')!=workflow_head):
        raise ValueError('Native artifact workflow ownership mismatch')
    # dispatch的工作流头可以不同，但这里只获选取资格；下载后须按req源独验五件/receipt/锁才可链接。
    return {'run_id':req['native_run_id'],'sha':req['source_commit'],
            'artifact_name':name,'artifact_id':found[0]['id']}


def files(root, rows):
    """只读取固定清单中的仓库相对普通文件，越界路径/链接/错字节均拒绝。"""
    result={}
    for row in rows:
        relative=Path(row['path'])
        if relative.is_absolute() or '..' in relative.parts or row['path'] in result:
            raise ValueError('Invalid input path')
        file=root/relative
        if (os.path.commonpath([str(root.resolve()),str(file.resolve())])!=str(root.resolve())
            or file.is_symlink() or not file.is_file() or file.stat().st_size!=row['bytes']
            or digest(file)!=row['sha256']):raise ValueError('Original input differs: '+str(relative))
        result[row['path']]=row['sha256']
    return result



def probe_inputs(manifest, catalog, probe_id):
    """先核原情景能力与具名门是否相容；Continue缺能力仍失败，绝不自动改成direct。"""
    if (manifest.get('format')!='HANMO-NATIVE-BANK-INPUTS/2'
        or manifest.get('probe_ids')!=sorted(PROBES)
        or manifest.get('scenario_id')!=SCENARIO or probe_id not in PROBES):
        raise ValueError('Exact bounded probe input manifest required')
    scenario=catalog.get('data',{}).get('scenarios',{}).get(SCENARIO)
    if not isinstance(scenario,dict):raise ValueError('Original bank scenario missing')
    profile=scenario.get('player_plan_profile')
    if profile not in (None,'known_life_v1'):raise ValueError('Unknown original PlayerPlan profile')
    if probe_id==PROBE and profile!='known_life_v1':
        raise ValueError('Original bank has no PlayerPlan capability; Continue probe incompatible')
    args,scope=PROBES[probe_id]
    return {'scenario_id':SCENARIO,'player_plan_profile':profile,
            'probe_id':probe_id,'argv_suffix':list(args),'expected_scope':scope}


def git(root, *args):
    """固定产物源码与探针工具源码分开记录，不能把后继dirty冒同代头根。"""
    return subprocess.check_output(['git','-C',str(root),*args],text=True).strip()


def symbols(archive):
    """从这份正式库找恰一原apply/step符号；不硬抄另一编译器的旧库地址。"""
    raw=subprocess.check_output(['nm','-g','--defined-only',str(archive)],text=True)
    result={}
    for key,stem in [('APPLY','_ZN5hanmo8resident18apply_naval_repairE'),
                     ('STEP','_ZN5hanmo8resident17step_naval_repairE')]:
        found={line.split()[-1] for line in raw.splitlines()
               if len(line.split())>=3 and line.split()[-2] in ('T','W')
               and line.split()[-1].startswith(stem)}
        if len(found)!=1:raise ValueError('Exact repair wrapper symbol unavailable: '+key)
        result[key]=next(iter(found))
    return result


def execute(args):
    """完成固定输入/五产物预验、独立probe链接运行及不可变后验，任何红保原出口。"""
    req=request(args.request);root=args.source.resolve();operator=args.operator.resolve()
    artifacts=args.artifacts.resolve();out=args.output.resolve()
    if out.exists():raise ValueError('Fresh risk output required')
    out.mkdir(parents=True)
    receipt={'format':'HANMO-NATIVE-BANK-RISK-RESULT/1','status':'RUNNING','request':req,
             'started_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'stages':[]}
    receipt_path=out/'risk-receipt.json'
    def save():receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
    def run(name, command):
        stage={'name':name,'argv':list(map(str,command))};receipt['stages'].append(stage);save()
        with (out/(name+'.log')).open('w') as log:
            completed=subprocess.run(stage['argv'],stdout=log,stderr=subprocess.STDOUT)
        stage['exit_code']=completed.returncode;save()
        if completed.returncode:raise RuntimeError(name+' failed: '+str(completed.returncode))
    expected={};before=None;manifest=None
    try:
        if git(root,'rev-parse','HEAD')!=req['source_commit'] or git(root,'status','--porcelain=v1'):
            raise ValueError('Exact clean producer header root required')
        if git(operator,'status','--porcelain=v1'):raise ValueError('Clean frozen probe/input source required')
        receipt['probe_source_commit']=git(operator,'rev-parse','HEAD')
        verifier=root/'tools/ci/verify_native_outputs.py'
        spec=importlib.util.spec_from_file_location('fixed_native_outputs',str(verifier))
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        before=module.verify(artifacts,req['source_commit']);receipt['native_before']=before
        manifest=json.loads((operator/'tools/ci/native_bank_risk_inputs.json').read_text())
        rows=manifest['inputs']+manifest['probe_sources'];expected=files(operator,rows)
        receipt['input_and_probe_sha256']=expected
        original=operator/'verification/system-battle-military/naval3-active-cold-48ba224'
        route=probe_inputs(manifest,json.loads((original/'inputs/catalog.json').read_text()),req['probe_id'])
        receipt['input_profile_and_scope']=route
        wraps=symbols(artifacts/'libhanmo_resident.a');receipt['wrapped_symbols']=wraps
        compiler=subprocess.check_output(['g++','-dumpversion'],text=True).strip()
        if compiler.split('.')[0]!='9':raise ValueError('Original Ubuntu20 GCC9 ABI compiler required')
        receipt['compiler']=compiler
        command=['g++','-std=c++17','-O0','-g0','-pipe','-UNDEBUG',
                 '-I'+str(root/'native/world/include'),'-I'+str(root/'native/world/src')]
        for key,symbol in wraps.items():
            command+=['-DREAL_NAVAL_REPAIR_'+key+'="__real_'+symbol+'"',
                      '-DWRAP_NAVAL_REPAIR_'+key+'="__wrap_'+symbol+'"','-Wl,--wrap='+symbol]
        command+=[str(operator/'native/world/tests/resident/naval_repair_resume_probe.cpp'),
                  '-Wl,--start-group',str(artifacts/'libhanmo_resident.a'),
                  str(artifacts/'libhanmo_world.a'),str(artifacts/'libhanmo_flecs.a'),
                  '-Wl,--end-group','-lpthread','-ldl','-o',str(out/'naval-repair-resume')]
        run('compile-probe',command)
        run('bank-probe',[out/'naval-repair-resume',original/'inputs/catalog.json',
                         original/'run-01/failure.hmcb',original/'run-01/formed.hmcb',out/'run-01']+route['argv_suffix'])
        result=json.loads((out/'run-01/result.json').read_text())
        if result.get('ok') is not True or result.get('scope')!=route['expected_scope']:
            raise ValueError('Exact named bank risk probe result missing')
        receipt['probe_result']=result;receipt['status']='PASS_NAMED_BANK_RISK_ONLY'
    except Exception as error:
        receipt['status']='FAILED';receipt['error']=str(error)
    finally:
        try:
            if before is not None:
                after=module.verify(artifacts,req['source_commit']);receipt['native_after']=after
                if after!=before:raise ValueError('Native artifact identity changed')
            if expected and files(operator,manifest['inputs']+manifest['probe_sources'])!=expected:
                raise ValueError('Original input changed')
            if git(root,'rev-parse','HEAD')!=req['source_commit'] or git(root,'status','--porcelain=v1'):
                raise ValueError('Producer source changed')
            if git(operator,'status','--porcelain=v1'):raise ValueError('Probe source changed')
            receipt['postcheck']='PASS'
        except Exception as error:
            receipt['status']='FAILED';receipt['postcheck_error']=str(error)
        receipt['completed_at']=datetime.datetime.now(datetime.timezone.utc).isoformat();save()
    return 0 if receipt['status']=='PASS_NAMED_BANK_RISK_ONLY' else 1


def main():
    """三个有限入口：校请求、选唯一已成功run产物、运行原银行探针。"""
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='mode',required=True)
    req=sub.add_parser('request');req.add_argument('--request',type=Path,required=True)
    sel=sub.add_parser('select');sel.add_argument('--request',type=Path,required=True)
    sel.add_argument('--run',type=Path,required=True);sel.add_argument('--artifacts',type=Path,required=True)
    exe=sub.add_parser('execute')
    for name in ['request','source','operator','artifacts','output']:exe.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    if args.mode=='request':print(json.dumps(request(args.request)));return 0
    if args.mode=='select':
        value=select(request(args.request),json.loads(args.run.read_text()),json.loads(args.artifacts.read_text()))
        for key,item in value.items():print(str(key)+'='+str(item))
        return 0
    return execute(args)

if __name__=='__main__':raise SystemExit(main())
