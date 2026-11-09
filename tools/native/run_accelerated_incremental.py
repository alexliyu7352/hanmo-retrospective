"""新run显式.d/PCH/ccache执行桥；原N1原文件和资源policy保持不变，默认只准备。"""
from pathlib import Path
from collections.abc import Mapping
import argparse
import copy
import datetime
import fcntl
import importlib.util
import json
import os
import shlex
import subprocess
import sys
sys.dont_write_bytecode=True


def module(name,path):
    """只加载同目录具名新实现或请求SHA锁定的原N1，不搜索环境模块名。"""
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


D=module('depfile_incremental',Path(__file__).with_name('depfile_incremental.py'))
A=module('build_acceleration',Path(__file__).with_name('build_acceleration.py'))
P=module('paired_resolution',Path(__file__).with_name('paired_resolution.py'))
C=module('execution_context',Path(__file__).with_name('execution_context.py'))
require=D.require


def save(path,value):
    """新run原件只写一次；既有终态、失败和输出目录不能覆盖。"""
    with Path(path).open('x') as stream:json.dump(value,stream,ensure_ascii=False,indent=2);stream.write('\n')


def final_decision(classification,pair):
    """最终执行集合只含COMPILE/REUSE，未知不会作为隐式采用留下。"""
    if classification['decision']=='NEEDS_RESOLUTION_PROOF':
        if pair and pair.get('accepted'):
            return {'decision':'REUSE','reasons':[], 'paired_proof':pair}
        return {'decision':'COMPILE','reasons':classification['reasons']+['resolution_proof_missing_or_rejected']+(pair or {}).get('reasons',[])}
    return classification


def pch_header(row):
    """只认CMake真实-force-include wrapper，未支持隐式PCH搜索则拒绝。"""
    command=row['command'];headers=[command[i+1] for i,x in enumerate(command[:-1]) if x=='-include' and 'cmake_pch' in command[i+1]]
    require(len(headers)<=1,'multiple_PCH_wrappers_unsupported')
    return headers[0] if headers else None


def partition_phases(queue):
    """真实CMake PCH创建动作先序；缺原编译规则不能凭扩展名造命令。"""
    pch=[];units=[]
    for row in queue:
        D.canonical_action(row)
        command=row['command'];is_pch='-x' in command and command[command.index('-x')+1]=='c++-header'
        (pch if is_pch else units).append(row)
    headers={pch_header(row):row for row in pch}
    require(None not in headers and len(headers)==len(pch),'unique_actual_PCH_creation_required')
    for row in units:
        header=pch_header(row)
        require(header is None or header in headers,'PCH_use_without_actual_CMake_creation')
    return {'pch':pch,'translation_units':units}


def wrap_action(row,profile,python,launcher):
    """新wrapper逐项绑定，原canonical argv另存；不把新前缀伪称原命令。"""
    result=copy.deepcopy(row);result['canonical_command']=list(row['command'])
    result['command']=[python,launcher,'--profile',profile['path'],'--sha256',profile['sha256'],'--',*row['command']]
    return result


def compiler_history(history):
    """只允许证实miss的原编译峰值，缓存hit和未分类行不缩小未来配额。"""
    return [row for row in history if A.compiler_peak_eligible(row) or
            (row.get('exit_code') not in (None,0) and (row.get('packed_memory_guard_stop') or
             row.get('bounded_memory_guard_stop') or row.get('timeout_seconds') or row.get('oom_or_signal_evidence') or
             row.get('exit_code') in (-9,-15,124,137,143)))]


ORIGINAL_N1_SHA256='20fd65a249bc4e121a68855d50b170dc984088aff4c1dda10ed4d4a6177d6cb3'


