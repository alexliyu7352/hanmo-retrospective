"""新运行真实.d内容/解析namespace增量；未知逐TU重编，不修改原make_plan或历史证据。"""
from pathlib import Path
import hashlib
import copy
import json
import os
import re
import stat


def require(ok, reason):
    """不可由python -O关闭的证据拒门。"""
    if not ok:
        raise RuntimeError(reason)


def sha(path):
    """流式读原件并核读前后身份，避免写中流被封作成功。"""
    p = Path(path); before = p.stat(); digest = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    after = p.stat()
    require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns), 'file_changed_during_read')
    return digest.hexdigest()


def digest(value):
    """结构指纹只用于原结构身份，不归一预处理输出或源码。"""
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def ident(path):
    """保逻辑path、实际target与bytes；不同source绝对路径不是同输入。"""
    p = Path(os.path.abspath(path))
    return {'path':str(p), 'realpath':str(p.resolve()), 'bytes':p.stat().st_size, 'sha256':sha(p)}


def parse_depfile(path, cwd):
    """解析GCC make转义/续行/MP空规则；不使用会吞引号语义的shell解析器。"""
    text = Path(path).read_text().replace('\\\r\n', '').replace('\\\n', '')
    lines = text.splitlines(); require(lines, 'empty_depfile')
    colon = None; escaped = False
    for i, ch in enumerate(lines[0]):
        if ch == ':' and not escaped:
            colon = i; break
        escaped = ch == '\\' and not escaped
        if ch != '\\': escaped = False
    require(colon is not None, 'malformed_depfile')
    # -MP后续行必须是空的phony目标；不忽略第二个带依赖的真实规则。
    for line in lines[1:]:
        require(not line.strip() or line.rstrip().endswith(':'), 'multiple_dependency_rules_unsupported')
    rest = lines[0][colon+1:]; tokens = []; token = []; i = 0
    while i < len(rest):
        ch = rest[i]
        if ch == '\\':
            require(i+1 < len(rest), 'truncated_make_escape'); token.append(rest[i+1]); i += 2
        elif ch == '$':
            require(i+1 < len(rest) and rest[i+1] == '$', 'make_variable_unsupported'); token.append('$'); i += 2
        elif ch.isspace():
            if token: tokens.append(''.join(token)); token=[]
            i += 1
        elif ch == '#':
            raise RuntimeError('unescaped_make_comment_unsupported')
        else:
            token.append(ch); i += 1
    if token: tokens.append(''.join(token))
    require(tokens, 'empty_dependency_closure')
    # 保逻辑搜索path而非先resolve丢掉symlink身份。
    return sorted({os.path.abspath(x if os.path.isabs(x) else os.path.join(cwd,x)) for x in tokens})


def canonical_action(action):
    """保原argv全部语义token；这里只支持已明确单TU的真实CMake命令。"""
    for key in ('source','object','cwd'):
        require(Path(action[key]).is_absolute(), 'absolute_action_required')
    cmd = action['command']; require(isinstance(cmd,list) and all(isinstance(x,str) for x in cmd), 'argv_required')
    require(cmd.count('-c') == cmd.count('-o') == 1, 'single_CMake_action_required')
    require(cmd[cmd.index('-c')+1] == action['source'] and cmd[cmd.index('-o')+1] == action['object'], 'action_source_output_mismatch')
    require(not any(x.startswith('@') for x in cmd), 'response_files_unproved')
    return copy.deepcopy({key:action[key] for key in ('source','object','cwd','command')})


