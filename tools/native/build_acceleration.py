"""新运行显式启动器：核冻结身份后才启动ccache；不修改旧scheduler或放宽默认PCH语义。"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import os
import re
import subprocess
import sys
import time

ENVIRONMENT_KEYS = ('PATH', 'CPATH', 'CPLUS_INCLUDE_PATH', 'C_INCLUDE_PATH', 'GCC_EXEC_PREFIX',
                    'COMPILER_PATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH', 'LD_PRELOAD',
                    'SOURCE_DATE_EPOCH', 'LANG', 'LC_ALL', 'LC_CTYPE', 'LC_MESSAGES', 'TZ',
                    'OBJC_INCLUDE_PATH', 'DEPENDENCIES_OUTPUT', 'SUNPRO_DEPENDENCIES', 'LANGUAGE',
                    'GCC_COMPARE_DEBUG', 'GCC_EXTRA_DIAGNOSTIC_OUTPUT', 'GCC_COLORS', 'TMPDIR', 'LD_AUDIT')

POISON_GUARD_BYTES = '// 构建加速准入：时间内建宏不得进入PCH或缓存结果。\n#pragma GCC poison __DATE__ __TIME__ __TIMESTAMP__\n'.encode('utf-8')


def require(ok, reason):
    """安全前提用真实异常，python -O不能消除拒门。"""
    if not ok:
        raise RuntimeError(reason)


def sha(path):
    """流式读取工具/配置/PCH实际字节，不以mtime判等。"""
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def file_identity(path):
    """保逻辑别名和实际目标，防alternatives换代但旧binary仍在。"""
    path = Path(path).absolute()
    return {'path': str(path), 'realpath': str(path.resolve()), 'bytes': path.stat().st_size, 'sha256': sha(path)}


def verify_file(item):
    """逐真实path/realpath/长度/hash核对；缺失与改链均拒。"""
    try:
        require(file_identity(item['path']) == item, 'file_identity_mismatch')
    except (OSError, KeyError) as error:
        raise RuntimeError('file_identity_missing_or_invalid') from error


def environment_identity(environment):
    """只保存技术白名单值的hash/unset，不输出原环境值或秘密。"""
    return {key: (None if key not in environment else hashlib.sha256(environment[key].encode()).hexdigest())
            for key in ENVIRONMENT_KEYS}


def cache_key_material(profile):
    """缓存键只含真实语义身份，不混run时间/日志路径/整队列导致全部永远miss。"""
    require(profile.get('toolchain_files') and profile.get('toolchain_sha256'), 'resolved_toolchain_files_required')
    return copy.deepcopy({'schema_version':1, 'compiler':profile['compiler'], 'ccache':profile['ccache'],
            'environment':profile['environment'], 'toolchain_files':profile['toolchain_files'],
            'toolchain_sha256':profile['toolchain_sha256'], 'mode':profile['mode'], 'base_dir':profile['base_dir'],
            'working_directories':sorted({x['cwd'] for x in profile['actions']}),
            'volatile_guard':profile.get('volatile_guard'), 'hash_dir':False, 'direct_mode':False, 'depend_mode':False,
            'acceleration_semantics':acceleration_semantics(profile['ccache'],profile['base_dir'],profile['mode'])})


def configuration_semantics(base_dir, mode):
    """单一来源生成实际配置的输出相关字段，不混缓存容量或run目录。"""
    require(mode in ('strict','guarded_pch'),'unsupported_cache_mode')
    require(isinstance(base_dir,str) and Path(base_dir).is_absolute() and base_dir==base_dir.strip() and
            not any(c in base_dir for c in '\n\r\0$'),'configuration_value_invalid')
    return {'base_dir':base_dir,'hash_dir':False,'compiler_check':'content','direct_mode':False,
            'depend_mode':False,'hard_link':False,'file_clone':False,
            'sloppiness':'' if mode=='strict' else 'pch_defines,time_macros','remote_storage':''}


def acceleration_semantics(ccache,base_dir,mode):
    """真实cache二进制和launcher/config协议进入.d身份，不能跨路径变换语义采用。"""
    verify_file(ccache)
    modes=['strict'] if mode=='strict' else ['strict','guarded_pch']
    require(mode in ('strict','guarded_pch'),'unsupported_cache_mode')
    return {'protocol':'CCACHE_INPUT_ARGV_BASE_DIR_V1','ccache':copy.deepcopy(ccache),
            'launcher':file_identity(__file__),
            'configuration_by_mode':{name:configuration_semantics(base_dir,name) for name in modes}}


def verify_acceleration_context(context,profile):
    """每份真实config与上下文允许的mode同源；原件路径不同不代表语义变化。"""
    value=context.get('acceleration_semantics',{})
    expected=acceleration_semantics(profile['ccache'],profile['base_dir'],profile['mode'])
    require(all(value.get(k)==expected[k] for k in ('protocol','ccache','launcher')) and
            value.get('configuration_by_mode',{}).get(profile['mode'])==expected['configuration_by_mode'][profile['mode']],
            'acceleration_context_mismatch')


def render_config(profile):
    """生成完全显式严格配置；受控PCH mode必须在执行前另验批准。"""
    values = [profile[key] for key in ('base_dir', 'cache_dir', 'max_size')]
    require(all(isinstance(x, str) and x and x==x.strip() and not any(c in x for c in '\n\r\0$') for x in values), 'configuration_value_invalid')
    require(Path(values[0]).is_absolute() and Path(values[1]).is_absolute(), 'configuration_value_not_absolute')
    require(re.fullmatch(r'[1-9][0-9]*(?:K|M|G|T)?', values[2]) is not None, 'configuration_value_size')
    semantics=configuration_semantics(values[0],profile['mode'])
    lines=[]
    for key,value in semantics.items():
        text=str(value).lower() if isinstance(value,bool) else value
        # 4.14.2 ConfigReader跨行跳空白；comment确保空值不吞后续key。
        lines.append(f'{key} = {text if text else "# explicit empty value"}\n')
    return ''.join(lines)+f'cache_dir = {values[1]}\nmax_size = {values[2]}\n'


def canonical_arguments(command):
    """只移除已声明输出/依赖副作用参数，其余原flags逐token保留。"""
    result = []; i = 0
    while i < len(command):
        flag = command[i]
        if flag in ('-o', '-MF', '-MT', '-MQ'):
            require(i + 1 < len(command), 'malformed_output_argument'); i += 2
        elif flag in ('-MD', '-MMD'):
            i += 1
        else:
            result.append(flag); i += 1
    return result


def pch_context(command, cwd, compiler, environment):
    """从真实argv推导同宏/flags上下文，只去CMake已知的单输入和header语言差异。"""
    args = canonical_arguments(command)
    require(args.count('-c') == 1 and args.index('-c') + 1 < len(args), 'pch_single_source_required')
    index = args.index('-c'); del args[index:index + 2]
    if '-x' in args:
        index = args.index('-x')
        require(index + 1 < len(args) and args[index + 1] in ('c++', 'c++-header'), 'pch_language_unsupported')
        del args[index:index + 2]
    require(not any(x.startswith(('-fplugin', '-fmodules', '-specs', '--specs')) for x in args), 'pch_external_semantics_unsupported')
    return {'argv': args, 'cwd': cwd, 'compiler': compiler, 'environment': environment}


def verify_pch_use(profile, action, command, cwd):
    """使用端必须借实际创建终态与相同真实宏环境，不能自填两串相等hash当证据。"""
    creation = action.get('pch_creation_receipt')
    require(isinstance(creation, dict), 'pch_actual_creation_receipt_required')
    verify_file(creation)
    receipt = json.loads(Path(creation['path']).read_text())
    require(receipt.get('kind') == 'ACTUAL_NEW_RUN_CCACHE_ACTION' and receipt.get('exit_code') == 0 and
            receipt.get('action_kind') == 'pch_create' and receipt.get('volatile_guard_passed') is True, 'pch_successful_creation_required')
    expected = pch_context(command, cwd, profile['compiler'], profile['environment'])
    require(receipt.get('pch_context') == expected, 'pch_same_flags_macros_context_required')
    require(action.get('pch_inputs'), 'pch_actual_bytes_and_dependencies_required')
    for item in action['pch_inputs']:
        verify_file(item)
    outputs = [item for item in action['pch_inputs'] if item['path'].endswith('.gch')]
    require(len(outputs) == 1 and receipt.get('output_identity', {}).get('sha256') == outputs[0]['sha256'] and
            receipt.get('output_identity', {}).get('bytes') == outputs[0]['bytes'], 'pch_creation_output_mismatch')
    require(receipt.get('dependency_identities') == action.get('pch_creation_inputs'), 'pch_creation_dependencies_required')
    for item in action.get('pch_creation_inputs', []):
        verify_file(item)


def output_path(command):
    """仅支持原N1单输出动作，不解释shell或响应文件。"""
    require(command.count('-o') == 1 and command.index('-o') + 1 < len(command), 'one_output_required')
    return command[command.index('-o') + 1]


def validate_poison_command(command, guard):
    """GCC硬poison必须是首强制头；拒所有可能在poison之前定义别名的复杂宏通道。"""
    verify_file(guard)
    require(Path(guard['path']).read_bytes()==POISON_GUARD_BYTES,'exact_poison_guard_required')
    require('-Werror=date-time' in command and '-Wsystem-headers' in command and '-Werror=invalid-pch' in command,
            'volatile_system_header_diagnostic_guard_required')
    require(not any(x.startswith(('-Wp,','-Xpreprocessor','-fpreprocessed','@','-imacros','-include-pch',
                                 '-fplugin','-specs','--specs')) for x in command),
            'pre_poison_external_preprocessor_unsupported')
    includes=[];i=0
    while i<len(command):
        flag=command[i]
        if flag=='-include':
            require(i+1<len(command),'missing_force_include');includes.append(command[i+1]);i+=2;continue
        require(not flag.startswith('-include'),'joined_force_include_unproved')
        macro=None
        if flag in ('-D','-U'):
            require(i+1<len(command),'missing_macro_argument');macro=command[i+1];i+=1
        elif flag.startswith(('-D','-U')):macro=flag[2:]
        if macro is not None:
            # GCC poison明确豁免此前已定义宏；只准最窄对象宏和ASCII字面值，不能拼接/函数/转义。
            name,sep,value=macro.partition('=')
            require(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*',name) is not None and
                    not re.search(r'__(?:DATE|TIME|TIMESTAMP)__|_Pragma',macro) and
                    not any(c in macro for c in '#\\\n\r') and
                    (not sep or re.fullmatch(r'[A-Za-z0-9_+./: -]*|"[A-Za-z0-9_+./: -]*"',value) is not None),
                    'pre_poison_macro_exemption_unproved')
        i+=1
    require(includes and includes[0]==guard['path'] and includes.count(guard['path'])==1,
            'poison_guard_must_be_first_force_include')


def validate_volatile_guard(command, paths, guard):
    """成功编译/缓存证据必须含原硬poison头；普通_Pragma不能撤销poison，不再假词法absence。"""
    validate_poison_command(command,guard)
    require(guard['path'] in [str(x) for x in paths],'actual_MD_poison_guard_required')


def verify_guarded_policy(profile):
    """共用开关独立锁获准语义，不把用户要优化当作允许任意sloppiness。"""
    approval = profile.get('guarded_pch_approval')
    require(isinstance(approval, dict), 'guarded_pch_approval_required')
    verify_file(approval)
    policy = json.loads(Path(approval['path']).read_text())
    require(policy.get('kind') == 'APPROVED_CONTROLLED_GCC_PCH_CCACHE_POLICY' and policy.get('approved') is True,
            'guarded_pch_approval_required')
    require(policy.get('sloppiness') == ['pch_defines', 'time_macros'] and
            policy.get('volatile_macro_error_flag') == '-Werror=date-time' and
            policy.get('system_header_warning_flag') == '-Wsystem-headers' and
            policy.get('volatile_guard') == profile.get('volatile_guard') and
            policy.get('poison_preexisting_macro_policy') == 'first_force_include_simple_object_defines_only' and
            policy.get('direct_mode') is False and policy.get('depend_mode') is False and
            policy.get('compiler') == profile['compiler'] and policy.get('base_dir') == profile['base_dir'],
            'guarded_pch_approval_scope_mismatch')
    require(policy.get('actual_source_reference') and policy.get('authorization_reference'), 'guarded_pch_approval_provenance_required')


def prepare_invocation(profile, command, cwd, environment):
    """核精确新动作、工具与环境；返回子进程计划，不执行外部程序。"""
    require(profile.get('schema_version') == 1, 'profile_schema_required')
    if profile.get('mode') == 'guarded_pch':
        verify_guarded_policy(profile)
        validate_poison_command(command,profile['volatile_guard'])
        require('-Werror=date-time' in command and not any(x in command for x in ('-Wno-date-time','-Wno-error=date-time','-w')), 'guarded_all_actions_time_macro_error_required')
    require(environment_identity(environment) == profile['environment'], 'technical_environment_changed')
    require(profile.get('technical_inputs'), 'technical_identity_inputs_required')
    for item in [profile['ccache'], profile['compiler'], profile['configuration'], profile['cache_key_identity'], *profile['technical_inputs'], *profile['toolchain_files']]:
        verify_file(item)
    require(json.loads(Path(profile['cache_key_identity']['path']).read_text()) == cache_key_material(profile), 'cache_key_not_exact_semantics')
    verify_file(profile['execution_context_identity'])
    context=json.loads(Path(profile['execution_context_identity']['path']).read_text())
    require(context['toolchain_sha256']==profile['toolchain_sha256'] and context['environment']==profile['environment'], 'cache_runtime_or_environment_key_not_current_context')
    verify_acceleration_context(context,profile)
    require(os.access(profile['ccache']['path'], os.X_OK) and os.access(profile['compiler']['path'], os.X_OK), 'executable_tool_required')
    require(Path(profile['configuration']['path']).read_text() == render_config(profile), 'configuration_not_exact')
    require(command and command[0] == profile['compiler']['path'] and not any(x.startswith('@') for x in command), 'compiler_alias_or_response_file_unsupported')
    matches = [a for a in profile['actions'] if a['cwd'] == cwd and canonical_arguments(a['command']) == canonical_arguments(command)]
    require(len(matches) == 1, 'exact_action_not_unique_or_missing')
    action = matches[0]; out = output_path(command); expected = action['object']
    require(action.get('input_files') and any(x['path'] == action['source'] for x in action['input_files']), 'action_source_identity_required')
    for item in action['input_files']:
        verify_file(item)
    require(out == expected or (out.startswith(expected + '.') and out.endswith('.pending.o') and Path(out).parent == Path(expected).parent), 'output_not_original_or_N1_pending')
    require(command.count('-c') == 1 and command[command.index('-c') + 1] == action['source'], 'original_source_required')
    pch = action['kind'] == 'pch_create' or bool(action.get('pch_inputs')) or any('cmake_pch' in x for x in command)
    if pch:
        require(profile['mode'] == 'guarded_pch', 'pch_requires_explicit_guarded_policy')
        require('-Werror=date-time' in command and not any(x in command for x in ('-Wno-date-time', '-Wno-error=date-time', '-w')), 'pch_volatile_macro_error_guard_required')
        pch_context(command, cwd, profile['compiler'], profile['environment'])
        if action['kind'] != 'pch_create':
            verify_pch_use(profile, action, command, cwd)
    compiler = ['-MD' if x == '-MMD' else x for x in command]
    require('-MD' in compiler, 'complete_system_MD_dependency_required')
    child = {k: v for k, v in environment.items() if not k.startswith('CCACHE_')}
    child['CCACHE_CONFIGPATH'] = profile['configuration']['path']
    child['CCACHE_DIR'] = profile['cache_dir']
    child['CCACHE_TEMPDIR'] = str(Path(profile['cache_dir']) / 'tmp')
    # 只散列稳定语义键和PCH真实bytes；profile/run路径只作执行锁，不能把新run变成必miss。
    extras = [profile['cache_key_identity']['path']] + [x['path'] for x in action.get('pch_inputs', [])]
    require(all(':' not in x for x in extras), 'extra_file_separator_unsupported')
    child['CCACHE_EXTRAFILES'] = ':'.join(extras)
    return {'command': [profile['ccache']['path'], *compiler], 'compiler_command': compiler,
            'environment': child, 'action': action, 'pch': pch, 'output': out}


def cache_outcome(text):
    """只认独立statslog的明确计数名；未知/混合绝不冒hit或miss。"""
    lines = [x.strip() for x in text.splitlines() if x.strip() and not x.startswith('#')]
    outcomes = [x for x in lines if x in ('direct_cache_hit', 'preprocessed_cache_hit', 'cache_miss')]
    return outcomes[0] if len(outcomes) == 1 else 'unknown'


def compiler_peak_eligible(receipt):
    """唯真miss且成功才可提报编译峰值；hit/未知的轻RSS排除。"""
    return receipt.get('cache_outcome') == 'cache_miss' and receipt.get('exit_code') == 0 and receipt.get('compiler_invoked') is True


def main():
    """仅显式新run调用；沿原N1进程组/limits，不另开session或改变guard。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', required=True); parser.add_argument('--sha256', required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER); args = parser.parse_args()
    require(sha(args.profile) == args.sha256, 'profile_hash_mismatch')
    profile = json.loads(Path(args.profile).read_text())
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    invocation = prepare_invocation(profile, command, os.getcwd(), dict(os.environ))
    root = Path(profile['evidence_dir']); require(root.is_dir() and root.is_absolute(), 'existing_evidence_directory_required')
    key = hashlib.sha256((os.getcwd() + '\0' + '\0'.join(command)).encode()).hexdigest()
    stats = root / (key + '.stats'); result = root / (key + '.json')
    require(not stats.exists() and not result.exists(), 'no_unknown_call_replay')
    invocation['environment']['CCACHE_STATSLOG'] = str(stats)
    started = time.monotonic()
    proc = subprocess.run(invocation['command'], env=invocation['environment'])
    require(sha(args.profile) == args.sha256, 'profile_changed_during_action')
    for item in [profile['ccache'], profile['configuration'], profile['compiler'], profile['cache_key_identity'], *profile['technical_inputs'], *profile['toolchain_files'], *invocation['action']['input_files'], *invocation['action'].get('pch_inputs', [])]:
        verify_file(item)
    outcome = cache_outcome(stats.read_text()) if stats.exists() else 'unknown'
    dependency_identities = []
    if proc.returncode == 0:
        import importlib.util
        spec=importlib.util.spec_from_file_location('actual_full_MD',Path(__file__).with_name('depfile_incremental.py'))
        deps=importlib.util.module_from_spec(spec);spec.loader.exec_module(deps)
        require(command.count('-MF') == 1, 'actual_MF_output_required')
        dep_path=command[command.index('-MF')+1]
        paths=deps.parse_depfile(dep_path,os.getcwd())
        require(invocation['action']['source'] in paths, 'actual_MD_source_required')
        dependency_identities=[file_identity(p) for p in paths]
        if profile['mode']=='guarded_pch':validate_volatile_guard(command,paths,profile['volatile_guard'])
    receipt = {'kind': 'ACTUAL_NEW_RUN_CCACHE_ACTION', 'profile_sha256': args.sha256,
               'canonical_command': invocation['action']['command'], 'ccache_input_compiler_command': invocation['compiler_command'],
               'actual_backend_argv_observed':False,
               'actual_launcher_command': invocation['command'], 'cwd': os.getcwd(), 'exit_code': proc.returncode,
               'elapsed_seconds': time.monotonic() - started, 'cache_outcome': outcome,
               'compiler_invoked': outcome == 'cache_miss', 'pch': invocation['pch'],
               'action_kind': invocation['action']['kind'],
               'volatile_guard_passed': proc.returncode == 0 and profile['mode']=='guarded_pch',
               'volatile_guard':profile.get('volatile_guard'),
               'input_identities': invocation['action']['input_files'],
               'dependency_identities': dependency_identities,
               'output_identity': file_identity(invocation['output']) if proc.returncode == 0 else None,
               'pch_context': pch_context(command, os.getcwd(), profile['compiler'], profile['environment']) if invocation['pch'] else None,
               'stats_identity': file_identity(stats) if stats.exists() else None}
    receipt['eligible_for_compiler_peak_history'] = compiler_peak_eligible(receipt)
    with result.open('x') as stream:
        json.dump(receipt, stream, indent=2); stream.write('\n')
    return proc.returncode


if __name__ == '__main__':
    raise SystemExit(main())