def original_uncached_history(spec,reader):
    """原N1固定forced-CCACHE_DISABLE历史只作RSS估计，不充当本代对象采用或cachehit证据。"""
    if not spec:return []
    require(spec.get('kind')=='ORIGINAL_UNCACHED_N1_BUDGET_HISTORY','uncached_history_authority_required')
    reader.checked(spec['scheduler'],json_value=False)
    require(spec['scheduler']['sha256']==ORIGINAL_N1_SHA256,'original_forced_cache_disable_scheduler_required')
    drivers={x['path']:x for x in spec['compiler_identities']}
    for ref in drivers.values():A.verify_file(ref)
    timings={x['path']:x for x in spec['time_logs']}
    result=[]
    for ref in spec['result_files']:
        for row in reader.checked(ref):
            cmd=row.get('original_command',row.get('command',[]))
            if row.get('exit_code') is None or not cmd or cmd[0] not in drivers or row.get('cache_outcome') is not None:continue
            require('-c' in cmd and cmd[cmd.index('-c')+1]==row['source'],'uncached_history_original_input_required')
            require(row['time_log'] in timings,'uncached_history_original_GNUtime_lock_required')
            reader.checked(timings[row['time_log']],json_value=False)
            result.append(row)  # 原行不加伪cache_miss；未改N1继续核GNUtime数值及历史frozen身份。
    return result


def may_continue_batch(result,policy,phase):
    """普通TU错误按原keep-going继续后批；PCH失败/资源停/未完成动作始终禁止后依赖执行。"""
    return (result['status']=='complete' or
            (phase=='translation_units' and result['status']=='failed' and policy.get('keep_going_compile_errors') is True
             and not result.get('pending') and all(r.get('exit_code')==0 or r.get('continued_after_compile_error') is True for r in result['results'])))


def verify_cmake_queue(queue,commands):
    """实际CMake command每token核；源和输出只作同cwd绝对化，不删除任意launcher。"""
    actual=[]
    for item in commands:
        cwd=str(Path(item['directory']).resolve());cmd=item.get('arguments') or shlex.split(item['command'])
        require(cmd.count('-c')==cmd.count('-o')==1,'actual_CMake_single_compile_required')
        for flag in ('-c','-o'):
            i=cmd.index(flag);cmd[i+1]=os.path.abspath(os.path.join(cwd,cmd[i+1]))
        actual.append({'source':cmd[cmd.index('-c')+1],'object':cmd[cmd.index('-o')+1],'cwd':cwd,'command':cmd})
    require(len({r['object'] for r in queue})==len(queue),'duplicate_selected_object')
    for row in queue:require(actual.count(D.canonical_action(row))==1,'selected_action_not_exact_CMake')


def verify_context(context,reader):
    """当前技术身份真实文件逐SHA与有效子环境绑定；不生成历史控制字段。"""
    require(context.get('kind')=='CURRENT_EXECUTION_RESOLUTION_IDENTITY' and context.get('complete_system_headers') is True,'complete_current_execution_identity_required')
    D.semantic_context(context)
    derivation=reader.checked(context['derivation_request'])
    require({k:v for k,v in context.items() if k!='derivation_request'}==C.derive(derivation,reader),'context_not_exact_current_derivation')
    require(context['environment']==A.environment_identity(dict(os.environ,LC_ALL='C')),'effective_N1_environment_changed')
    require(context.get('files') and context.get('resolved_subprograms') and context.get('search_evidence'),'resolved_tools_and_search_evidence_required')
    for ref in [*context['files'],*context['resolved_subprograms'],context['search_evidence']]:reader.checked(ref,json_value=False)


class CheckedStateIndex(Mapping):
    """仅持object→原件ref；每次访问沿原SHA/stat门重读，绝不缓存全部state JSON。"""
    def __init__(self,refs,reader):
        self.references={};self.reader=reader
        for ref in refs:
            state=reader.checked(ref,retain_json=False);obj=state['action']['object']
            require(obj not in self.references,'duplicate_old_action_state')
            self.references[obj]=dict(ref);del state

    def __getitem__(self,obj):
        state=self.reader.checked(self.references[obj],retain_json=False)
        require(state['action']['object']==obj,'indexed_state_object_changed')
        return state

    def __iter__(self):return iter(self.references)

    def __len__(self):return len(self.references)


