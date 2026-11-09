"""真实临时.d/文件身份测试；不启动编译器，不把synthetic当产品依赖证明。"""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'native/depfile_incremental.py'


class DepfileTests(unittest.TestCase):
    """覆盖旧闭包变化、潜在新头、命令环境身份与缺证的逐TU拒门。"""
    def setUp(self):
        """每例独立普通文件，原object/MF从未由测试编译生成。"""
        self.assertTrue(MODULE.exists(), 'dependency planner not implemented')
        spec = importlib.util.spec_from_file_location('deps', MODULE)
        self.m = importlib.util.module_from_spec(spec); spec.loader.exec_module(self.m)
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.inc = self.root/'include';self.inc.mkdir()
        self.src = self.root/'src';self.src.mkdir();self.source=self.src/'main.cpp';self.source.write_text('#include "a.hpp"\nint x;\n')
        self.header=self.inc/'a.hpp';self.header.write_text('struct A {};\n')
        self.obj=self.root/'a.o';self.obj.write_bytes(b'synthetic object, not compiler result')
        self.dep=self.root/'a.d';self.dep.write_text(f'{self.obj}: {self.source} {self.header}\n')
        self.action={'source':str(self.source),'object':str(self.obj),'cwd':str(self.root),'command':['/synthetic/c++','-I'+str(self.inc),'-c',str(self.source),'-o',str(self.obj)]}
        self.context={'environment':{'CPATH':None},'toolchain_sha256':'a'*64,'frozen_manifest_sha256':'f'*64,'implicit_search_roots':[], 'search_evidence_sha256':'b'*64,'semantic_input_policy':'ordinary_cpp_no_external_inputs'}
        self.state=self.m.capture_state(self.action,self.dep,self.context,{'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m.sha(self.obj),'dependency_sha256':self.m.sha(self.dep)},synthetic=True)

    def decision(self):
        """新规划只读取文件，回传REUSE/COMPILE或需resolution证明。"""
        return self.m.classify(self.action,self.state,self.context,allow_synthetic=True)

    def test_unchanged_real_depfile_reuses(self):
        """已锁当前纯合成闭包相同才走窄复用路径。"""
        self.assertEqual(self.decision()['decision'],'REUSE')

    def test_unrelated_new_file_does_not_force_all(self):
        """不在该TU任何搜索目录的新增文档不会全局失效。"""
        (self.root/'docs').mkdir();(self.root/'docs/new.md').write_text('unrelated')
        self.assertEqual(self.decision()['decision'],'REUSE')

    def test_new_shadow_header_requires_proof(self):
        """原.d未列的新同名头仍会改变quote前置目录解析。"""
        (self.src/'a.hpp').write_text('shadow')
        self.assertEqual(self.decision()['decision'],'NEEDS_RESOLUTION_PROOF')

    def test_has_include_namespace_change_requires_proof(self):
        """包括未入旧.d的新存在性，不只比较已读headers。"""
        (self.inc/'optional.hpp').write_text('now exists')
        self.assertEqual(self.decision()['decision'],'NEEDS_RESOLUTION_PROOF')

    def test_dependency_bytes_change_compiles(self):
        """依赖内容变化不能借相同namespace放过。"""
        self.header.write_text('struct A {int changed;};\n')
        self.assertEqual(self.decision()['decision'],'COMPILE')

    def test_object_or_mf_tamper_compiles(self):
        """旧缓存和原MF任一变动均非可采用成功。"""
        self.obj.write_bytes(b'not original')
        self.assertEqual(self.decision()['decision'],'COMPILE')

    def test_environment_or_flags_compile(self):
        """新环境/原命令变化不能只看.d复用。"""
        self.context['environment']['CPATH']='changed'
        self.assertEqual(self.decision()['decision'],'COMPILE')
        self.context['environment']['CPATH']=None;self.action['command'].append('-DNEW=1')
        self.assertEqual(self.decision()['decision'],'COMPILE')

    def test_cache_execution_semantics_change_compiles(self):
        """新cache路径转换/配置不是旧driver语义，可用.d也不能掩盖这次换代。"""
        self.context['acceleration_semantics']={'ccache':{'sha256':'c'*64},'base_dir':'/new','hash_dir':False}
        self.assertEqual(self.decision()['decision'],'COMPILE')

    def test_symlink_retarget_requires_proof(self):
        """搜索目录内别名改变是namespace变化，旧目标仍在也不放行。"""
        a=self.inc/'optional.hpp';a.symlink_to('a.hpp')
        self.state=self.m.capture_state(self.action,self.dep,self.context,{'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m.sha(self.obj),'dependency_sha256':self.m.sha(self.dep)},synthetic=True)
        a.unlink();a.symlink_to('../src/main.cpp')
        self.assertEqual(self.decision()['decision'],'NEEDS_RESOLUTION_PROOF')

    def test_unknown_dynamic_include_requires_proof(self):
        """宏include无法从旧实际依赖推导所有负查询，保持未知。"""
        self.source.write_text('#define PICK "a.hpp"\n#include PICK\n')
        self.state=self.m.capture_state(self.action,self.dep,self.context,{'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m.sha(self.obj),'dependency_sha256':self.m.sha(self.dep)},synthetic=True)
        self.assertEqual(self.decision()['decision'],'NEEDS_RESOLUTION_PROOF')

    def test_joined_search_flags_preserve_empty_front_root(self):
        """合法joined系统/quote/after前根不能从旧MD漏掉，新增shadow必须重核。"""
        for flag in ('-isystem','-iquote','-idirafter'):
            with self.subTest(flag=flag):
                front=self.root/flag[1:];front.mkdir(exist_ok=True)
                self.action['command'].insert(1,flag+str(front))
                self.state=self.m.capture_state(self.action,self.dep,self.context,{'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m.sha(self.obj),'dependency_sha256':self.m.sha(self.dep)},synthetic=True)
                (front/'a.hpp').write_text('new shadow')
                self.assertEqual(self.decision()['decision'],'NEEDS_RESOLUTION_PROOF')
                self.action['command'].pop(1)


    def test_target_subprogram_and_volatile_macro_rejected(self):
        """-B切搜索或-D时间宏不能借旧MD未出现该token而复用。"""
        for flag in ('-B/other/toolchain','-DTODAY=__DATE__','-m32'):
            with self.subTest(flag=flag):
                self.action['command'].append(flag)
                state=self.m.capture_state(self.action,self.dep,self.context,{'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m.sha(self.obj),'dependency_sha256':self.m.sha(self.dep)},synthetic=True)
                self.assertEqual(self.m.classify(self.action,state,self.context,allow_synthetic=True)['decision'],'COMPILE')
                self.action['command'].pop()

    def test_typed_pch_receipt_is_bound_to_actual_bytes(self):
        """PCH不是凭扩展名跳过分析，创建终态和每项原依赖都必须匹配。"""
        pch=self.root/'cmake_pch.hxx.gch';pch.write_bytes(b'synthetic PCH')
        dependencies=[self.m.ident(self.header)]
        receipt=self.root/'creation.json';receipt.write_text(json.dumps({'kind':'ACTUAL_NEW_RUN_CCACHE_ACTION','action_kind':'pch_create','exit_code':0,'volatile_guard_passed':True,'output_identity':self.m.ident(pch),'dependency_identities':dependencies}))
        binding={'header':str(self.header),'object_identity':self.m.ident(pch),'creation_receipt':self.m.ident(receipt),'creation_dependencies':dependencies}
        self.m.verify_pch_binding(binding)
        pch.write_bytes(b'changed')
        with self.assertRaises(RuntimeError):self.m.verify_pch_binding(binding)


    def test_unimplemented_GCC_directory_aliases_compile(self):
        """sysroot operand与未实现目录别名不被当普通相对目录而误采用。"""
        for flags in (['-I=/foo'],['-isystem','$SYSROOT/foo'],['-iquote=/foo'],['-idirafter$SYSROOT/foo'],
                      ['-imultilib','other'],['-imultiarch','other'],['--include-prefix=/other'],['--prefix=/other'],['-includepre.hpp'],['-imacrosmacros.hpp'],['--no-sysroot-suffix'],['-x','assembler-with-cpp']):
            with self.subTest(flags=flags):
                action=copy.deepcopy(self.action);action['command']+=flags
                state=self.m.capture_state(action,self.dep,self.context,{'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m.sha(self.obj),'dependency_sha256':self.m.sha(self.dep)},synthetic=True)
                self.assertEqual(self.m.classify(action,state,self.context,allow_synthetic=True)['decision'],'COMPILE')

    def test_missing_historical_context_compiles(self):
        """旧真实记录没有新环境schema时不得补造旧史。"""
        self.assertEqual(self.m.classify(self.action,None,self.context)['decision'],'COMPILE')

    def test_make_escaped_paths_and_phony(self):
        """原make转义空格/#/$与MP空规则保解析，不用substring匹配。"""
        p=self.inc/'a b#$c.hpp';p.write_text('x')
        escaped=str(p).replace('$','$$').replace('#',r'\#').replace(' ',r'\ ')
        self.dep.write_text(f'{self.obj}: {self.source} \\\n {escaped}\n{escaped}:\n')
        self.assertEqual(set(self.m.parse_depfile(self.dep,str(self.root))),{str(self.source),str(p)})


if __name__ == '__main__':
    unittest.main()
