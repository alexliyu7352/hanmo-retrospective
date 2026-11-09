#!/usr/bin/env python3
"""复用固定CI五件与原缓存准备首店Installed，再仅链接原资产/冷档/事务探针。"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

SOURCE = 'ed44ea150a37533da94cb5f16fc5fa52a48bdb25'
RUN = 38224828405
REPOSITORY = '（原私有仓库）'
SCENARIO = 'private_shop::scenario.first_shop'
PLAYER = 'society::actor.apprentice'
SCOPE = 'ORIGINAL_FIRST_SHOP_DAILY_RENT_ASSET_COLD_TRANSACTION_ONLY'
EXPORTER = 'tools/private_shop_export_inputs.gd'
EXPORTER_SHA = 'd2cd6ecffebc3a94b5d12ab18d202d9f4ef79dd741f5317076af9277b4c5a4c7'
PROBES = {
 'native/world/tests/resident/economic_world/private_shop_live_probe.cpp': '5811b374af6010a9702f5e55ffe2fbd83ae52112d600a295842bd85da45a7fea',
 'native/world/tests/resident/economic_world/production_disruption_live_probe.cpp': 'd3542c7458e16fb6c9f33752a253ec908eb156092a643bb3385c69dfde9101a2',
}
# 执行探针仅纠正原交互/typed报价协议；产品两源SHA继续原样验证。
TOOL_PROBES = dict(PROBES)
TOOL_PROBES['native/world/tests/resident/economic_world/private_shop_live_probe.cpp'] = '222ddafc0f06866d494978b76ff22c82af27bf3a72990edec3ca74334c5cf5c6'
PROBE = 'first-shop-daily-rent-protocol4'
PACKS = ['content/packs/authored/local_society_1_8', 'content/packs/authored/private_shops_1_1']
SELECTION = 'content/packs/selections/private-shops-1.1.0.json'

# 固定产品配方各自保留原源；后继只验证所选正式产物的同一首店资产门。
PROFILES = {
    'original-protocol4': {'source': SOURCE, 'run': RUN, 'probe': PROBE,
                           'probes': dict(PROBES), 'cache_attempt': False},
    'reservation-owner-fix': {
        'source': '67cc70a2b126a68c6ffe4ca3f4bb73f80c53ec80', 'run': 38240212288,
        'probe': 'first-shop-reservation-owner-fix', 'probes': dict(TOOL_PROBES),
        'cache_attempt': True},
    'settlement-phase-fix': {
        'source': 'c7fd158d68b58abea84790098e6549cd0b8add0d', 'run': 38250753551,
        'probe': 'first-shop-settlement-phase-fix', 'probes': dict(TOOL_PROBES),
        'cache_attempt': True},
}
ACTIVE_PROFILE = 'original-protocol4'


def activate_profile(name):
    """每进程只选择一个具名常量配方；请求内容不能自由指定产品/run/探针。"""
    global SOURCE, RUN, PROBE, PROBES, ACTIVE_PROFILE
    if name not in PROFILES: raise ValueError('Unknown bounded shop profile')
    value=PROFILES[name]
    SOURCE=value['source'];RUN=value['run'];PROBE=value['probe'];PROBES=dict(value['probes'])
    ACTIVE_PROFILE=name


def sha(path):
    """摘要绑定普通原件，拒绝用链接扩大输入边界。"""
    path = Path(path)
    if path.is_symlink() or not path.is_file(): raise ValueError('Regular input required: ' + str(path))
    return hashlib.sha256(path.read_bytes()).hexdigest()


def request(key):
    """仅接原run实际保存的精确cache key；产品代次、仓库与门的范围不能由自由参数改变。"""
    suffix = ('-run-'+str(RUN)+r'-attempt-[1-9][0-9]*') if PROFILES[ACTIVE_PROFILE]['cache_attempt'] else ''
    if not isinstance(key, str) or not re.fullmatch(r'ubuntu2004-native-[0-9a-f]{64}-' + SOURCE + suffix, key):
        raise ValueError('Exact selected native cache key required')
    return {'format':'HANMO-PRIVATE-SHOP-RISK/1', 'repository':REPOSITORY,
            'native_run_id':RUN, 'source_commit':SOURCE, 'native_cache_key':key,
            'probe_id':PROBE, 'scope':SCOPE}


def read_request(path):
    """重新核请求全形状；未知字段及重定向原件都不放行。"""
    value = json.loads(path.read_text())
    if value != request(value.get('native_cache_key')): raise ValueError('Bounded shop request differs')
    return value


def load(path, name):
    """按明确脚本路径加载原固定工具，不让后继工具冒产品源。"""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def git(root, *args):
    """读取真实Git身份；无法证明固定干净头时立即失败。"""
    env=dict(os.environ,GIT_ALLOW_PROTOCOL='',GIT_TERMINAL_PROMPT='0')
    return subprocess.check_output(['git','-C',str(root),*args],text=True,env=env).strip()


def git_metadata(root):
    """逐原树核所有属性/忽略文件，补齐稀疏祖目录元数据后才允许离线净源核验。"""
    rows=[]
    for entry in git(root,'ls-tree','-r','-z','HEAD').split('\0'):
        if not entry:continue
        header,name=entry.split('\t',1)
        if Path(name).name not in ('.gitignore','.gitattributes'):continue
        mode,kind,expected=header.split()
        path=root/name
        if kind!='blob' or mode not in ('100644','100755') or path.is_symlink() or not path.is_file():
            raise ValueError('Original sparse Git metadata missing: '+name)
        data=path.read_bytes()
        actual=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        if actual!=expected:raise ValueError('Original sparse Git metadata changed: '+name)
        rows.append({'path':name,'blob':expected})
    return rows


def source_identity(source, operator):
    """产品按显式固定配方选择，导出工具原字节及两份实际Git头独立保留。"""
    metadata={'product':git_metadata(source),'tool':git_metadata(operator)}
    if git(source,'rev-parse','HEAD') != SOURCE or git(source,'status','--porcelain=v1'):
        raise ValueError('Exact clean selected product source required')
    if git(operator,'status','--porcelain=v1'): raise ValueError('Clean tool source required')
    if sha(operator/EXPORTER) != EXPORTER_SHA: raise ValueError('Original 33f57 exporter differs')
    for path, expected in PROBES.items():
        if sha(source/path) != expected: raise ValueError('Original shop probe differs: '+path)
    for path, expected in TOOL_PROBES.items():
        if sha(operator/path) != expected: raise ValueError('Named tool-side shop probe differs: '+path)
    return {'product_source':SOURCE, 'tool_source':git(operator,'rev-parse','HEAD'),
            'exporter_sha256':EXPORTER_SHA, 'probe_sha256':dict(PROBES),'execution_probe_sha256':dict(TOOL_PROBES),'git_metadata':metadata}


def cached_runtime(source, cache, receipt):
    """只消费原缓存的固定引擎与四许可，核锁、原receipt和原绝对根；不下载或改回执。"""
    if str(cache) != '/cache': raise ValueError('Original receipt cache mount must remain /cache')
    lock = json.loads((source/'native/dependencies.lock.json').read_text())
    engine = cache/'Godot_v4.7.2-stable_linux.x86_64'
    expected = lock['godot_bridge']['sdk']['engine_sha256']
    if receipt.get('engine',{}).get('sha256') != expected or sha(engine) != expected:
        raise ValueError('Original fixed engine identity differs')
    rows = {'engine':{'path':str(engine),'sha256':expected}}
    for name, relative in [('flecs','LICENSE'),('json','LICENSE.MIT'),('picosha2','LICENSE'),('godot-cpp','LICENSE.md')]:
        spec = lock['godot_bridge']['binding'] if name == 'godot-cpp' else lock['libraries'][name]
        original = receipt.get('dependencies',{}).get(name,{})
        directory = cache/'deps'/name
        if original.get('root') != str(directory) or original.get('files') != spec['files']:
            raise ValueError('Original dependency receipt differs: '+name)
        expected = spec['files'][relative]
        if sha(directory/relative) != expected: raise ValueError('Locked distribution license differs: '+name)
        rows[name] = {'path':str(directory/relative),'sha256':expected}
    return rows


def exported_inputs(directory):
    """检查原导出对象的实际情景与日租能力，不增添profile或修改请求。"""
    catalog = json.loads((directory/'catalog.json').read_text())
    native = json.loads((directory/'request.json').read_text())
    identity = native.get('expected_catalog_identity',{})
    modules = identity.get('module_versions',[])
    names = [item.get('name') for item in modules]
    if (native.get('scenario_id') != SCENARIO or native.get('founder_selection') != PLAYER
        or not catalog.get('ok') or SCENARIO not in catalog.get('data',{}).get('scenarios',{})
        or not catalog.get('pack_set_digest') or identity.get('pack_set_digest') != catalog['pack_set_digest']
        or len(names) != len(set(names))
        or {'name':'private_shop_daily_rent','version':'1'} not in modules):
        raise ValueError('Original Installed first-shop profile differs')
    return {name:sha(directory/name) for name in ('catalog.json','request.json')}


def validate_result(value):
    """只认可这份原首店探针的明确成功范围；不扩大到Plan、出生或GUI。"""
    if (value.get('ok') is not True or value.get('real_world') is not True
        or value.get('daily_rent_tested') is not True or value.get('gui_tested') is not False
        or value.get('same_player') != PLAYER or type(value.get('checks')) is not int
        or value['checks'] <= 0): raise ValueError('Named first-shop result incomplete')


class Journal:
    """每个实际阶段落原argv和退出；失败后保留首红并独立后验。"""
    def __init__(self, output, req, phase):
        if output.exists(): raise ValueError('Fresh output required: '+str(output))
        output.mkdir(parents=True)
        self.output = output
        self.value = {'format':'HANMO-PRIVATE-SHOP-RISK-RESULT/1','phase':phase,'request':req,
                      'status':'RUNNING','started_at':self.now(),'stages':[]}
        self.save()
    def now(self):
        return datetime.datetime.now(datetime.timezone.utc).isoformat()
    def save(self):
        (self.output/'receipt.json').write_text(json.dumps(self.value,indent=2)+'\n')
    def run(self, name, command, cwd=None):
        row={'name':name,'argv':list(map(str,command)),'started_at':self.now()}
        self.value['stages'].append(row); self.save()
        with (self.output/(name+'.log')).open('w') as stream:
            result=subprocess.run(row['argv'],cwd=cwd,stdout=stream,stderr=subprocess.STDOUT)
        row.update(exit_code=result.returncode,completed_at=self.now());self.save()
        if result.returncode: raise RuntimeError(name+' failed: '+str(result.returncode))


def execute(args):
    """分开正常输入准备与Ubuntu20单probe执行；两阶段都核五件与原源的前后身份。"""
    req=read_request(args.request);source=args.source.resolve();operator=args.operator.resolve()
    artifacts=args.artifacts.resolve();output=args.output.resolve()
    journal=Journal(output,req,args.command);before=None;origin=None;inputs=None;runtime=None
    verifier=None
    try:
        verifier=load(source/'tools/ci/verify_native_outputs.py','fixed_native_verifier')
        origin=source_identity(source,operator);journal.value['sources']=origin
        before=verifier.verify(artifacts,SOURCE);journal.value['native_before']=before
        if args.command=='prepare':
            receipt=json.loads((artifacts/'native-build-receipt.json').read_text())
            runtime=cached_runtime(source,args.cache.resolve(),receipt);journal.value['cached_runtime']=runtime
            engine=runtime['engine']['path']
            actual=subprocess.check_output([engine,'--headless','--version'],text=True).strip()
            lock=json.loads((source/'native/dependencies.lock.json').read_text())
            if actual!=lock['godot_bridge']['sdk']['version']:raise ValueError('Fixed engine version differs')
            compose=output/'compose'
            journal.run('compose',[sys.executable,source/'tools/content_pipeline.py','compose',
                '--packs',source/PACKS[0],'--packs',source/PACKS[1],'--selection',source/SELECTION,'--output',compose],source)
            sys.path.insert(0,str(source));from tools import native_resident_profile
            pack=json.loads((compose/'pack-set.lock.json').read_text())
            pack_digest=hashlib.sha256(native_resident_profile.content_stage.canonical(pack)).hexdigest()
            config={'format':'HM-NATIVE-RESIDENT-RELEASE/1','export_mode':'debug',
              'engine':{'version':actual,'sha256':runtime['engine']['sha256']},
              'native':{'source_commit':SOURCE,'library_sha256':sha(artifacts/'libhanmo_world_bridge.so')},
              'content':{'pack_set_digest':pack_digest,'scenario_id':SCENARIO},
              'storage':{'profile':'risk-private-shop','custom_user_dir_name':'risk-private-shop'}}
            config_path=output/'profile.json';config_path.write_text(json.dumps(config,indent=2)+'\n')
            # 仍调用原prepare完整源码/库/许可门；生成profile不是回写原生产receipt。
            prepared=native_resident_profile.prepare_profile(source,output/'stage',artifacts/'libhanmo_world_bridge.so',
                [compose],native_build_receipt=artifacts/'native-build-receipt.json',profile_config=config_path)
            journal.value['stage_manifest_sha256']=sha(output/'stage/native-resident-stage.json');journal.save()
            game=Path(prepared['game']);export=output/'inputs'
            journal.run('import',[engine,'--headless','--editor','--path',game,'--import'],output)
            journal.run('export-inputs',[engine,'--headless','--path',game,'--script',operator/EXPORTER,'--',
                prepared['manifest']['content']['catalog_path'],export],output)
            inputs=exported_inputs(export);journal.value['inputs']=inputs
            checked=native_resident_profile.validate_profile(source,game)
            if checked['manifest']!=prepared['manifest']:raise ValueError('Prepared profile changed')
            journal.value['status']='INPUTS_PREPARED_RUNTIME_NOT_RUN'
        else:
            preparation=json.loads((args.prepared/'receipt.json').read_text())
            if (preparation.get('status')!='INPUTS_PREPARED_RUNTIME_NOT_RUN' or preparation.get('request')!=req
                or preparation.get('sources')!=origin or preparation.get('native_after')!=before):
                raise ValueError('Exact successful preparation receipt required')
            directory=args.prepared/'inputs';inputs=exported_inputs(directory)
            if inputs!=preparation.get('inputs'):raise ValueError('Installed exported inputs changed')
            journal.value['inputs']=inputs
            compiler=subprocess.check_output(['g++','-dumpversion'],text=True).strip()
            if compiler.split('.')[0]!='9':raise ValueError('Original Ubuntu20 GCC9 ABI required')
            journal.value['compiler']=compiler
            command=['g++','-std=c++17','-O0','-g0','-pipe','-UNDEBUG',
              '-I'+str(source/'native/world/include'),'-I'+str(source/'native/world/src'),
              operator/next(iter(TOOL_PROBES)),'-Wl,--start-group',artifacts/'libhanmo_resident.a',
              artifacts/'libhanmo_world.a',artifacts/'libhanmo_flecs.a','-Wl,--end-group','-lpthread','-ldl','-o',output/'first-shop-probe']
            journal.run('compile-probe',command)
            journal.run('shop-probe',[output/'first-shop-probe',directory/'catalog.json',directory/'request.json',output/'bank-run'])
            result=json.loads((output/'bank-run/result.json').read_text());validate_result(result)
            journal.value.update(status='PASS_NAMED_SHOP_RISK_ONLY',scope=SCOPE,result=result)
    except Exception as error:
        journal.value.update(status='FAILED',error=str(error))
    finally:
        try:
            if before is not None:
                after=verifier.verify(artifacts,SOURCE);journal.value['native_after']=after
                if before!=after:raise ValueError('Original five outputs changed')
            if origin is not None and source_identity(source,operator)!=origin:raise ValueError('Original sources changed')
            if inputs is not None:
                directory=output/'inputs' if args.command=='prepare' else args.prepared/'inputs'
                if exported_inputs(directory)!=inputs:raise ValueError('Original exported inputs changed')
            if runtime is not None:
                current=cached_runtime(source,args.cache.resolve(),json.loads((artifacts/'native-build-receipt.json').read_text()))
                if current!=runtime:raise ValueError('Original runtime cache inputs changed')
            journal.value['postflight']='PASS' if before is not None or origin is not None else 'NOT_REACHED'
        except Exception as error:
            journal.value.update(status='FAILED',postflight='FAILED',postflight_error=str(error))
        journal.value['completed_at']=journal.now();journal.save()
    return 0 if journal.value['status'] in ('INPUTS_PREPARED_RUNTIME_NOT_RUN','PASS_NAMED_SHOP_RISK_ONLY') else 1


def main():
    """入口只公开固定请求、原artifact选择和两个具名阶段，不接受命令或URL输入。"""
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('request');p.add_argument('--cache-key',required=True)
    p=sub.add_parser('identity')
    for name in ('source','operator'):p.add_argument('--'+name,type=Path,required=True)
    p=sub.add_parser('select')
    for name in ('request','run','artifacts'):p.add_argument('--'+name,type=Path,required=True)
    for phase in ('prepare','execute'):
        p=sub.add_parser(phase)
        for name in ('request','source','operator','artifacts','output'):p.add_argument('--'+name,type=Path,required=True)
        p.add_argument('--'+('cache' if phase=='prepare' else 'prepared'),type=Path,required=True)
    for child in sub.choices.values():
        child.add_argument('--profile',choices=tuple(PROFILES),default='original-protocol4')
    args=parser.parse_args()
    activate_profile(args.profile)
    if args.command=='request':print(json.dumps(request(args.cache_key)));return 0
    if args.command=='identity':print(json.dumps(source_identity(args.source.resolve(),args.operator.resolve())));return 0
    if args.command=='select':
        helper=load(Path(__file__).with_name('native_bank_risk.py'),'original_bank_selection')
        result=helper.select(read_request(args.request),json.loads(args.run.read_text()),json.loads(args.artifacts.read_text()))
        for key,value in result.items():print(str(key)+'='+str(value))
        return 0
    return execute(args)


if __name__=='__main__':
    raise SystemExit(main())