def prepare(request,root,reader,*,write=True):
    """只读生成逐对象计划；任何新CMake source独立编，不用任意新增文件全编开关。"""
    context=reader.checked(request['context']);verify_context(context,reader)
    require(request['source_root']==context['source_root'] and
            context['acceleration_semantics']==A.acceleration_semantics(request['ccache'],request['source_root'],request['cache_mode']),
            'request_acceleration_context_mismatch')
    queue=reader.checked(request['queue']);verify_cmake_queue(queue,reader.checked(request['cmake_commands']))
    phases=partition_phases(queue);states=CheckedStateIndex(request.get('states',[]),reader)
    bundles=request.get('resolution_proofs',{});evidence=[];rebuild=[];reuse=[]
    for row in queue:
        state=states.get(row['object']);classification=D.classify(row,state,context)
        pair=P.verify_pair(row,state,context,bundles.get(row['object']),reader) if classification['decision']=='NEEDS_RESOLUTION_PROOF' else None
        decision=final_decision(classification,pair);decision.update(source=row['source'],object=row['object']);evidence.append(decision)
        (reuse if decision['decision']=='REUSE' else rebuild).append(row)
        del state
    # 创建PCH变化后依赖的实际对象只能在新PCH身份下编/命中缓存，不提前采用旧PCH相关输出。
    rebuilt_pch={pch_header(r) for r in phases['pch'] if r in rebuild}
    for row in list(reuse):
        if pch_header(row) in rebuilt_pch and pch_header(row) is not None:
            reuse.remove(row);rebuild.append(row)
            item=next(x for x in evidence if x['object']==row['object']);item.update(decision='COMPILE',reasons=['actual_PCH_creation_rebuilt'])
    frozen=reader.checked(request['frozen_inputs'])
    require(request['frozen_inputs']['sha256']==context['frozen_manifest_sha256'],'context_source_freeze_mismatch')
    locked={x['path']:x['sha256'] for x in frozen['files']}
    for ref in frozen['files']:reader.checked(ref,json_value=False)
    for row in queue:
        require(row['source'] in locked and D.sha(row['source'])==locked[row['source']]==row['source_sha256'],'selected_source_outside_freeze')
    for name in ('scheduler','python','launcher','policy','gnu_time','cache_snapshot_helper'):
        reader.checked(request[name],json_value=False)
    require(request['scheduler']['sha256']==ORIGINAL_N1_SHA256,'unchanged_original_N1_required')
    required_helpers={str(Path(__file__).resolve()),str(Path(__file__).with_name('depfile_incremental.py').resolve()),
                      str(Path(__file__).with_name('paired_resolution.py').resolve()),str(Path(__file__).with_name('build_acceleration.py').resolve()),
                      str(Path(__file__).with_name('cache_snapshot.py').resolve()),str(Path(__file__).with_name('execution_context.py').resolve())}
    helper_refs=request.get('implementation_files',[])
    require({x['path'] for x in helper_refs}==required_helpers,'complete_new_implementation_lock_required')
    for ref in [*helper_refs,request['ccache'],*context.get('files',[]),*context.get('resolved_subprograms',[])]:
        reader.checked(ref,json_value=False)
        require(locked.get(ref['path'])==ref['sha256'],'technical_or_implementation_input_outside_N1_freeze')
    original_uncached_history(request.get('original_uncached_history'),reader)
    protection=reader.checked(request['overwrite_protection'])
    by_original={x['original_path']:x for x in protection['files']}
    for row in rebuild:
        for path in (row['object'],row['object']+'.d'):
            if Path(path).exists():
                item=by_original[path];snapshot=Path(item['snapshot_path']);a=Path(path).stat();b=snapshot.stat()
                require((a.st_dev,a.st_ino)!=(b.st_dev,b.st_ino) and D.sha(path)==D.sha(snapshot)==item['sha256'],'independent_overwrite_protection_required')
    for row in rebuild:
        row['source_sha256']=locked[row['source']]
    result={'kind':'ACTUAL_DEPFILE_PLAN_NOT_EXECUTED','total':len(queue),'pch_actions':len(phases['pch']),
            'translation_units':len(phases['translation_units']),'rebuild':rebuild,'reused':reuse,'evidence':evidence,
            'compile_count':len(rebuild),'reused_count':len(reuse),'new_frozen_paths_are_not_global_invalidation':True}
    if write:save(root/'plan.json',result)
    return result,context,states