def strip_comments(text):
    """保字符串和换行，只移注释及行拼接；复杂词法保持保守未知。"""
    text = text.replace('\\\r\n','').replace('\\\n','')
    pattern = re.compile(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|/\*.*?\*/|//[^\n]*', re.S)
    return pattern.sub(lambda m:m.group(1) if m.group(1) is not None else '\n'*m.group(0).count('\n'),text)


def resolution_requirements(action, dependencies, context, pch_binding=None):
    """显式/隐式搜索根与每个已读文件quote目录；宏查询不假装有限负查询已知。"""
    cmd = action['command']; roots = [str(Path(action['source']).parent)]
    roots += [str(Path(x).parent) for x in dependencies if not x.endswith(('.gch','.pch'))]
    roots += context['implicit_search_roots']; unknown = []; unsupported = []
    i=1
    while i < len(cmd):
        flag=cmd[i]
        if flag in ('-I','-isystem','-iquote','-idirafter'):
            require(i+1<len(cmd),'missing_include_option')
            operand=cmd[i+1]
            if operand.startswith(('=','$SYSROOT')):unsupported.append('sysroot_expanded_include_operand_unproved')
            else:roots.append(os.path.abspath(os.path.join(action['cwd'],operand)))
            i+=2;continue
        if flag.startswith('-I') and len(flag)>2:
            if flag[2:].startswith(('=','$SYSROOT')):unsupported.append('sysroot_expanded_include_operand_unproved')
            else:roots.append(os.path.abspath(os.path.join(action['cwd'],flag[2:])))
        for prefix in ('-isystem','-iquote','-idirafter'):
            if flag.startswith(prefix) and len(flag)>len(prefix):
                operand=flag[len(prefix):]
                if operand.startswith(('=','$SYSROOT')):unsupported.append('sysroot_expanded_include_operand_unproved')
                else:roots.append(os.path.abspath(os.path.join(action['cwd'],operand)))
        if flag.startswith(('-B','-fplugin','-fmodules','-fprofile','-specs','--specs','-Wp,','-Xpreprocessor','--sysroot','-isysroot','-iprefix','-iwithprefix','-isystem-after','-imultilib','-imultiarch','--no-sysroot-suffix','-iplugindir','--include','--prefix','--include-prefix','--include-with-prefix','-Wa,','-Xassembler')):
            unsupported.append('unsupported_external_compiler_semantics')
        if flag.startswith(('-D','-U')):
            macro=cmd[i+1] if flag in ('-D','-U') and i+1<len(cmd) else flag[2:]
            if re.search(r'__(?:DATE|TIME|TIMESTAMP)__',macro):unsupported.append('volatile_command_macro')
            if '##' in macro or '\\' in macro or '%:' in macro:unknown.append('unproved_command_macro_construction')
        if flag.startswith('-m') and flag not in ('-m64',):unsupported.append('target_search_configuration_requires_new_resolution_contract')
        if flag.startswith(('-include','-imacros')) and flag not in ('-include','-imacros'):
            unsupported.append('joined_forced_include_unproved')
        if flag=='-x' and (i+1>=len(cmd) or cmd[i+1] not in ('c','c++','c++-header')):
            unsupported.append('non_ordinary_language_unproved')
        if flag in ('-include','-imacros'):
            require(i+1<len(cmd),'missing_forced_include')
            header=cmd[i+1]
            if not Path(header).is_absolute() or header not in dependencies:
                if not (pch_binding and header==pch_binding.get('header')):
                    unsupported.append('forced_include_without_actual_dependency')
        i+=1
    roots = list(dict.fromkeys(os.path.abspath(x) for x in roots))
    extra=[]
    for path in dependencies:
        if path.endswith(('.gch','.pch')):
            if not pch_binding or path!=pch_binding.get('object_identity',{}).get('path'):
                unsupported.append('PCH_dependency_requires_typed_creation_state')
            continue
        text=strip_comments(Path(path).read_text(errors='strict'))
        # 该完整directive只poison不展开时间宏；后续任何真实使用仍由原检查拒绝。
        text=re.sub(r'^#pragma GCC poison __DATE__ __TIME__ __TIMESTAMP__$','',text,flags=re.M)
        if '??' in text or '%:' in text or '\\u' in text or '\\U' in text:
            unknown.append('unproved_preprocessor_lexical_form')
        if '##' in text:
            unknown.append('unproved_macro_token_construction')
        if re.search(r'\b__(?:DATE|TIME|TIMESTAMP)__\b',text):
            unsupported.append('volatile_compile_macro')
        if re.search(r'\.(?:incbin|include)\b',text) or re.search(r'^\s*#\s*(?:embed|pragma\s+(?:GCC\s+)?dependency)\b',text,re.M):
            unsupported.append('external_semantic_file_input')
        for operand in re.findall(r'^\s*#\s*(?:include|include_next|import)\s+([^\n]+)',text,re.M):
            match=re.fullmatch(r'\s*[<"]([^>"\n]+)[>"]\s*',operand)
            if not match: unknown.append('macro_include_operand');continue
            name=match.group(1)
            if os.path.isabs(name): extra.append(str(Path(name).parent))
            elif '..' in Path(name).parts:
                extra += [os.path.abspath(os.path.join(r,name,'..')) for r in [str(Path(path).parent),*roots]]
        for query in re.findall(r'\b__has_include(?:_next)?\b([^\n]*)',text):
            match=re.match(r'\s*\(\s*[<"]([^>"\n]+)[>"]\s*\)',query)
            if not match: unknown.append('macro_has_include_operand');continue
            name=match.group(1)
            if os.path.isabs(name): extra.append(str(Path(name).parent))
            elif '..' in Path(name).parts:
                extra += [os.path.abspath(os.path.join(r,name,'..')) for r in [str(Path(path).parent),*roots]]
    return {'roots':list(dict.fromkeys(roots+extra)), 'unknown':sorted(set(unknown)), 'unsupported':sorted(set(unsupported))}


def namespace_identity(roots):
    """只锁名称/类型/权限/别名而非无关内容；缺根也记录，新增shadow不可漏。"""
    records=[]
    def walk(path, ancestors):
        """跟随真实目录链接并拒环/权限失败，不能把未读目录当空。"""
        p=Path(path)
        try: st=p.lstat()
        except FileNotFoundError:
            records.append({'path':str(p),'kind':'missing'});return
        row={'path':str(p),'mode':stat.S_IMODE(st.st_mode)}
        if p.is_symlink():
            row.update(kind='symlink',target=os.readlink(p),resolved=str(p.resolve()))
        elif stat.S_ISDIR(st.st_mode): row['kind']='directory'
        elif stat.S_ISREG(st.st_mode): row['kind']='file'
        else: raise RuntimeError('non_regular_search_namespace')
        records.append(row)
        if p.is_dir():
            real=str(p.resolve());require(real not in ancestors,'cyclic_search_namespace')
            before=p.stat()
            names=sorted(os.listdir(p))
            for name in names:walk(p/name,ancestors|{real})
            after=p.stat();require((before.st_mtime_ns,before.st_ctime_ns)==(after.st_mtime_ns,after.st_ctime_ns),'namespace_changed_during_read')
    canonical=sorted(set(os.path.abspath(x) for x in roots))
    require(all(x not in ('/','/home','/proc','/sys','/dev') for x in canonical),'unbounded_search_namespace')
    covered=[]
    for root in sorted(canonical,key=len):
        if not any(Path(root).is_relative_to(Path(parent)) for parent in covered):covered.append(root)
    for root in covered: walk(root,set())
    return {'roots':list(roots),'entries':records,'sha256':digest(records)}


def semantic_context(context):
    """全源freeze允许无关叶改变；命令解析工具、环境和搜索证据必须逐项相同。"""
    required=('environment','toolchain_sha256','implicit_search_roots','search_evidence_sha256','semantic_input_policy')
    require(all(key in context for key in required), 'historical_resolution_context_missing')
    require(context['semantic_input_policy']=='ordinary_cpp_no_external_inputs','unsupported_semantic_policy')
    require(all(Path(x).is_absolute() for x in context['implicit_search_roots']),'absolute_search_roots_required')
    return {**{key:context[key] for key in required},'acceleration_semantics':context.get('acceleration_semantics')}


def verify_pch_binding(binding):
    """额外PCH闭包只认真实创建终态与实际bytes，不能用名字同的.gch跳过底层头。"""
    for item in [binding['object_identity'], binding['creation_receipt'], *binding['creation_dependencies']]:
        require(ident(item['path'])==item, 'typed_PCH_binding_identity_changed')
    receipt=json.loads(Path(binding['creation_receipt']['path']).read_text())
    require(receipt.get('kind')=='ACTUAL_NEW_RUN_CCACHE_ACTION' and receipt.get('action_kind')=='pch_create' and
            receipt.get('exit_code')==0 and receipt.get('volatile_guard_passed') is True,'typed_PCH_creation_terminal_required')
    require(receipt['output_identity']['sha256']==binding['object_identity']['sha256'] and
            receipt['dependency_identities']==binding['creation_dependencies'],'typed_PCH_creation_bytes_or_closure_mismatch')
    require(binding['header'] in {x['path'] for x in binding['creation_dependencies']},'typed_PCH_header_not_in_creation_MD')


def capture_state(action, depfile, context, result, *, synthetic=False, pch_binding=None):
    """仅成功后捕获实际-MD及输入；旧MMD无环境记录不能回填成这个新代schema。"""
    action=canonical_action(action);semantic_context(context)
    require(result.get('exit_code')==0 and result.get('atomic_cache_update') is True,'successful_atomic_result_required')
    require(result['object_sha256']==sha(action['object']) and result['dependency_sha256']==sha(depfile),'actual_result_bytes_differ')
    if not synthetic:
        require(context.get('kind')=='CURRENT_EXECUTION_RESOLUTION_IDENTITY' and context.get('complete_system_headers') is True,'current_complete_identity_required')
        require(result.get('actual_dependency_mode')=='MD' and result.get('raw_result_identity'),'actual_full_MD_result_required')
        raw=result['raw_result_identity'];require(sha(raw['path'])==raw['sha256'],'raw_result_changed')
    deps=parse_depfile(depfile,action['cwd']);require(action['source'] in deps,'depfile_omits_source')
    if pch_binding:verify_pch_binding(pch_binding)
    req=resolution_requirements(action,deps,context,pch_binding)
    namespace=None
    if not req['unsupported']:
        try:namespace=namespace_identity(req['roots'])
        except (RuntimeError,OSError) as error:
            req['unsupported'].append('namespace_capture_unproved:'+str(error))
    return {'schema_version':1,'kind':'ACTUAL_FULL_MD_ACTION_STATE','synthetic':synthetic,'action':action,
            'result':copy.deepcopy(result),'context':copy.deepcopy(context),'object_identity':ident(action['object']),'depfile_identity':ident(depfile),
            'dependencies':[ident(p) for p in deps], 'resolution_requirements':req,'namespace':namespace,'pch_binding':copy.deepcopy(pch_binding)}


def classify(action, state, context, *, allow_synthetic=False):
    """每TU独立判定；未知返回需证明，最终plan没有证明必重编。"""
    try:
        action=canonical_action(action);semantic_context(context)
        require(state and state.get('kind')=='ACTUAL_FULL_MD_ACTION_STATE' and state.get('schema_version')==1,'no_current_format_historical_state')
        require(not state.get('synthetic') or allow_synthetic,'synthetic_state_not_adoptable')
        require(state['action']==action,'canonical_command_source_output_or_cwd_changed')
        require(semantic_context(state['context'])==semantic_context(context),'toolchain_environment_or_searchpath_changed')
        for item in [state['object_identity'],state['depfile_identity'],*state['dependencies']]:
            require(ident(item['path'])==item,'actual_dependency_object_or_MF_changed')
        deps=parse_depfile(state['depfile_identity']['path'],action['cwd'])
        require(deps==sorted(x['path'] for x in state['dependencies']),'actual_dependency_closure_changed')
        binding=state.get('pch_binding')
        if binding:verify_pch_binding(binding)
        req=resolution_requirements(action,deps,context,binding)
        require(not state['resolution_requirements']['unsupported'],'original_namespace_or_semantics_unproved')
        require(not req['unsupported'],'unsupported_semantics:'+','.join(req['unsupported']))
        if req['unknown']:
            return {'decision':'NEEDS_RESOLUTION_PROOF','reasons':req['unknown']}
        current=namespace_identity(req['roots'])
        if current!=state['namespace']:
            return {'decision':'NEEDS_RESOLUTION_PROOF','reasons':['include_namespace_changed']}
        return {'decision':'REUSE','reasons':[], 'original_object_sha256':state['object_identity']['sha256']}
    except (RuntimeError,OSError,ValueError,KeyError,TypeError) as error:
        return {'decision':'COMPILE','reasons':[str(error)]}
