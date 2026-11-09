"""只验证现有raw-E/MD证据consumer的拒门；不运行预处理器。"""
import importlib.util
import json
import tempfile
from pathlib import Path
import unittest

MODULE=Path(__file__).resolve().parents[1]/'native/paired_resolution.py'


class PairedResolutionTests(unittest.TestCase):
    """原始命令/输出与实际终态缺失时不能接受伪pair。"""
    def setUp(self):
        """只导入新纯读consumer，不调用任何外部程序。"""
        self.assertTrue(MODULE.exists(),'paired evidence consumer missing')
        spec=importlib.util.spec_from_file_location('paired',MODULE)
        self.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.m)
        self.row={'original_command':['/cc','-O2','-c','/src/a.cpp','-o','/out/a.o'],'source':'/src/a.cpp','object':'/out/a.o'}

    def fixture(self, new_changes=None, new_exit=0):
        """仅临时JSON/MD模拟既有真实格式的完整引用图，不标实际产品证明。"""
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=Path(temp.name)
        def put(name,value,raw=False):
            path=root/name;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(value if raw else json.dumps(value))
            return {'path':str(path),'bytes':path.stat().st_size,'sha256':self.m._d.sha(path)}
        source=root/'src.cpp';source.write_text('int x;')
        obj=root/'a.o';obj.write_text('synthetic, not compiler output')
        dep=put('old-object.d',f'{obj}: {source}\n',True)
        action={'source':str(source),'object':str(obj),'cwd':str(root),'command':['/synthetic/cc','-c',str(source),'-o',str(obj)]}
        row={**action,'original_command':action['command'],'source_sha256':self.m._d.sha(source),'exit_code':0,'atomic_cache_update':True,'object_sha256':self.m._d.sha(obj),'dependency_file':dep['path'],'dependency_sha256':dep['sha256'],'frozen_manifest_sha256':'f'*64}
        rows=put('rows.json',[row]);sample=put('sample.json',{'kind':'USER_ACCEPTED_CURRENT_ROUND_SAMPLE_TERMINAL','all40_equal':True,'equal_count':40,'actual_attempts':40,'error':None})
        frozen=put('frozen.json',{'files':[{'path':str(source),'sha256':row['source_sha256']}]});technical=put('technical.json',{'synthetic':True})
        observed_files=[{'path':str(source),'bytes':source.stat().st_size,'sha256':row['source_sha256']}]
        refs=[];sides={};observed={}
        for phase in ('old','new'):
            md=put(phase+'/actual.MD.d',f'{obj}: {source}\n',True);err=put(phase+'/stderr','',True);gnu=put(phase+'/gnu-time','synthetic no process',True)
            obs=put(phase+'/observed.json',{'files':observed_files});observed[phase]=obs
            proof={'phase':phase,'index':0,'old_result_index':0,'source':str(source),'object':str(obj),'cwd':str(root),'original_command':action['command'],
              'actual_command':self.m.probe_argv(row,md['path']),'original_object_sha256':row['object_sha256'],'original_MF_sha256':dep['sha256'],
              'original_row_frozen_sha256':'f'*64,'decision':'SIDE_PROVED_NOT_YET_ADOPTABLE','exit_code':0,'raw_preprocessor_bytes':10,'raw_preprocessor_lines':1,
              'raw_preprocessor_sha256':'a'*64,'dependency_count':1,'dependency_identity_sha256':self.m._d.digest(observed_files),
              'normalization':None,'volatile_inputs':[],'external_semantic_inputs':[],'external_preprocessed_asm':[], 'actual_MD':md,'stderr':err,'gnu_time':gnu}
            if phase=='new':proof.update(new_changes or {})
            sides[phase]=put(phase+'/result.json',proof);refs += [md,err,gnu,obs,sides[phase]]
        oldmanifest=put('old-manifest.json',{'synthetic':True})
        manifest=put('new-manifest.json',{'new_source':'synthetic-source','new_frozen':frozen,'technical_manifest':technical,'old_results':rows,'sample_terminal':sample,'old_phase_manifest':oldmanifest})
        terminals={};actuals={}
        for phase,mref in [('old',oldmanifest),('new',manifest)]:
            terminals[phase]=put(phase+'/terminal.json',{'phase':phase,'manifest_sha256':mref['sha256'],'complete':True,'completed':1,'total':1,'error':None,'results':[{'result':sides[phase]}]})
            actuals[phase]=put(phase+'/session.json',{'phase':phase,'exit_code':new_exit if phase=='new' else 0,'terminal':terminals[phase]})
            refs += [terminals[phase],actuals[phase]]
        refs.append(manifest)
        plan=put('plan.json',{'sample_terminal_sha256':sample['sha256'],'candidate_evidence':[{'source':str(source),'object':str(obj),'old_result_index':0,'decision':'ADOPT_UNCHANGED_OLD_ROW','reasons':[],'old_proof':sides['old'],'new_proof':sides['new']}]})
        lock=put('lock.json',{'files':refs})
        context={'source_commit':'synthetic-source','frozen_manifest_sha256':frozen['sha256'],'technical_manifest_sha256':technical['sha256'],
            'environment':{},'toolchain_sha256':'t'*64,'implicit_search_roots':[],'search_evidence_sha256':'s'*64,'semantic_input_policy':'ordinary_cpp_no_external_inputs','empirical_authorization_reference':'synthetic fixture, not user authority'}
        bundle={'kind':'EXISTING_STAGED_RAW_E_MD_PROOF_BUNDLE','new_manifest':manifest,'plan':plan,'lock':lock,'historical_admission':'user_current_round_40',
            'execution_context_sha256':self.m._d.digest(self.m._d.semantic_context(context)),'empirical_authorization_reference':context['empirical_authorization_reference']}
        for phase in ('old','new'):bundle.update({phase+'_terminal':terminals[phase],phase+'_actual_session':actuals[phase],phase+'_observed_inputs':observed[phase]})
        return action,context,bundle

    def test_complete_synthetic_graph_and_wrong_actual_exit(self):
        """完整临时引用图可走到末端；tool非零即使有terminal文件仍拒。"""
        action,context,bundle=self.fixture()
        result=self.m.verify_pair(action,None,context,bundle)
        self.assertTrue(result['accepted'],result)
        action,context,bundle=self.fixture(new_exit=1)
        result=self.m.verify_pair(action,None,context,bundle)
        self.assertFalse(result['accepted']);self.assertIn('actual_side_tool_terminal_required',result['reasons'])

    def test_normalization_and_equal_length_wrong_hash_rejected(self):
        """全图hash更新也不能洗掉语义raw不等或归一化。"""
        for change in [{'normalization':'strip_line_markers'},{'raw_preprocessor_sha256':'b'*64}]:
            action,context,bundle=self.fixture(change)
            self.assertFalse(self.m.verify_pair(action,None,context,bundle)['accepted'])

    def test_original_probe_conversion(self):
        """严格复用原O已审变换，不归一__FILE__或字节。"""
        self.assertEqual(self.m.probe_argv(self.row,'/e/MD.d'),['/cc','-O2','-E','/src/a.cpp','-MD','-MF','/e/MD.d','-MT','/out/a.o'])

    def test_external_semantics_refused(self):
        """plugin/response等非E闭包不靠两个相等hash放行。"""
        for flag in ['@args','-fplugin=x','-Wp,-DXX','-fprofile-use=outside','-P']:
            row=dict(self.row,original_command=self.row['original_command']+[flag])
            with self.assertRaises(RuntimeError):self.m.probe_argv(row,'/e/MD.d')

    def test_missing_pair_is_not_adoption(self):
        """缺证只返回拒绝理由，不读取任意猜测位置或伪造E。"""
        verdict=self.m.verify_pair({},None,{},None)
        self.assertFalse(verdict['accepted']);self.assertTrue(verdict['reasons'])

    def test_raw_pair_difference(self):
        """同token含义也不能归一原输出byte或line差别。"""
        a={'raw_preprocessor_bytes':10,'raw_preprocessor_lines':2,'raw_preprocessor_sha256':'a'*64,'dependency_count':1,'dependency_identity_sha256':'b'*64}
        b=dict(a,raw_preprocessor_sha256='c'*64)
        with self.assertRaisesRegex(RuntimeError,'paired_raw_preprocessor_sha256'):
            self.m.compare_sides(a,b)


    def test_only_exact_executed_poison_declaration_is_nonvolatile(self):
        """仅纯临时证据；不执行GCC，不把相同basename当豁免。"""
        import copy
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);guard=root/'guard.hpp'
            guard.write_bytes(self.m._a.POISON_GUARD_BYTES)
            g=self.m._a.file_identity(guard)
            command=['/synthetic/cc','-include',str(guard),'-Werror=date-time','-Wsystem-headers','-Werror=invalid-pch','-c','/s/a.cpp','-o','/b/a.o']
            receipt=dict(kind='ACTUAL_NEW_RUN_CCACHE_ACTION',exit_code=0,volatile_guard_passed=True,volatile_guard=g,canonical_command=command,output_identity={'sha256':'o'},dependency_identities=[g])
            rp=root/'receipt.json';rp.write_text(json.dumps(receipt));ref=self.m._d.ident(rp)
            row=dict(original_command=command,object_sha256='o',launcher_receipt=ref)
            state=dict(result=dict(launcher_receipt=ref))
            evidence=dict(kind='EXACT_ORIGINAL_EXECUTION_FORCED_POISON_DECLARATION_ONLY',guard=g,launcher_receipt=ref)
            original=dict(decision='RECOMPILE_UNPROVED',exit_code=0,volatile_inputs=[str(guard)],changed_or_added_dependencies=[],external_semantic_inputs=[],external_preprocessed_asm=False,normalization=None,maximum_resident_set_kib=123,raw_preprocessor_sha256='raw-not-normalized')
            bp=root/'original-side.json';bp.write_text(json.dumps(original));bref=self.m._d.ident(bp)
            proof=dict(original,kind='CAPTURED_EXECUTION_RAW_E_MD_POISON_AWARE',decision='SIDE_PROVED_NOT_YET_ADOPTABLE',poison_declaration_evidence=evidence,original_volatile_lexical_inputs=[str(guard)],volatile_inputs=[],original_side_result=bref)
            obs={str(guard):{**g,'volatile_lexical_hits':['__DATE__','__TIME__','__TIMESTAMP__']}}
            refs={ref['path']:ref,g['path']:g,bref['path']:bref}
            self.m.verify_exact_poison_declaration(proof,row,state,obs,[str(guard)],refs,self.m.EvidenceReader())
            other=root/'other.hpp';other.write_text('int ordinary;')
            for invalid in ['MISSING',None,False,'',0,{},['__DATE__','__DATE__'],['UNKNOWN']]:
                bad=dict(obs);record=self.m._d.ident(other)
                if invalid!='MISSING':record['volatile_lexical_hits']=invalid
                bad[str(other)]=record
                with self.assertRaises(RuntimeError):self.m.verify_exact_poison_declaration(proof,row,state,bad,[str(guard),str(other)],refs,self.m.EvidenceReader())
            with self.assertRaises(RuntimeError):self.m.verify_exact_poison_declaration(dict(proof,raw_preprocessor_sha256='rewritten'),row,state,obs,[str(guard)],refs,self.m.EvidenceReader())
            other=root/'other.hpp';other.write_text('__DATE__')
            bad=dict(obs);bad[str(other)]={**self.m._d.ident(other),'volatile_lexical_hits':['__DATE__']}
            with self.assertRaises(RuntimeError):self.m.verify_exact_poison_declaration(proof,row,state,bad,[str(guard),str(other)],refs,self.m.EvidenceReader())
            for badproof in [dict(proof,original_volatile_lexical_inputs=[]),dict(proof,kind='generic'),dict(proof,volatile_inputs=[str(guard)])]:
                with self.assertRaises(RuntimeError):self.m.verify_exact_poison_declaration(badproof,row,state,obs,[str(guard)],refs,self.m.EvidenceReader())
            other_guard=root/'same-name/guard.hpp';other_guard.parent.mkdir();other_guard.write_bytes(guard.read_bytes())
            badrow=dict(row,original_command=[x if x!=str(guard) else str(other_guard) for x in command])
            with self.assertRaises(RuntimeError):self.m.verify_exact_poison_declaration(proof,badrow,state,obs,[str(guard)],refs,self.m.EvidenceReader())
            guard.write_bytes(guard.read_bytes()+b'// extra token __TIME__\n')
            with self.assertRaises(RuntimeError):self.m.verify_exact_poison_declaration(proof,row,state,obs,[str(guard)],refs,self.m.EvidenceReader())


    def test_rehashed_extra_use_changed_header_and_missing_force_include(self):
        """攻击者同步更新全部ref/receipt也不能越过精确头字节和首include门。"""
        cases=[('actual-use',self.m._a.POISON_GUARD_BYTES+b'const char *time_value = __DATE__;\n',True,'exact_poison_guard_required'),
               ('changed-comment',self.m._a.POISON_GUARD_BYTES+b'// changed header\n',True,'exact_poison_guard_required'),
               ('missing-include',self.m._a.POISON_GUARD_BYTES,False,'poison_guard_must_be_first_force_include')]
        for label,content,include,error in cases:
            with self.subTest(case=label),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);guard=root/'guard.hpp';guard.write_bytes(content);g=self.m._a.file_identity(guard)
                command=['/synthetic/cc']+(['-include',str(guard)] if include else [])+['-Werror=date-time','-Wsystem-headers','-Werror=invalid-pch','-c','/s/a.cpp','-o','/b/a.o']
                receipt=dict(kind='ACTUAL_NEW_RUN_CCACHE_ACTION',exit_code=0,volatile_guard_passed=True,volatile_guard=g,canonical_command=command,output_identity={'sha256':'o'},dependency_identities=[g])
                rp=root/'receipt.json';rp.write_text(json.dumps(receipt));ref=self.m._d.ident(rp)
                row=dict(original_command=command,object_sha256='o',launcher_receipt=ref);state=dict(result=dict(launcher_receipt=ref))
                evidence=dict(kind='EXACT_ORIGINAL_EXECUTION_FORCED_POISON_DECLARATION_ONLY',guard=g,launcher_receipt=ref)
                original=dict(decision='RECOMPILE_UNPROVED',exit_code=0,volatile_inputs=[str(guard)],changed_or_added_dependencies=[],external_semantic_inputs=[],external_preprocessed_asm=False,normalization=None,maximum_resident_set_kib=123)
                bp=root/'original-side.json';bp.write_text(json.dumps(original));bref=self.m._d.ident(bp)
                proof=dict(original,kind='CAPTURED_EXECUTION_RAW_E_MD_POISON_AWARE',decision='SIDE_PROVED_NOT_YET_ADOPTABLE',poison_declaration_evidence=evidence,original_volatile_lexical_inputs=[str(guard)],volatile_inputs=[],original_side_result=bref)
                obs={str(guard):{**g,'volatile_lexical_hits':['__DATE__','__TIME__','__TIMESTAMP__']}};refs={ref['path']:ref,g['path']:g,bref['path']:bref}
                with self.assertRaisesRegex(RuntimeError,error):self.m.verify_exact_poison_declaration(proof,row,state,obs,[str(guard)],refs,self.m.EvidenceReader())


if __name__=='__main__':unittest.main()