def make_profiles(request,root,rows,context,reader,pch_receipts):
    """每真实driver/相位一份不可变profile；PCH使用只在真实创建终态后绑定bytes。"""
    result={};groups={}
    for row in rows:groups.setdefault((row['command'][0],bool(pch_header(row))),[]).append(row)
    for number,((driver,uses_pch),actions) in enumerate(groups.items()):
        base=root/f'profile-{number:02d}';base.mkdir()
        compiler=A.file_identity(driver)
        require(compiler in request['compiler_identities'],'unregistered_compiler_driver')
        ccache=request['ccache'];A.verify_file(ccache)
        profile={'schema_version':1,'mode':request['cache_mode'] if uses_pch else 'strict','base_dir':request['source_root'],
                 'cache_dir':request['cache_dir'],'max_size':request['cache_max_size'],'evidence_dir':str(base/'evidence'),
                 'compiler':compiler,'ccache':ccache,'environment':context['environment'],'toolchain_sha256':context['toolchain_sha256'],
                 'execution_context_identity':A.file_identity(request['context']['path']),
                 'technical_inputs':[A.file_identity(request['context']['path']),A.file_identity(request['frozen_inputs']['path'])],
                 'toolchain_files':[A.file_identity(ref['path']) for ref in context['resolved_subprograms']], 'actions':[],
                 'working_directories':sorted({row['cwd'] for row in actions})}
        (base/'evidence').mkdir()
        if profile['mode']=='guarded_pch':
            approval=request['guarded_pch_policy_by_driver'][driver];policy=reader.checked(approval);profile['guarded_pch_approval']=A.file_identity(approval['path'])
            profile['volatile_guard']=policy['volatile_guard'];A.verify_file(profile['volatile_guard'])
        for row in actions:
            command=row['command'];kind='pch_create' if '-x' in command and command[command.index('-x')+1]=='c++-header' else 'tu'
            action={**D.canonical_action(row),'kind':kind,'input_files':[A.file_identity(row['source'])],'pch_inputs':[]}
            header=pch_header(row)
            if header and kind!='pch_create':
                saved=pch_receipts[header];raw=reader.checked(saved['receipt'])
                action.update(pch_inputs=[A.file_identity(saved['object'])],pch_creation_receipt=A.file_identity(saved['receipt']['path']),pch_creation_inputs=raw['dependency_identities'])
            profile['actions'].append(action)
        key=A.cache_key_material(profile);keyroot=Path(request['semantic_key_dir']);require(keyroot.is_dir(),'existing_semantic_key_dir_required')
        keyfile=keyroot/(D.digest(key)+'.json')
        if keyfile.exists():require(json.loads(keyfile.read_text())==key,'semantic_key_collision')
        else:save(keyfile,key)
        profile['cache_key_identity']=A.file_identity(keyfile)
        config=base/'ccache.conf';config.write_text(A.render_config(profile));profile['configuration']=A.file_identity(config)
        path=base/'profile.json';save(path,profile);ref={'path':str(path),'sha256':D.sha(path)}
        for row in actions:result[row['object']]=ref
    return result


