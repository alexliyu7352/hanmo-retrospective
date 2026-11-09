"""消费既有逐对象raw-E/full-MD终态格式；只读已冻结证据，不建立新探针服务。"""
from pathlib import Path
import importlib.util
import json
import sys
sys.dont_write_bytecode=True
_spec=importlib.util.spec_from_file_location('actual_depfile_contract',Path(__file__).with_name('depfile_incremental.py'))
_d=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_d)
_aspec=importlib.util.spec_from_file_location('actual_poison_contract',Path(__file__).with_name('build_acceleration.py'))
_a=importlib.util.module_from_spec(_aspec);_aspec.loader.exec_module(_a)
require=_d.require


def probe_argv(row, dep):
    """逐字沿已审run_dependency_probe_v2变换，独立MD，无宏/路径归一。"""
    command=list(row['original_command'])
    require(command.count('-c')==command.count('-o')==1,'unsupported_compile_shape')
    require(command[command.index('-c')+1]==row['source'] and command[command.index('-o')+1]==row['object'],'original_source_or_output_shape')
    forbidden={'-MD','-MMD','-MF','-MT','-MQ','-M','-MM','-MG','-MP','-E','-P','-S','-save-temps','-frecord-gcc-switches'}
    require(not any(x in forbidden or x.startswith(('@','-Wp,','-fplugin','-fmodule','-fprofile','-specs=','--specs=','-include-pch','-save-temps=')) for x in command),'unsupported_external_compile_semantics')
    command[command.index('-c')]='-E';i=command.index('-o');del command[i:i+2]
    return command+['-MD','-MF',str(dep),'-MT',row['object']]


class EvidenceReader:
    """同stat仅复用已哈希不可变控制，任何原件被替换重新核；不读取私有环境值。"""
    def __init__(self):
        """缓存范围仅当前一次计划，不跨run沿用旧mtime判断。"""
        self.cache={}

    def checked(self, ref, *, json_value=True, retain_json=True):
        """支持原历史path/bytes/SHA结构，不能使用未在manifest登记的猜测路径。"""
        require(isinstance(ref,dict) and set(('path','sha256'))<=set(ref),'evidence_reference_required')
        path=Path(ref['path']);require(path.is_absolute(),'absolute_evidence_reference_required')
        st=path.stat();signature=(st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns)
        key=(str(path),ref['sha256'])
        if key not in self.cache or self.cache[key][0]!=signature:
            require(_d.sha(path)==ref['sha256'] and ('bytes' not in ref or st.st_size==ref['bytes']),'evidence_identity_changed:'+str(path))
            self.cache[key]=(signature,None)
        if not json_value:return ref
        if not retain_json:
            # 大state只留小身份票；不得因先前普通读取而继续持有旧JSON。
            self.cache[key]=(signature,None)
            return json.loads(path.read_text())
        if self.cache[key][1] is None:self.cache[key]=(signature,json.loads(path.read_text()))
        return self.cache[key][1]


def compare_sides(old,new):
    """原stdout的byte/line/hash及整个MD内容闭包全等，缺字段或任一差异拒绝。"""
    for key in ('raw_preprocessor_bytes','raw_preprocessor_lines','raw_preprocessor_sha256','dependency_count','dependency_identity_sha256'):
        require(old[key]==new[key],'paired_'+key+'_differs')



