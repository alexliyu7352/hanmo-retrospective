"""只测试有限请求/产物选择与不可变输入门；不把小fixture当正式银行通过。"""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import os
import subprocess
import unittest
import native_bank_risk as risk


class NativeBankRiskContract(unittest.TestCase):
    def setUp(self):
        """构造纯元数据样本，执行器与原生运行均不启动。"""
        self.req={'format':'HANMO-NATIVE-BANK-RISK/1','repository':risk.REPOSITORY,
                  'native_run_id':123,'source_commit':'a'*40,'probe_id':risk.PROBE}
        self.run={'id':123,'head_sha':'a'*40,'event':'push','repository':{'id':7,'full_name':risk.REPOSITORY},
                  'head_repository':{'id':7,'full_name':risk.REPOSITORY},
                  'path':risk.WORKFLOW,'status':'completed','conclusion':'success'}
        self.art={'total_count':1,'artifacts':[{'name':'native-ubuntu2004-'+'a'*40,
                  'id':5,'expired':False,'size_in_bytes':100,
                  'workflow_run':{'id':123,'repository_id':7,'head_repository_id':7,'head_sha':'a'*40}}]}

    def test_request_exact(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'request.json';p.write_text(json.dumps(self.req))
            self.assertEqual(risk.request(p),self.req)
            for field,value in [('source_commit','main'),('repository','other/repo'),
                                ('native_run_id',True),('probe_id','arbitrary-command')]:
                wrong=dict(self.req,**{field:value});p.write_text(json.dumps(wrong))
                with self.assertRaises(ValueError):risk.request(p)

    def test_success_unique(self):
        self.assertEqual(risk.select(self.req,self.run,self.art)['artifact_id'],5)

    def test_dispatch_source_is_separate_from_workflow_head(self):
        """实际手动原生run的工作流头不同，source名与真实归属仍严格且独验不能省略。"""
        run=dict(self.run,event='workflow_dispatch',head_sha='b'*40)
        artifacts=copy.deepcopy(self.art);artifacts['artifacts'][0]['workflow_run']['head_sha']='b'*40
        self.assertEqual(risk.select(self.req,run,artifacts)['sha'],'a'*40)
        for event in ['push','pull_request','workflow_run',None]:
            with self.assertRaises(ValueError):risk.select(self.req,dict(run,event=event),artifacts)
        wrong=copy.deepcopy(artifacts);wrong['artifacts'][0]['name']='native-ubuntu2004-'+'b'*40
        with self.assertRaises(ValueError):risk.select(self.req,run,wrong)
        source=(Path(__file__).resolve().parent/'native_bank_risk.py').read_text()
        self.assertIn("before=module.verify(artifacts,req['source_commit'])",source)
        self.assertLess(source.index("before=module.verify("),source.index("wraps=symbols("))

    def test_artifact_exact_workflow_ownership(self):
        """同名ZIP不能借其他run、fork仓库或另一个工作流头混入。"""
        for field,value in [('id',124),('repository_id',8),('head_repository_id',8),('head_sha','b'*40)]:
            wrong=copy.deepcopy(self.art);wrong['artifacts'][0]['workflow_run'][field]=value
            with self.assertRaises(ValueError):risk.select(self.req,self.run,wrong)
        wrong=copy.deepcopy(self.art);del wrong['artifacts'][0]['workflow_run']
        with self.assertRaises(ValueError):risk.select(self.req,self.run,wrong)
        for field,value in [('repository',{'id':True,'full_name':risk.REPOSITORY}),('head_repository',{'id':8,'full_name':'fork/hanmo'}),('event','schedule')]:
            with self.assertRaises(ValueError):risk.select(self.req,dict(self.run,**{field:value}),self.art)

    def test_non_success_and_wrong_identity(self):
        for field,value in [('conclusion','failure'),('status','in_progress'),('head_sha','b'*40),
                            ('path','.github/workflows/playable-preview.yml'),('id',124),
                            ('repository',{'full_name':'other/repo'})]:
            with self.assertRaises(ValueError):risk.select(self.req,dict(self.run,**{field:value}),self.art)

    def test_incomplete_duplicate_expired_artifacts(self):
        bad=[]
        x=copy.deepcopy(self.art);x['total_count']=2;bad.append(x)
        x=copy.deepcopy(self.art);x['total_count']=2;x['artifacts']*=2;bad.append(x)
        x=copy.deepcopy(self.art);x['artifacts'][0]['expired']=True;bad.append(x)
        x=copy.deepcopy(self.art);x['artifacts'][0]['name']='other';bad.append(x)
        for value in bad:
            with self.assertRaises(ValueError):risk.select(self.req,self.run,value)

    def test_exact_input_and_wrong_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'bank').write_bytes(b'original')
            rows=[{'path':'bank','bytes':8,'sha256':hashlib.sha256(b'original').hexdigest()}]
            self.assertEqual(len(risk.files(root,rows)),1)
            (root/'bank').write_bytes(b'modified')
            with self.assertRaises(ValueError):risk.files(root,rows)

    def test_escape_and_symlink(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'bank').write_bytes(b'original');(root/'alias').symlink_to(root/'bank')
            for path in ['../bank',str(root/'bank'),'alias']:
                with self.assertRaises(ValueError):risk.files(root,[{'path':path,'bytes':8,'sha256':hashlib.sha256(b'original').hexdigest()}])

    def test_workflow_bounded_trigger(self):
        root=Path(__file__).resolve().parents[2]
        text=(root/'.github/workflows/native-bank-risk.yml').read_text()
        self.assertIn("paths: ['verification/native-bank-risk-run.json', '.github/workflows/native-bank-risk.yml']",text)
        self.assertNotIn('cmake --build',text)
        self.assertIn('verify_native_outputs.py',text)
        self.assertIn('ref: ${{ steps.native.outputs.sha }}',text)
        self.assertIn('repository: （原私有仓库）',text)
        self.assertIn('            /.gitattributes\n            /.gitignore\n',text)
        self.assertIn('env HOME=/tmp PYTHONDONTWRITEBYTECODE=1',text)
        self.assertIn('HEAD:$metadata',text)
        self.assertIn('  workflow_dispatch:\n',text)
        self.assertIn('ref: ${{ github.sha }}',text)
        self.assertNotIn('${{ inputs.',text)
        self.assertNotIn('workflow_dispatch:\n    inputs:',text)


    def test_sparse_partial_clone_keeps_git_metadata_offline(self):
        """三文件本地仓证明原缺ignore会懒取；修后断开原仓仍严格辨识净源/脏源。"""
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);origin=root/'origin';checkout=root/'checkout';origin.mkdir()
            env=dict(os.environ,HOME=str(root),GIT_CONFIG_NOSYSTEM='1')
            def git(*args,cwd=origin,check=True):
                result=subprocess.run(['git',*map(str,args)],cwd=cwd,env=env,text=True,
                                      stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
                if check:self.assertEqual(result.returncode,0,result.stdout)
                return result
            git('init','-q');(origin/'.gitattributes').write_text('*.cpp text eol=lf\n')
            (origin/'.gitignore').write_text('output/\n');(origin/'tools/ci').mkdir(parents=True)
            (origin/'tools/ci/probe.cpp').write_text('int main() { return 0; }\n')
            git('add','.');git('-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','-qm','fixture')
            git('config','uploadpack.allowFilter','true')
            # Git 2.25 的本地 promisor 服务须明确允许获取已被提交树引用的单个 blob。
            git('config','uploadpack.allowReachableSHA1InWant','true')
            git('clone','--filter=blob:none','--no-checkout',origin.as_uri(),checkout)
            # 先安装非锥形范围再首次 checkout；旧 Git 的 sparse init 会先取根文件，掩盖原缺件。
            git('config','core.sparseCheckout','true',cwd=checkout)
            (checkout/'.git/info/sparse-checkout').write_text('/tools/ci/**\n')
            git('checkout','-q','HEAD',cwd=checkout)
            unavailable=(root/'no-remote').as_uri();git('remote','set-url','origin',unavailable,cwd=checkout)
            self.assertNotEqual(git('status','--porcelain=v1',cwd=checkout,check=False).returncode,0)
            git('remote','set-url','origin',origin.as_uri(),cwd=checkout)
            git('sparse-checkout','set','/.gitattributes','/.gitignore','/tools/ci/**',cwd=checkout)
            git('remote','set-url','origin',unavailable,cwd=checkout)
            self.assertEqual(git('status','--porcelain=v1',cwd=checkout).stdout,'')
            (checkout/'tools/ci/probe.cpp').write_text('int main() { return 1; }\n')
            self.assertIn(' M tools/ci/probe.cpp',git('status','--porcelain=v1',cwd=checkout).stdout)


    def test_probe_routes_require_original_capability(self):
        """纯元数据样本验证路由；正式执行另逐SHA核原银行与真实Installed目录。"""
        root=Path(__file__).resolve().parents[2]
        manifest=json.loads((root/'tools/ci/native_bank_risk_inputs.json').read_text())
        # 原生 build 的稀疏树不取验证银行；这里不把样本当原档成功。
        catalog={'data':{'scenarios':{risk.SCENARIO:{}}}}
        direct=risk.probe_inputs(manifest,catalog,risk.DIRECT_PROBE)
        self.assertIsNone(direct['player_plan_profile']);self.assertEqual(direct['argv_suffix'],['direct'])
        self.assertEqual(direct['expected_scope'],'ORIGINAL_NAVAL3_DIRECT_ASSET_COLD_TRANSACTION_ONLY')
        with self.assertRaisesRegex(ValueError,'no PlayerPlan capability'):
            risk.probe_inputs(manifest,catalog,risk.PROBE)
        wrong=dict(manifest,scenario_id='invented')
        with self.assertRaises(ValueError):risk.probe_inputs(wrong,catalog,risk.DIRECT_PROBE)
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'request.json';p.write_text(json.dumps(dict(self.req,probe_id=risk.DIRECT_PROBE)))
            self.assertEqual(risk.request(p)['probe_id'],risk.DIRECT_PROBE)
        # 仅纯元数据样本覆盖另一合法路由；不生成或修改原银行/Installed文件。
        selected=copy.deepcopy(catalog)
        selected['data']['scenarios'][risk.SCENARIO]['player_plan_profile']='known_life_v1'
        route=risk.probe_inputs(manifest,selected,risk.PROBE)
        self.assertEqual(route['argv_suffix'],[])
        self.assertNotEqual(route['expected_scope'],direct['expected_scope'])


if __name__=='__main__':unittest.main()