def quarantine_old_pch(rows,phase,protection):
    """原pending输出不会先覆盖旧.gch；已保护目标真实移入新run，防创建时隐式加载旧PCH。"""
    saved=[];protected={x['original_path']:x for x in protection['files']}
    for number,row in enumerate(rows):
        path=Path(row['object'])
        require(path.suffix=='.gch','actual_GCC_PCH_output_required')
        if not path.exists():continue
        require(path.is_file() and not path.is_symlink(),'ordinary_old_PCH_required')
        old=A.file_identity(path);proof=protected.get(str(path));require(proof is not None,'protected_old_PCH_required')
        snapshot=Path(proof['snapshot_path']);snapshot_id=A.file_identity(snapshot)
        require(old['sha256']==proof['sha256']==snapshot_id['sha256'] and old['bytes']==snapshot_id['bytes'] and
                (path.stat().st_dev,path.stat().st_ino)!=(snapshot.stat().st_dev,snapshot.stat().st_ino),'original_PCH_protection_changed')
        destination=phase/f'previous-pch-{number:04d}.preserved'
        require(not destination.exists(),'fresh_PCH_quarantine_required')
        save(phase/f'old-pch-{number:04d}-move-intent.json',{'original':old,'destination':str(destination),'protection':proof})
        path.rename(destination)
        with destination.open('rb') as stream:os.fsync(stream.fileno())
        for directory in {path.parent,phase}:
            fd=os.open(directory,os.O_RDONLY|os.O_DIRECTORY)
            try:os.fsync(fd)
            finally:os.close(fd)
        require(A.sha(destination)==old['sha256'] and destination.stat().st_size==old['bytes'],'old_PCH_quarantine_changed')
        saved.append({'original':old,'preserved':A.file_identity(destination)})
    save(phase/'old-pch-quarantine.json',{'kind':'ORIGINAL_PCH_BYTES_PRESERVED_NOT_NEW_ADOPTION','files':saved})


def checkpoint_cache(request,phase,profiles,lock,prior_indexes):
    """每至多100个终态动作原锁内停驻，只对子进程nice19冻结cache，不调用Space连接器。"""
    helper=request['cache_snapshot_helper'];require(D.sha(helper['path'])==helper['sha256'],'cache_snapshot_helper_changed')
    reference=next(iter(profiles.values()));configuration=json.loads(Path(reference['path']).read_text())
    snap={'kind':'QUIESCENT_N1_CCACHE_SNAPSHOT_REQUEST','cache_dir':request['cache_dir'],
          'output_directory':str(phase/'cache-snapshot'),'queue_lock':json.loads(Path(request['policy']['path']).read_text())['lock_path'],
          'scheduler_terminal':{'path':str(phase/'compile/result.json'),'sha256':D.sha(phase/'compile/result.json')},
          'active_calls':{'path':str(phase/'compile/active-calls.json'),'sha256':D.sha(phase/'compile/active-calls.json')},
          'profile':reference,'ccache_identity':configuration['ccache'],'scheduler':request['scheduler'],
          'previous_indexes':prior_indexes[-1:], 'chunk_bytes':64*1024**2}
    path=phase/'cache-snapshot-request.json';save(path,snap)
    command=[request['python']['path'],helper['path'],'--request',str(path),'--sha256',D.sha(path),'--lock-fd',str(lock.fileno())]
    with (phase/'cache-snapshot.log').open('x') as output:
        proc=subprocess.run(command,stdout=output,stderr=subprocess.STDOUT,pass_fds=(lock.fileno(),),timeout=600)
    require(proc.returncode==0,'cache_snapshot_failed_original_log_retained')
    index=phase/'cache-snapshot/snapshot-index.json';reference={'path':str(index),'sha256':D.sha(index)}
    prior_indexes.append(reference)
    print(json.dumps({'kind':'FROZEN_CACHE_CHECKPOINT_FOR_UNIQUE_SPACE_OWNER','index':reference,'uploaded':False}),flush=True)