def verify_exact_poison_declaration(proof,row,state,by_path,deps,refs,reader):
    """只认原成功执行的固定三宏poison声明；原词法命中保留，绝不改raw输出。"""
    require(proof.get('kind')=='CAPTURED_EXECUTION_RAW_E_MD_POISON_AWARE' and state,
            'exact_actual_poison_proof_kind_required')
    evidence=proof.get('poison_declaration_evidence')
    require(isinstance(evidence,dict) and evidence.get('kind')=='EXACT_ORIGINAL_EXECUTION_FORCED_POISON_DECLARATION_ONLY',
            'exact_poison_declaration_evidence_required')
    guard=evidence['guard'];receipt_ref=evidence['launcher_receipt']
    require(receipt_ref==row['launcher_receipt']==state['result']['launcher_receipt'] and
            refs.get(receipt_ref['path'])==receipt_ref and refs.get(guard['path'])==guard,
            'poison_original_receipt_and_guard_not_locked')
    receipt=reader.checked(receipt_ref,retain_json=False)
    require(receipt.get('kind')=='ACTUAL_NEW_RUN_CCACHE_ACTION' and receipt.get('exit_code')==0 and
            receipt.get('volatile_guard_passed') is True and receipt.get('volatile_guard')==guard and
            receipt['canonical_command']==row['original_command'] and
            receipt['output_identity']['sha256']==row['object_sha256'],
            'poison_not_original_successful_launcher')
    _a.validate_poison_command(row['original_command'],guard)
    require(guard in receipt['dependency_identities'] and guard['path'] in deps,
            'poison_not_in_actual_compiler_and_probe_MD')
    observed=by_path[guard['path']]
    require(all(observed[k]==guard[k] for k in ('path','bytes','sha256')) and
            observed.get('volatile_lexical_hits')==['__DATE__','__TIME__','__TIMESTAMP__'],
            'original_poison_lexical_observation_required')
    for path in deps:
        hits=by_path[path].get('volatile_lexical_hits')
        require(type(hits) is list and all(type(x) is str and x in ('__DATE__','__TIME__','__TIMESTAMP__') for x in hits) and
                len(hits)==len(set(hits)), 'explicit_original_volatile_lexical_list_required')
    lexical=sorted(p for p in deps if by_path[p]['volatile_lexical_hits'])
    require(lexical==[guard['path']] and proof.get('original_volatile_lexical_inputs')==lexical and
            proof.get('volatile_inputs')==[], 'other_volatile_input_not_poison_declaration')
    original_ref=proof['original_side_result']
    require(refs.get(original_ref['path'])==original_ref,'original_unmodified_probe_result_not_locked')
    original=reader.checked(original_ref,retain_json=False)
    require(original.get('decision')=='RECOMPILE_UNPROVED' and original.get('exit_code')==0 and
            original.get('volatile_inputs')==lexical and original.get('changed_or_added_dependencies')==[] and
            original.get('external_semantic_inputs')==[] and original.get('external_preprocessed_asm') is False and
            original.get('normalization') is None and not any(original.get(k) for k in ('proof_error','precheck_error','resource_stop')) and
            type(original.get('maximum_resident_set_kib')) is int and original['maximum_resident_set_kib']>0,
            'original_probe_not_only_exact_poison_lexical_rejection')
    expected=dict(original,kind='CAPTURED_EXECUTION_RAW_E_MD_POISON_AWARE',
                  decision='SIDE_PROVED_NOT_YET_ADOPTABLE',volatile_inputs=[],
                  original_volatile_lexical_inputs=lexical,poison_declaration_evidence=evidence,
                  original_side_result=original_ref)
    require(proof==expected,'derived_poison_proof_changed_other_original_fields')



