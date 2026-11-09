"""只用临时普通文件测试启动器身份/拒门；不调用任何编译器或缓存程序。"""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'native/build_acceleration.py'


class AccelerationTests(unittest.TestCase):
    """缺工具、错命令与未授权PCH必须在实际外呼之前拒绝。"""
    def setUp(self):
        """真实临时文件模拟身份字节，不伪造编译器执行结果。"""
        self.assertTrue(MODULE.exists(), 'new strict build acceleration module is not implemented')
        spec = importlib.util.spec_from_file_location('accel', MODULE)
        self.m = importlib.util.module_from_spec(spec); spec.loader.exec_module(self.m)
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for n in ('ccache', 'c++', 'main.cpp', 'toolchain.json'):
            (self.root / n).write_text(n)
            if n in ('ccache', 'c++'):
                (self.root / n).chmod(0o700)
        (self.root/'cache').mkdir(); (self.root/'evidence').mkdir()
        self.cmd = [str(self.root/'c++'), '-O2', '-g0', '-c', str(self.root/'main.cpp'), '-o', str(self.root/'main.o')]
        self.p = {'schema_version': 1, 'mode': 'strict', 'base_dir': str(self.root), 'cache_dir': str(self.root/'cache'), 'max_size': '1G', 'evidence_dir': str(self.root/'evidence'), 'ccache': self.m.file_identity(self.root/'ccache'), 'compiler': self.m.file_identity(self.root/'c++'), 'technical_inputs': [self.m.file_identity(self.root/'toolchain.json')], 'environment': self.m.environment_identity({}), 'actions': [{'command': self.cmd, 'cwd': str(self.root), 'source': str(self.root/'main.cpp'), 'object': str(self.root/'main.o'), 'kind': 'cxx', 'pch_inputs': [], 'input_files': [self.m.file_identity(self.root/'main.cpp')]}]}
        cfg=self.root/'ccache.conf';cfg.write_text(self.m.render_config(self.p));self.p['configuration']=self.m.file_identity(cfg)
        self.p['toolchain_files']=[self.p['compiler']];self.p['toolchain_sha256']='a'*64
        context=self.root/'context.json';context.write_text(json.dumps({'toolchain_sha256':self.p['toolchain_sha256'],'environment':self.p['environment'],'acceleration_semantics':self.m.acceleration_semantics(self.p['ccache'],self.p['base_dir'],'strict')}));self.p['execution_context_identity']=self.m.file_identity(context)
        key=self.root/'semantic-key.json';key.write_text(json.dumps(self.m.cache_key_material(self.p)));self.p['cache_key_identity']=self.m.file_identity(key)

    def test_strict_configuration(self):
        """默认保持完整宏检查，不因目标想要hit而放宽。"""
        text=self.m.render_config(self.p)
        self.assertIn('sloppiness = # explicit empty value\n', text);self.assertIn('direct_mode = false',text)
        self.assertIn('hash_dir = false',text);self.assertNotIn('time_macros',text)

    def test_empty_values_are_comment_delimited_for_ccache_4142(self):
        """4.14.2的空值扫描跨行；显式comment留空且不吞下一key。"""
        text=self.m.render_config(self.p)
        self.assertIn('sloppiness = # explicit empty value\n',text)
        self.assertIn('remote_storage = # explicit empty value\n',text)
        self.assertNotRegex(text,r'(?m)^\w+ =\s*$')

    def test_prepared_template_has_no_bare_empty_values(self):
        """旧纯准备模板同样不能保留4.14.2会跨行吞值的语法。"""
        text=MODULE.with_name('ccache.conf.in').read_text()
        self.assertIn('sloppiness = # explicit empty value\n',text)
        self.assertIn('remote_storage = # explicit empty value\n',text)
        self.assertNotRegex(text,r'(?m)^\w+ =\s*$')

    def test_exact_command_and_scoped_environment(self):
        """只新子进程清原CCACHE_DISABLE，父字典不改，系统依赖改用MD。"""
        env={'CCACHE_DISABLE':'1','CCACHE_SLOPPINESS':'time_macros'}
        inv=self.m.prepare_invocation(self.p,self.cmd+['-MMD','-MF',str(self.root/'new.d'),'-MT',str(self.root/'main.o')],str(self.root),env)
        self.assertNotIn('CCACHE_DISABLE',inv['environment']);self.assertNotIn('CCACHE_SLOPPINESS',inv['environment'])
        self.assertEqual(env['CCACHE_DISABLE'],'1');self.assertIn('-MD',inv['compiler_command']);self.assertNotIn('-MMD',inv['compiler_command'])

    def test_changed_flags_rejected(self):
        """不能任意去掉wrapper前缀/flags后冒原CMake动作。"""
        with self.assertRaisesRegex(RuntimeError,'exact_action'):
            self.m.prepare_invocation(self.p,self.cmd+['-DFOREIGN=1'],str(self.root),{})

    def test_missing_ccache_rejected(self):
        """缺工具拒绝，不能把绕过执行当已启用cache。"""
        (self.root/'ccache').unlink()
        with self.assertRaisesRegex(RuntimeError,'identity'):
            self.m.prepare_invocation(self.p,self.cmd,str(self.root),{})

    def test_technical_environment_drift_rejected(self):
        """影响include的环境改变必须失效。"""
        with self.assertRaisesRegex(RuntimeError,'environment'):
            self.m.prepare_invocation(self.p,self.cmd,str(self.root),{'CPATH':'/other'})

    def test_strict_pch_rejected_not_bypassed(self):
        """98%动作不能默默绕cache后声称双优化。"""
        self.p['actions'][0]['kind']='pch_create'
        with self.assertRaisesRegex(RuntimeError,'pch_requires_explicit_guarded_policy'):
            self.m.prepare_invocation(self.p,self.cmd,str(self.root),{})

    def test_guarded_mode_without_approval_rejected(self):
        """受控共用必须完整批准与宏前提，不能只设sloppiness。"""
        self.p['mode']='guarded_pch'
        with self.assertRaisesRegex(RuntimeError,'approval'):
            self.m.prepare_invocation(self.p,self.cmd,str(self.root),{})

    def test_hit_never_admission_evidence(self):
        """轻量cachehit不能降低下次真实编译的RSS预算。"""
        self.assertFalse(self.m.compiler_peak_eligible({'cache_outcome':'preprocessed_cache_hit'}))
        self.assertFalse(self.m.compiler_peak_eligible({'cache_outcome':'unknown'}))
        self.assertTrue(self.m.compiler_peak_eligible({'cache_outcome':'cache_miss','exit_code':0,'compiler_invoked':True}))

    def test_pch_context_macros_are_not_erased(self):
        """真实创建/使用只去语言包装和源token，宏或优化变化必须仍可见。"""
        creation = self.cmd + ['-x', 'c++-header', '-DVALUE=1']
        use = self.cmd + ['-DVALUE=1']
        self.assertEqual(self.m.pch_context(creation, str(self.root), self.p['compiler'], self.p['environment']),
                         self.m.pch_context(use, str(self.root), self.p['compiler'], self.p['environment']))
        self.assertNotEqual(self.m.pch_context(creation, str(self.root), self.p['compiler'], self.p['environment']),
                            self.m.pch_context(self.cmd + ['-DVALUE=2'], str(self.root), self.p['compiler'], self.p['environment']))

    def test_changed_source_rejected(self):
        """同路径source字节变化不能凭原action字符串放行。"""
        (self.root/'main.cpp').write_text('changed')
        with self.assertRaisesRegex(RuntimeError, 'identity'):
            self.m.prepare_invocation(self.p, self.cmd, str(self.root), {})

    def test_run_metadata_does_not_poison_cache_key(self):
        """run/log目录变化不强制全部miss；真实工具/环境变化仍进键。"""
        key=self.m.cache_key_material(self.p)
        self.p['evidence_dir']='/another/run';self.p['actions']=[dict(self.p['actions'][0],source='/another/source.cpp')]
        self.assertEqual(self.m.cache_key_material(self.p),key)
        self.p['environment']['CPATH']='changed'
        self.assertNotEqual(self.m.cache_key_material(self.p),key)

    def test_poison_guard_order_and_macro_exemption(self):
        """首include硬poison，不允许先定义宏/强制头绕过GCC既有宏豁免。"""
        guard=self.root/'volatile_macro_guard.hpp';guard.write_bytes(self.m.POISON_GUARD_BYTES)
        ref=self.m.file_identity(guard)
        flags=['-Werror=date-time','-Wsystem-headers','-Werror=invalid-pch','-include',str(guard),'-include','/b/cmake_pch.hxx']
        self.m.validate_poison_command(flags,ref)
        for extra in [['-DOLD=__TIME__'],['-DGLUE(a,b)=a##b'],['-DOLD=\\u005f_TIME__'],['-imacros','/x'],['-Wp,-include,/x'],['-DOLD=_Pragma("x")']]:
            with self.assertRaises(RuntimeError):self.m.validate_poison_command(flags+extra,ref)
        with self.assertRaises(RuntimeError):self.m.validate_poison_command(['-include','/first']+flags,ref)
        self.m.validate_poison_command(flags+['-DCOUNT=2','-D','NAME="fixed"'],ref)
        guard.write_text('// wrong guard')
        with self.assertRaises(RuntimeError):self.m.validate_poison_command(flags,ref)

    def test_poison_guard_actual_dependency_and_pragma(self):
        """动态diagnostic不是poison豁免；不词法拒正常stdlib，但必需真实guard依赖。"""
        guard=self.root/'volatile_macro_guard.hpp';guard.write_bytes(self.m.POISON_GUARD_BYTES)
        ref=self.m.file_identity(guard)
        flags=['-Werror=date-time','-Wsystem-headers','-Werror=invalid-pch','-include',str(guard)]
        src=self.root/'main.cpp';src.write_text('_Pragma("GCC diagnostic ignored \"-Wdate-time\"")')
        self.m.validate_volatile_guard(flags,[str(guard),str(src)],ref)
        with self.assertRaises(RuntimeError):self.m.validate_volatile_guard(flags,[str(src)],ref)

    def test_configuration_injection_rejected(self):
        """换行不能在原路径配置中注入sloppiness或remote。"""
        self.p['base_dir'] += '\nsloppiness = time_macros'
        with self.assertRaisesRegex(RuntimeError,'configuration_value'):
            self.m.render_config(self.p)

    def test_context_cache_configuration_is_same_actual_protocol(self):
        """实际ccache/config语义必须与.d context相同，不把缺字段当历史已证。"""
        semantics=self.m.acceleration_semantics(self.p['ccache'],self.p['base_dir'],'strict')
        self.assertEqual(semantics['configuration_by_mode']['strict']['hash_dir'],False)
        self.assertEqual(semantics['configuration_by_mode']['strict']['sloppiness'],'')
        context=json.loads(Path(self.p['execution_context_identity']['path']).read_text())
        context['acceleration_semantics']=semantics
        context['acceleration_semantics']['configuration_by_mode']['strict']['hash_dir']=True
        path=Path(self.p['execution_context_identity']['path']);path.write_text(json.dumps(context));self.p['execution_context_identity']=self.m.file_identity(path)
        with self.assertRaisesRegex(RuntimeError,'acceleration_context'):
            self.m.prepare_invocation(self.p,self.cmd+['-MD','-MF',str(self.root/'new.d')],str(self.root),{})


if __name__ == '__main__':
    unittest.main()