def execute(request,root,plan,context,states,reader):
    """仅显式--execute调用未改N1；单一原queue锁涵盖PCH前序和后续TU。"""
    scheduler=module('unchanged_original_N1',request['scheduler']['path']);policy=reader.checked(request['policy'])
    require(D.sha(request['scheduler']['path'])==request['scheduler']['sha256'],'scheduler_identity_changed')
    require(request['cache_mode'] in ('strict','guarded_pch'),'cache_mode_required')
    require(Path(request['cache_dir']).is_dir(),'existing_dedicated_cache_required')
    phases=partition_phases(reader.checked(request['queue']));rebuild={r['object'] for r in plan['rebuild']}
    rows=[];failures=[];pch_receipts={};history=compiler_history(reader.checked(request['history']) if request.get('history') else [])
    history+=original_uncached_history(request.get('original_uncached_history'),reader)
    for row in phases['pch']:
        if row['object'] not in rebuild:
            saved=states[row['object']]['result']['launcher_receipt'];reader.checked(saved)
            pch_receipts[pch_header(row)]={'object':row['object'],'receipt':saved}
    with open(policy['lock_path'],'a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        prior_indexes=[]
        for phase_name in ('pch','translation_units'):
            group=[r for r in phases[phase_name] if r['object'] in rebuild]
            if not group:continue
            phase_root=root/phase_name;phase_root.mkdir()
            for batch_index,offset in enumerate(range(0,len(group),100)):
                selected=group[offset:offset+100]
                phase=phase_root/f'batch-{batch_index:04d}';phase.mkdir()
                if phase_name=='pch':quarantine_old_pch(selected,phase,reader.checked(request['overwrite_protection']))
                profiles=make_profiles(request,phase,selected,context,reader,pch_receipts)
                wrapped=[wrap_action(row,profiles[row['object']],request['python']['path'],request['launcher']['path']) for row in selected]
                # PCH未知预算仍交原N1保守独占；不传cache-hit轻量RSS，不改hard policy。
                result=scheduler.compile_queue(wrapped,phase/'compile',request['frozen_inputs']['path'],request['gnu_time']['path'],policy,history=history,lock_handle=lock)
                save(phase/'scheduler-terminal.json',result)
                for raw in result['results']:
                    if raw['exit_code']!=0:
                        failures.append(raw);continue
                    original=next(r for r in selected if r['object']==raw['object'])
                    profile=json.loads(Path(profiles[raw['object']]['path']).read_text());matches=[]
                    for path in Path(profile['evidence_dir']).glob('*.json'):
                        receipt=json.loads(path.read_text())
                        if receipt.get('canonical_command')==original['command'] and receipt.get('output_identity',{}).get('sha256')==raw['object_sha256']:matches.append((path,receipt))
                    require(len(matches)==1,'exact_successful_launcher_receipt_required');path,receipt=matches[0]
                    require(receipt['exit_code']==0 and '-MD' in receipt['ccache_input_compiler_command'],'actual_full_MD_launcher_required')
                    enriched=dict(raw,kind='DERIVED_CANONICAL_ACTION_WITH_ACTUAL_LAUNCHER_PROVENANCE',
                                  scheduler_original_command=raw['original_command'],scheduler_command=raw['command'],
                                  original_command=original['command'],command=receipt['actual_launcher_command'],
                                  ccache_input_compiler_command=receipt['ccache_input_compiler_command'],actual_backend_argv_observed=False,
                                  canonical_command=original['command'],actual_dependency_mode='MD',cache_outcome=receipt['cache_outcome'],compiler_invoked=receipt['compiler_invoked'],launcher_receipt={'path':str(path),'sha256':D.sha(path)})
                    rawfile=phase/(D.digest(original['object'])+'.result.json');save(rawfile,enriched);enriched['raw_result_identity']={'path':str(rawfile),'sha256':D.sha(rawfile)}
                    binding=None
                    if phase_name=='translation_units' and pch_header(original):
                        created=pch_receipts[pch_header(original)];created_receipt=reader.checked(created['receipt'])
                        binding={'header':pch_header(original),'object_identity':D.ident(created['object']),
                                 'creation_receipt':D.ident(created['receipt']['path']),
                                 'creation_dependencies':created_receipt['dependency_identities']}
                    state=D.capture_state(original,raw['dependency_file'],context,enriched,pch_binding=binding)
                    statefile=phase/(D.digest(original['object'])+'.state.json');save(statefile,state);enriched['state_identity']={'path':str(statefile),'sha256':D.sha(statefile)};rows.append(enriched)
                    del state  # cache冻结/下个N1准入不持有上一TU的大namespace。
                    if phase_name=='pch':pch_receipts[pch_header(original)]={'object':raw['object'],'receipt':{'path':str(path),'sha256':D.sha(path)}}
                try:
                    checkpoint_cache(request,phase,profiles,lock,prior_indexes)
                except (RuntimeError,OSError,subprocess.SubprocessError) as error:
                    save(root/'partial-terminal.json',{'kind':'ACTUAL_CACHE_PRESERVATION_STOP_NO_COMPLETE','error':str(error),'results':rows,'scheduler_status':result['status']})
                    return 1
                if not may_continue_batch(result,policy,phase_name):
                    save(root/'partial-terminal.json',{'kind':'ACTUAL_N1_PARTIAL_NO_COMPLETE','phase':phase_name,'status':result['status'],'results':rows,'scheduler_terminal':str(phase/'scheduler-terminal.json')});return 1
        if failures:
            save(root/'partial-terminal.json',{'kind':'ACTUAL_N1_ORDINARY_ERRORS_DRAINED_ALL_BATCHES_NO_COMPLETE','results':rows,'failures':failures,'pending':[]});return 1
        verify_context(context,reader)
        for ref in reader.checked(request['frozen_inputs'])['files']:reader.checked(ref,json_value=False)
        adopted=[]
        for action in plan['reused']:
            state=states[action['object']];verdict=D.classify(action,state,context)
            pair=P.verify_pair(action,state,context,request.get('resolution_proofs',{}).get(action['object']),reader) if verdict['decision']=='NEEDS_RESOLUTION_PROOF' else None
            require(final_decision(verdict,pair)['decision']=='REUSE','post_execution_adoption_drift');adopted.append(state['result'])
            del state  # 最终只汇总原result小行，不连带保留state/context/namespace。
        # PCH辅助动作与产品TU分开计数，不将.gch假作新增产品对象。
        save(root/'compile-results.json',rows+adopted)
        save(root/'compile-complete.json',{'kind':'ACTUAL_ACCELERATED_COMPILE_COMPLETE_NOT_LINK_OR_GATE','at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
             'translation_units':len(phases['translation_units']),'pch_actions':len(phases['pch']),'new_actions':len(rows),'adopted_actions':len(adopted),
             'results':{'path':str(root/'compile-results.json'),'sha256':D.sha(root/'compile-results.json')},'cache_hits_are_not_frontend_compilations':True,
             'cache_outcomes':{key:sum(r.get('cache_outcome')==key for r in rows) for key in ('cache_miss','direct_cache_hit','preprocessed_cache_hit','unknown')},
             'real_PCH_cache_measurement_not_inferred_from_configuration':True})
    return 0


def main():
    """新目录prepare与显式execute各锁原request；没有自动configure、安装或probe。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--request',required=True);parser.add_argument('--sha256',required=True);parser.add_argument('--execute',action='store_true');args=parser.parse_args()
    require(D.sha(args.request)==args.sha256,'new_run_request_changed');request=json.loads(Path(args.request).read_text())
    require(request['kind']=='EXPLICIT_NEW_RUN_ACCELERATED_INCREMENTAL_REQUEST','explicit_new_run_request_required')
    root=Path(request['run_directory']);require(root.is_absolute(),'absolute_new_run_required');exists=root.exists();require(not exists or args.execute,'new_prepare_directory_required')
    if not exists:root.mkdir(exist_ok=False)
    reader=P.EvidenceReader();plan,context,states=prepare(request,root,reader,write=not exists)
    if exists:
        prior=json.loads((root/'request-identity.json').read_text())
        require(prior['sha256']==args.sha256 and json.loads((root/'plan.json').read_text())==plan,'prepared_plan_or_request_changed')
    else:save(root/'request-identity.json',{'path':str(Path(args.request).absolute()),'sha256':args.sha256,'execute_requested':args.execute})
    if args.execute:save(root/'execution-start.json',{'at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'request_sha256':args.sha256,'kind':'ACTUAL_NEW_RUN_EXECUTION_START'})
    return execute(request,root,plan,context,states,reader) if args.execute else 0


if __name__=='__main__':raise SystemExit(main())