def verify_pair(action,state,context,bundle,reader=None):
    """返回可审逐TU结论；解析/失证一律false，由新planner重编，不假旧史。"""
    reader=reader or EvidenceReader()
    try:
        require(isinstance(bundle,dict),'missing_resolution_pair')
        require(bundle.get('kind')=='EXISTING_STAGED_RAW_E_MD_PROOF_BUNDLE','unsupported_pair_bundle')
        manifest=reader.checked(bundle['new_manifest']);plan=reader.checked(bundle['plan']);lock=reader.checked(bundle['lock'])
        refs={x['path']:x for x in lock['files']};require(len(refs)==len(lock['files']),'duplicate_proof_lock_path')
        for ref in refs.values():reader.checked(ref,json_value=False)
        require(refs.get(bundle['new_manifest']['path'])==bundle['new_manifest'],'manifest_not_in_original_proof_lock')
        require(manifest['new_source']==context['source_commit'] and manifest['new_frozen']['sha256']==context['frozen_manifest_sha256'],'pair_not_for_current_source_freeze')
        require(bundle['execution_context_sha256']==_d.digest(_d.semantic_context(context)),'pair_resolution_context_changed')
        frozen=reader.checked(manifest['new_frozen']);reader.checked(manifest['technical_manifest'])
        for frozen_file in frozen['files']:
            reader.checked(frozen_file,json_value=False)
        require(context['technical_manifest_sha256']==manifest['technical_manifest']['sha256'],'pair_technical_identity_changed')
        items=[x for x in plan['candidate_evidence'] if x['object']==action['object'] and x['source']==action['source']]
        require(len(items)==1 and items[0]['decision']=='ADOPT_UNCHANGED_OLD_ROW' and not items[0]['reasons'],'pair_has_no_original_adoption_decision')
        item=items[0];rows=reader.checked(manifest['old_results']);row=rows[item['old_result_index']]
        require(row['source']==action['source'] and row['object']==action['object'] and row['cwd']==action['cwd'] and row['original_command']==action['command'],'pair_actual_action_changed')
        require(row['exit_code']==0 and row.get('atomic_cache_update') is True and _d.sha(row['object'])==row['object_sha256'],'pair_original_object_not_successful_current_bytes')
        old_dep=Path(row['dependency_file']) if row.get('dependency_file') else Path(row['command'][row['command'].index('-MF')+1])
        require(_d.sha(old_dep)==row['dependency_sha256'],'pair_original_MF_changed')
        require(_d.sha(row['source'])==row['source_sha256'],'pair_original_source_changed')
        # 旧40经验前提只在该具名授权轮次显式使用；不可将样本当所有TU的普遍证明。
        admission=bundle['historical_admission']
        if admission=='user_current_round_40':
            require(context.get('empirical_authorization_reference') and context['empirical_authorization_reference']==bundle['empirical_authorization_reference'],'empirical_round_authority_required')
            sample=reader.checked(manifest['sample_terminal'])
            require(plan['sample_terminal_sha256']==manifest['sample_terminal']['sha256'],'original_plan_sample_scope_changed')
            require(sample['kind']=='USER_ACCEPTED_CURRENT_ROUND_SAMPLE_TERMINAL' and sample['all40_equal'] is True and sample['equal_count']==40 and sample['actual_attempts']==40 and not sample['error'],'actual_40_sample_gate_failed')
        else:
            require(admission=='captured_actual_execution' and state and not state.get('synthetic'),'actual_historical_execution_state_required')
            require(state['result'].get('raw_result_identity') and state['context'].get('kind')=='CURRENT_EXECUTION_RESOLUTION_IDENTITY','actual_historical_context_required')
            reader.checked(state['result']['raw_result_identity'],json_value=False)
            require(state['object_identity']['sha256']==row['object_sha256'] and state['depfile_identity']['sha256']==row['dependency_sha256'],'pair_wrong_historical_generation')
        sides={}
        for phase in ('old','new'):
            terminal_ref=bundle[phase+'_terminal'];require(refs.get(terminal_ref['path'])==terminal_ref,'terminal_not_in_proof_lock');terminal=reader.checked(terminal_ref)
            actual_ref=bundle[phase+'_actual_session'];require(refs.get(actual_ref['path'])==actual_ref,'actual_session_not_in_proof_lock');actual=reader.checked(actual_ref)
            require(actual['exit_code']==0 and actual['phase']==phase and actual['terminal']==terminal_ref,'actual_side_tool_terminal_required')
            expected_manifest=bundle['new_manifest'] if phase=='new' else manifest['old_phase_manifest']
            require(terminal['manifest_sha256']==expected_manifest['sha256'],'terminal_wrong_phase_manifest')
            require(terminal['phase']==phase and terminal['complete'] is True and terminal['completed']==terminal['total'] and not terminal['error'],'incomplete_side_terminal')
            proofref=item[phase+'_proof'];require(refs.get(proofref['path'])==proofref,'side_outside_original_lock')
            proof=reader.checked(proofref)
            hits=[x for x in terminal['results'] if x['result']==proofref];require(len(hits)==1,'side_not_in_actual_terminal')
            require(proof['phase']==phase and proof['old_result_index']==item['old_result_index'] and proof['source']==row['source'] and proof['object']==row['object'],'side_wrong_object')
            require(proof['original_object_sha256']==row['object_sha256'] and proof['original_MF_sha256']==row['dependency_sha256'] and proof['original_row_frozen_sha256']==row['frozen_manifest_sha256'],'side_wrong_historical_inputs')
            require(proof['decision']=='SIDE_PROVED_NOT_YET_ADOPTABLE' and proof['exit_code']==0 and proof['raw_preprocessor_bytes']>0 and proof['raw_preprocessor_lines']>0 and proof['dependency_count']>0,'side_unproved')
            require(proof.get('normalization') is None and not proof['volatile_inputs'] and not proof['external_semantic_inputs'] and not proof['external_preprocessed_asm'],'side_normalized_or_external_semantics')
            require(proof['original_command']==row['original_command'] and proof['cwd']==row['cwd'] and proof['actual_command']==probe_argv(row,proof['actual_MD']['path']),'side_argv_changed')
            for key in ('actual_MD','stderr','gnu_time'):
                require(refs.get(proof[key]['path'])==proof[key],'raw_process_evidence_not_locked');reader.checked(proof[key],json_value=False)
            observed_ref=bundle[phase+'_observed_inputs'];require(refs.get(observed_ref['path'])==observed_ref,'observation_not_in_original_lock')
            observed=reader.checked(observed_ref)['files'];by_path={x['path']:x for x in observed};require(len(by_path)==len(observed),'duplicate_observed_dependency')
            deps=_d.parse_depfile(proof['actual_MD']['path'],row['cwd']);require(row['source'] in deps and len(deps)==proof['dependency_count'],'raw_MD_closure_differs')
            if proof.get('kind')=='CAPTURED_EXECUTION_RAW_E_MD_POISON_AWARE':
                require(admission=='captured_actual_execution','poison_exception_not_empirical_or_generic')
                verify_exact_poison_declaration(proof,row,state,by_path,deps,refs,reader)
            else:
                require(not proof.get('poison_declaration_evidence') and not proof.get('original_volatile_lexical_inputs'),
                        'poison_evidence_requires_explicit_actual_schema')
            identities=[{k:by_path[p][k] for k in ('path','bytes','sha256')} for p in deps]
            require(_d.digest(identities)==proof['dependency_identity_sha256'],'raw_MD_observed_identity_differs')
            if phase=='new':
                for value in identities:
                    require(_d.sha(value['path'])==value['sha256'] and Path(value['path']).stat().st_size==value['bytes'],'new_observed_dependency_drift')
            sides[phase]=proof
        compare_sides(sides['old'],sides['new'])
        return {'accepted':True,'reasons':[], 'kind':'EXACT_CURRENT_SOURCE_PAIR_NOT_UNIVERSAL_PROOF','original_row':row,'original_row_index':item['old_result_index'],'proof_lock':bundle['lock']}
    except (RuntimeError,OSError,ValueError,KeyError,TypeError,IndexError) as error:
        return {'accepted':False,'reasons':[str(error)]}
