"""新N1接线的纯Python正负测试；从不执行scheduler/编译器。"""
import importlib.util
from pathlib import Path
import unittest
import tempfile
import json
import tracemalloc
import weakref
from unittest.mock import patch

MODULE=Path(__file__).resolve().parents[1]/'native/run_accelerated_incremental.py'


class AcceleratedRunTests(unittest.TestCase):
    """真实PCH前序、wrapper逐token和缺paired证据的最终重编分流。"""
    def setUp(self):
        """只导入新caller，不消费任何活产物。"""
        self.assertTrue(MODULE.exists(),'new-run bridge not implemented')
        spec=importlib.util.spec_from_file_location('accelerated',MODULE)
        self.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.m)

    def test_pch_actions_precede_consumers(self):
        """PCH必须取actual CMake头创建命令，不造虚拟对象。"""
        p={'source':'/b/cmake_pch.hxx.cxx','object':'/b/cmake_pch.hxx.gch','cwd':'/b','command':['/cc','-x','c++-header','-include','/b/cmake_pch.hxx','-c','/b/cmake_pch.hxx.cxx','-o','/b/cmake_pch.hxx.gch']}
        u={'source':'/s/a.cpp','object':'/b/a.o','cwd':'/b','command':['/cc','-include','/b/cmake_pch.hxx','-c','/s/a.cpp','-o','/b/a.o']}
        phases=self.m.partition_phases([u,p]);self.assertEqual(phases['pch'],[p]);self.assertEqual(phases['translation_units'],[u])
        with self.assertRaisesRegex(RuntimeError,'PCH'):
            self.m.partition_phases([u])

    def test_namespace_unknown_becomes_compile_without_pair(self):
        """最终plan不可遗留needsproof当采用。"""
        self.assertEqual(self.m.final_decision({'decision':'NEEDS_RESOLUTION_PROOF','reasons':['new_header']},None)['decision'],'COMPILE')

    def test_wrapper_does_not_rewrite_canonical_flags(self):
        """实际wrapper是新命令前缀，原命令另存不冒历史原argv。"""
        row={'source':'/s/a.cpp','object':'/b/a.o','cwd':'/b','command':['/cc','-O2','-c','/s/a.cpp','-o','/b/a.o']}
        got=self.m.wrap_action(row,{'path':'/e/profile.json','sha256':'a'*64},'/python','/tool/launcher.py')
        self.assertEqual(got['command'][-len(row['command']):],row['command']);self.assertEqual(got['canonical_command'],row['command'])

    def test_cache_hit_history_is_excluded(self):
        """不让轻RSS缓存命中降低未来原编译配额。"""
        history=[{'source':'/s/a','cache_outcome':'cache_miss','exit_code':0,'compiler_invoked':True},{'source':'/s/b','cache_outcome':'preprocessed_cache_hit','exit_code':0},{'source':'/s/c','maximum_resident_set_kib':1}]
        self.assertEqual(self.m.compiler_history(history),[history[0]])
        failure={'source':'/s/d','exit_code':125,'packed_memory_guard_stop':{'actual':True}}
        self.assertEqual(self.m.compiler_history(history+[failure]),[history[0],failure])

    def test_original_keep_going_crosses_batch_boundary(self):
        """批次不能截短原普通错误继续范围，资源停/PCH失败仍立即停。"""
        r={'status':'failed','pending':[],'results':[{'exit_code':1,'continued_after_compile_error':True}]}
        policy={'keep_going_compile_errors':True}
        self.assertTrue(self.m.may_continue_batch(r,policy,'translation_units'))
        self.assertFalse(self.m.may_continue_batch(r,policy,'pch'))
        r['status']='packed_memory_stop';self.assertFalse(self.m.may_continue_batch(r,policy,'translation_units'))
        r['status']='failed';r['results'][0]['continued_after_compile_error']=False
        self.assertFalse(self.m.may_continue_batch(r,policy,'translation_units'))

    def test_complete_prepare_partition_no_compiler(self):
        """真实prepare算法核全部请求文件与原N1 SHA；仅外部技术环境核验用显式synthetic替身。"""
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);src=root/'a.cpp';src.write_text('int x;')
            def ref(path):return {'path':str(path),'sha256':self.m.D.sha(path)}
            def put(name,value):
                path=root/name;path.write_text(json.dumps(value));return ref(path)
            action={'source':str(src),'object':str(root/'a.o'),'cwd':str(root),'source_sha256':self.m.D.sha(src),
                    'command':['/synthetic/c++','-c',str(src),'-o',str(root/'a.o')]}
            native=MODULE.parent
            helper_names=('run_accelerated_incremental.py','depfile_incremental.py','paired_resolution.py','build_acceleration.py','cache_snapshot.py','execution_context.py')
            frozen=put('frozen.json',{'files':[ref(src)]+[ref(native/name) for name in helper_names]})
            context=put('context.json',{'environment':{},'toolchain_sha256':'a'*64,'implicit_search_roots':[],
                    'search_evidence_sha256':'b'*64,'semantic_input_policy':'ordinary_cpp_no_external_inputs','frozen_manifest_sha256':frozen['sha256'],'source_root':str(root),'acceleration_semantics':self.m.A.acceleration_semantics(self.m.A.file_identity(src),str(root),'strict')})
            native=MODULE.parent
            request={'context':context,'source_root':str(root),'ccache':self.m.A.file_identity(src),'cache_mode':'strict','queue':put('queue.json',[action]),
                     'cmake_commands':put('commands.json',[{'directory':str(root),'arguments':action['command']}]),
                     'frozen_inputs':frozen,'scheduler':ref(native/'n1_compile_scheduler.py'),
                     'python':ref(src),'launcher':ref(native/'build_acceleration.py'),'policy':put('policy.json',{}),
                     'gnu_time':ref(src),'cache_snapshot_helper':ref(native/'cache_snapshot.py'),
                     'implementation_files':[ref(native/name) for name in ('run_accelerated_incremental.py','depfile_incremental.py','paired_resolution.py','build_acceleration.py','cache_snapshot.py','execution_context.py')],
                     'overwrite_protection':put('protected.json',{'files':[]})}
            with patch.object(self.m,'verify_context') as external_only:
                plan,_,_=self.m.prepare(request,root,self.m.P.EvidenceReader(),write=False)
                self.assertEqual((plan['compile_count'],plan['reused_count'],plan['translation_units']),(1,0,1));external_only.assert_called_once()
                request['cmake_commands']=put('foreign_commands.json',[{'directory':str(root),'arguments':action['command']+['-DFOREIGN=1']}])
                with self.assertRaisesRegex(RuntimeError,'exact_CMake'):self.m.prepare(request,root,self.m.P.EvidenceReader(),write=False)

    def test_uncached_original_history_keeps_lower_bound(self):
        """原guard125与较早成功峰同时保留，不把普通成功筛选变成低估。"""
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);driver=root/'cc';driver.write_text('synthetic, never executed');log=root/'time';log.write_text('original zero or time bytes')
            rows=[{'source':'/old/a.cpp','original_command':[str(driver),'-c','/old/a.cpp','-o','/b/a.o'],'exit_code':code,'time_log':str(log)} for code in (0,125)]
            rows[1]['packed_memory_guard_stop']={'original':True}
            path=root/'rows.json';path.write_text(json.dumps(rows))
            spec={'kind':'ORIGINAL_UNCACHED_N1_BUDGET_HISTORY','scheduler':{'path':str(MODULE.parent/'n1_compile_scheduler.py'),'sha256':self.m.ORIGINAL_N1_SHA256},
                  'compiler_identities':[self.m.A.file_identity(driver)],'time_logs':[self.m.A.file_identity(log)],'result_files':[self.m.A.file_identity(path)]}
            self.assertEqual(self.m.original_uncached_history(spec,self.m.P.EvidenceReader()),rows)

    def test_old_pch_preserved_before_new_atomic_creation(self):
        """只操作临时bytes；旧.gch必须已独立保护，失败不能覆盖保留位。"""
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);phase=root/'phase';phase.mkdir();obj=root/'cmake_pch.hxx.gch';obj.write_bytes(b'old pch bytes')
            snapshot=root/'snapshot';snapshot.write_bytes(obj.read_bytes());original=self.m.A.sha(obj)
            protection={'files':[{'original_path':str(obj),'snapshot_path':str(snapshot),'sha256':original}]}
            self.m.quarantine_old_pch([{'object':str(obj)}],phase,protection)
            self.assertFalse(obj.exists());self.assertEqual(self.m.A.sha(phase/'previous-pch-0000.preserved'),original)
            receipt=json.loads((phase/'old-pch-quarantine.json').read_text());self.assertEqual(receipt['files'][0]['original']['path'],str(obj))
            obj.write_bytes(b'new unknown original')
            with self.assertRaises(RuntimeError):self.m.quarantine_old_pch([{'object':str(obj)}],phase,protection)

    def test_state_index_retains_refs_not_json_and_rechecks(self):
        """只索引原ref；调用者改返回JSON不污染下一读，原件变化仍拒。"""
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'state.json';path.write_text(json.dumps({'action':{'object':'/b/a.o'},'namespace':['original']}))
            ref={'path':str(path),'sha256':self.m.D.sha(path)};reader=self.m.P.EvidenceReader()
            reader.checked(ref)  # 即使外层曾缓存同件，state索引也必须逐件释放JSON。
            states=self.m.CheckedStateIndex([ref],reader)
            self.assertEqual(len(states),1);self.assertIsNone(states.get('/missing'))
            self.assertTrue(all(value is None for _,value in reader.cache.values()))
            value=states['/b/a.o'];value['namespace'].append('caller mutation')
            self.assertEqual(states['/b/a.o']['namespace'],['original'])
            with self.assertRaisesRegex(RuntimeError,'duplicate_old_action_state'):self.m.CheckedStateIndex([ref,ref],reader)
            path.write_text(json.dumps({'action':{'object':'/b/a.o'},'namespace':['changed']}))
            with self.assertRaisesRegex(RuntimeError,'evidence_identity_changed'):states['/b/a.o']

    def test_high_reference_count_state_json_peak_is_bounded(self):
        """1024份小synthetic JSON仅测Python保留上界，不读真实state或声称产品RAM。"""
        with tempfile.TemporaryDirectory() as tmp:
            refs=[]
            for i in range(1024):
                path=Path(tmp)/f'{i}.json';path.write_text(json.dumps({'action':{'object':f'/b/{i}.o'},'namespace':'x'*8192}))
                refs.append({'path':str(path),'sha256':self.m.D.sha(path)})
            reader=self.m.P.EvidenceReader();live={'current':0,'maximum':0}
            loads=json.loads
            class TrackedState(dict):pass
            def released():live['current']-=1
            def tracked_loads(value):
                state=TrackedState(loads(value));live['current']+=1
                live['maximum']=max(live['maximum'],live['current'])
                weakref.finalize(state,released);return state
            tracemalloc.start()
            try:
                with patch.object(self.m.P.json,'loads',tracked_loads):
                    states=self.m.CheckedStateIndex(refs,reader)
                    for obj in states:
                        value=states[obj];self.assertEqual(value['action']['object'],obj);del value
                current,peak=tracemalloc.get_traced_memory()
            finally:tracemalloc.stop()
            self.assertEqual(len(states),1024)
            self.assertTrue(all(value is None for _,value in reader.cache.values()))
            self.assertLess(peak,4*1024*1024)
            self.assertEqual(live,{'current':0,'maximum':1})
            print(json.dumps({'kind':'SYNTHETIC_STATE_REFERENCE_PEAK_NOT_PRODUCT_RSS','states':1024,'payload_bytes_each':8192,'maximum_live_decoded_states':live['maximum'],'traced_current_bytes':current,'traced_peak_bytes':peak,'bound_bytes':4*1024*1024}))


if __name__=='__main__':unittest.main()
