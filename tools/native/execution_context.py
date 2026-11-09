"""只读派生新运行技术上下文：借原系统身份核验及真实CMake toolchains，不执行编译/查询。"""
from pathlib import Path
import argparse
import importlib.util
import json
import os
import re
import sys
sys.dont_write_bytecode=True


def load_module(name,path):
    """只导入具名同目录实现或固定SHA的既有只读verifier。"""
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m


A=load_module('context_acceleration',Path(__file__).with_name('build_acceleration.py'))
D=load_module('context_depfiles',Path(__file__).with_name('depfile_incremental.py'))
P=load_module('context_evidence',Path(__file__).with_name('paired_resolution.py'))
require=D.require
ORIGINAL_VERIFIER_SHA256='ce355466d728cab5798c4615a0ccb818cecff3dae1fcd7030f8f67e7034d050f'


def implicit_roots(reply,drivers):
    """读取实际FileAPI 1.x GNU工具链；不凭当前.d推测未命中的隐式前根。"""
    require(reply.get('kind')=='toolchains' and reply.get('version',{}).get('major')==1,'actual_toolchains_v1_required')
    seen={};roots=[]
    for value in reply['toolchains']:
        language=value['language']
        if language not in ('C','CXX'):continue
        require(language not in seen,'duplicate_language_toolchain')
        compiler=value['compiler'];require(compiler.get('id')=='GNU' and compiler.get('path') in drivers,'actual_GNU_driver_required')
        include=compiler.get('implicit',{}).get('includeDirectories')
        require(isinstance(include,list) and include and all(Path(p).is_absolute() for p in include),'actual_implicit_include_roots_required')
        seen[language]=compiler['path'];roots+=include
    require(set(seen.values())==set(drivers),'missing_actual_driver_toolchain')
    return list(dict.fromkeys(roots))


def absent_search_roots(configure_log):
    """真实GCC configure记录中的不存在搜索根也锁入namespace，不能只保已存在根。"""
    lines=[line for line in configure_log.splitlines() if 'ignoring nonexistent directory' in line]
    require(lines or ('#include <...> search starts here:' in configure_log and 'End of search list.' in configure_log),
            'actual_GNU_ignored_search_directory_evidence_required')
    roots=[]
    for line in lines:
        match=re.search(r'ignoring nonexistent directory "([^"\n]+)"',line)
        require(match and Path(match[1]).is_absolute() and '\\' not in match[1],'unparsed_GNU_missing_search_directory')
        roots.append(os.path.abspath(match[1]))
    return list(dict.fromkeys(roots))


def derive(request,reader=None):
    """所有上下文字段由锁定原件派生，不能只自填complete-system-headers布尔值。"""
    reader=reader or P.EvidenceReader()
    require(request.get('kind')=='DERIVE_CURRENT_BUILD_EXECUTION_CONTEXT','explicit_context_derivation_required')
    lock=reader.checked(request['technical_lock']);manifest=reader.checked(lock['technical_manifest'])
    require(lock['verifier_sha256']==ORIGINAL_VERIFIER_SHA256 and D.sha(lock['verifier_path'])==ORIGINAL_VERIFIER_SHA256,'original_identity_verifier_required')
    actual=reader.checked(request['technical_reverification'])
    require(actual.get('kind')=='CURRENT_EXECUTION_IDENTITY_REVERIFIED' and actual.get('driver_queries_rechecked') is True and
            actual.get('actual_child_environment_matched') is True and actual['execution_lock_sha256']==request['technical_lock']['sha256'] and
            actual['source_commit']==lock['source_commit'],'actual_current_driver_reverification_required')
    # 此调用不运行GCC；原helper重新核当前HEAD、环境、系统bytes、别名和原目录成员。
    verifier=load_module('original_current_identity_verifier',lock['verifier_path'])
    verifier.verify(request['technical_lock']['path'],request['technical_lock']['sha256'],probe_subprograms=False)
    configuration=reader.checked(request['configure_terminal'])
    require(configuration['exit_code']==0 and configuration['source_commit']==lock['source_commit'],'actual_current_configuration_required')
    index=reader.checked(request['cmake_reply_index']);toolchains=reader.checked(request['toolchains_reply'])
    A.verify_file(request['cmake'])
    require(index['cmake']['paths']['cmake']==request['cmake']['path'],'actual_CMake_binary_required')
    build=Path(request['cmake_reply_index']['path']).parents[4]
    require(Path(request['configure_log']['path'])==build/'CMakeFiles/CMakeConfigureLog.yaml','actual_current_CMake_configure_log_required')
    references=[x for x in index['objects'] if x.get('kind')=='toolchains' and x.get('version',{}).get('major')==1]
    require(len(references)==1 and (Path(request['cmake_reply_index']['path']).parent/references[0]['jsonFile']).absolute()==Path(request['toolchains_reply']['path']),'toolchains_not_in_actual_CMake_reply')
    files=[A.file_identity(x['path']) for x in manifest['unique_current_files']]
    aliases=[A.file_identity(x['requested_path']) for x in manifest['aliases']]
    programs=aliases[:]
    for query in manifest['driver_queries']:
        reader.checked(query['stdout'],json_value=False);reader.checked(query['stderr'],json_value=False)
        selection=query.get('resolved_selection',{})
        if 'requested_path' in selection:programs.append(A.file_identity(selection['requested_path']))
    programs=list({x['path']:x for x in programs}.values())
    reader.checked(request['configure_log'],json_value=False)
    roots=implicit_roots(toolchains,{x['path']:x for x in aliases})
    roots=list(dict.fromkeys(roots+absent_search_roots(Path(request['configure_log']['path']).read_text())))
    # CMake的隐式目录来自实际configure；若当前有额外环境搜索项，也必须覆盖其原逻辑目录。
    for name in ('CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH','OBJC_INCLUDE_PATH'):
        value=os.environ.get(name)
        require(value in (None,''),'nonempty_environment_search_requires_private_scoped_context_not_public_default')
    frozen=reader.checked(request['frozen_inputs'])
    require(frozen.get('files'),'current_frozen_files_required')
    context={'kind':'CURRENT_EXECUTION_RESOLUTION_IDENTITY','complete_system_headers':True,
             'source_root':lock['source_root'],'source_commit':lock['source_commit'],
             'frozen_manifest_sha256':request['frozen_inputs']['sha256'],
             'technical_manifest_sha256':lock['technical_manifest']['sha256'],
             'environment':A.environment_identity(dict(os.environ,LC_ALL='C')),
             'files':files,'resolved_subprograms':programs,'implicit_search_roots':roots,
             'search_evidence':request['toolchains_reply'],
             'search_evidence_sha256':D.digest({'toolchains':request['toolchains_reply']['sha256'],'missing_roots':roots}),
             'toolchain_sha256':D.digest({'files':files,'programs':programs}),
             'acceleration_semantics':A.acceleration_semantics(request['ccache'],lock['source_root'],request['cache_mode']),
             'semantic_input_policy':'ordinary_cpp_no_external_inputs'}
    return context


def main():
    """仅生成当前上下文新文件；无configure、compiler、缓存执行或私有环境值读取。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--request',required=True);parser.add_argument('--sha256',required=True);parser.add_argument('--output',required=True);args=parser.parse_args()
    require(D.sha(args.request)==args.sha256,'context_request_drift')
    request=json.loads(Path(args.request).read_text());context=derive(request)
    context['derivation_request']={'path':str(Path(args.request).absolute()),'sha256':args.sha256}
    with Path(args.output).open('x') as stream:json.dump(context,stream,ensure_ascii=False,indent=2);stream.write('\n')
    print(json.dumps({'kind':context['kind'],'path':str(Path(args.output).absolute()),'sha256':D.sha(args.output),'compiler_executed':False}))


if __name__=='__main__':main()
