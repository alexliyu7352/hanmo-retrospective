"""首店CI必要来源门：固定请求、缓存许可与原输入均须同代，未运行游戏。"""
import copy
import importlib.util
import json
import os
import subprocess
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('private_shop_risk_under_test',HERE/'private_shop_risk.py')
risk=importlib.util.module_from_spec(spec);spec.loader.exec_module(risk)


class PrivateShopRisk(unittest.TestCase):
    """反例保持错源、重复、能力缺失和原件替换硬拒。"""
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.key='ubuntu2004-native-'+'a'*64+'-'+risk.SOURCE

    def write(self,name,value):
        path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(value));return path

    def original_inputs(self):
        catalog={'ok':True,'pack_set_digest':'b'*64,'data':{'scenarios':{risk.SCENARIO:{}}}}
        request={'scenario_id':risk.SCENARIO,'founder_selection':risk.PLAYER,
                 'expected_catalog_identity':{'pack_set_digest':'b'*64,
                  'module_versions':[{'name':'private_shop_daily_rent','version':'1'}]}}
        return catalog,request

    def test_only_fixed_source_run_and_cache_key(self):
        actual=risk.request(self.key)
        self.assertEqual(actual['source_commit'],risk.SOURCE);self.assertEqual(actual['native_run_id'],38224828405)
        self.assertEqual(risk.read_request(self.write('request.json',actual)),actual)
        for bad in ['',self.key+'\n',self.key.replace(risk.SOURCE,'0'*40),'../'+self.key]:
            with self.subTest(bad=bad),self.assertRaises(ValueError):risk.request(bad)

    def test_request_extra_or_changed_scope_refused(self):
        for key,value in [('source_commit','0'*40),('repository','someone/other'),('scope','GUI'),('extra','x')]:
            bad=risk.request(self.key);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):risk.read_request(self.write('request.json',bad))

    def test_actual_exported_profile_bound(self):
        catalog,request=self.original_inputs();self.write('inputs/catalog.json',catalog);self.write('inputs/request.json',request)
        hashes=risk.exported_inputs(self.root/'inputs');self.assertEqual(set(hashes),{'catalog.json','request.json'})
        self.assertEqual(hashes['catalog.json'],risk.sha(self.root/'inputs/catalog.json'))

    def test_missing_or_duplicate_daily_rent_refused(self):
        catalog,request=self.original_inputs();self.write('inputs/catalog.json',catalog)
        for modules in [[],[{'name':'private_shop_daily_rent','version':'2'}],request['expected_catalog_identity']['module_versions']*2]:
            request['expected_catalog_identity']['module_versions']=modules;self.write('inputs/request.json',request)
            with self.subTest(modules=modules),self.assertRaises(ValueError):risk.exported_inputs(self.root/'inputs')

    def test_founder_scenario_and_catalog_identity_not_rewritten(self):
        for change in ['founder','scenario','digest','catalog']:
            catalog,request=self.original_inputs()
            if change=='founder':request['founder_selection']='other'
            if change=='scenario':request['scenario_id']='other'
            if change=='digest':request['expected_catalog_identity']['pack_set_digest']='c'*64
            if change=='catalog':catalog['data']['scenarios']={}
            self.write('inputs/catalog.json',catalog);self.write('inputs/request.json',request)
            with self.subTest(change=change),self.assertRaises(ValueError):risk.exported_inputs(self.root/'inputs')

    def runtime_fixture(self):
        specs={name:{'files':{rel:letter*64}} for name,rel,letter in [('flecs','LICENSE','1'),('json','LICENSE.MIT','2'),('picosha2','LICENSE','3'),('godot-cpp','LICENSE.md','4')]}
        lock={'libraries':{name:specs[name] for name in ('flecs','json','picosha2')},
              'godot_bridge':{'binding':specs['godot-cpp'],'sdk':{'engine_sha256':'e'*64}}}
        self.write('native/dependencies.lock.json',lock)
        receipt={'engine':{'sha256':'e'*64},'dependencies':{name:{'root':'/cache/deps/'+name,'files':spec['files']} for name,spec in specs.items()}}
        values={'/cache/Godot_v4.7.2-stable_linux.x86_64':'e'*64}
        for name,spec in specs.items():
            for path,value in spec['files'].items():values['/cache/deps/'+name+'/'+path]=value
        return receipt,values

    def test_original_cache_five_runtime_inputs(self):
        receipt,values=self.runtime_fixture();before=copy.deepcopy(receipt)
        with patch.object(risk,'sha',side_effect=lambda p:values[str(p)]):
            result=risk.cached_runtime(self.root,Path('/cache'),receipt)
        self.assertEqual(len(result),5);self.assertEqual(receipt,before)

    def test_cache_wrong_root_receipt_and_actual_license_refused(self):
        receipt,values=self.runtime_fixture()
        with self.assertRaises(ValueError):risk.cached_runtime(self.root,Path('/new-cache'),receipt)
        for changed in ['receipt-root','receipt-lock','license','engine']:
            bad=copy.deepcopy(receipt);data=dict(values)
            if changed=='receipt-root':bad['dependencies']['json']['root']='/elsewhere'
            if changed=='receipt-lock':bad['dependencies']['json']['files']['LICENSE.MIT']='f'*64
            if changed=='license':data['/cache/deps/json/LICENSE.MIT']='f'*64
            if changed=='engine':data['/cache/Godot_v4.7.2-stable_linux.x86_64']='f'*64
            with self.subTest(changed=changed),patch.object(risk,'sha',side_effect=lambda p:data[str(p)]),self.assertRaises(ValueError):
                risk.cached_runtime(self.root,Path('/cache'),bad)

    def test_result_requires_real_daily_rent_and_no_gui_claim(self):
        good={'ok':True,'real_world':True,'daily_rent_tested':True,'gui_tested':False,'same_player':risk.PLAYER,'checks':10}
        risk.validate_result(good)
        for key,value in [('ok',False),('real_world',False),('daily_rent_tested',False),('gui_tested',True),('same_player','other'),('checks',True)]:
            bad=dict(good);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):risk.validate_result(bad)

    def test_hash_rejects_symlink(self):
        target=self.write('actual.json',{'a':1});link=self.root/'alias.json';link.symlink_to(target)
        with self.assertRaises(ValueError):risk.sha(link)

    def test_first_preparation_exception_retained(self):
        req=self.write('request.json',risk.request(self.key))
        args=SimpleNamespace(command='prepare',request=req,source=self.root/'missing-source',operator=self.root/'operator',
                             artifacts=self.root/'artifacts',output=self.root/'output',cache=Path('/cache'))
        self.assertEqual(risk.execute(args),1)
        receipt=json.loads((self.root/'output/receipt.json').read_text())
        self.assertEqual(receipt['status'],'FAILED');self.assertEqual(receipt['stages'],[])
        self.assertIn('error',receipt);self.assertIn('completed_at',receipt)
        self.assertEqual(receipt['postflight'],'NOT_REACHED')

    def test_original_selection_preserves_dispatch_identity_and_wrong_run_rejection(self):
        helper=risk.load(HERE/'native_bank_risk.py','shop_test_original_selector')
        req=risk.request(self.key);head='a'*40
        run={'id':risk.RUN,'repository':{'id':17,'full_name':risk.REPOSITORY},
             'head_repository':{'id':17,'full_name':risk.REPOSITORY},'head_sha':head,
             'path':'.github/workflows/native-ubuntu2004.yml','status':'completed','conclusion':'success','event':'workflow_dispatch'}
        artifact={'id':99,'name':'native-ubuntu2004-'+risk.SOURCE,'expired':False,'size_in_bytes':123,
                  'workflow_run':{'id':risk.RUN,'repository_id':17,'head_repository_id':17,'head_sha':head}}
        entries={'total_count':1,'artifacts':[artifact]}
        self.assertEqual(helper.select(req,run,entries)['sha'],risk.SOURCE)
        for change in ['wrong-run','wrong-artifact-run','push-wrong-head','expired','duplicate']:
            bad=copy.deepcopy(run);art=copy.deepcopy(entries)
            if change=='wrong-run':bad['id']+=1
            if change=='wrong-artifact-run':art['artifacts'][0]['workflow_run']['id']+=1
            if change=='push-wrong-head':bad['event']='push'
            if change=='expired':art['artifacts'][0]['expired']=True
            if change=='duplicate':art['total_count']=2;art['artifacts']*=2
            with self.subTest(change=change),self.assertRaises(ValueError):helper.select(req,bad,art)


    def test_sparse_nested_metadata_is_required_and_status_stays_offline(self):
        """真实partial clone复现漏祖ignore懒取；补齐后离线仍能判净源、脏源和假元数据。"""
        origin=self.root/'origin';origin.mkdir();checkout=self.root/'checkout'
        env={key:value for key,value in os.environ.items() if not key.startswith('GIT_')}
        env.update(HOME=str(self.root),GIT_CONFIG_NOSYSTEM='1',GIT_ALLOW_PROTOCOL='file')
        def run(*args,cwd=origin):
            value=subprocess.run(['git',*map(str,args)],cwd=cwd,env=env,text=True,
                                 stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            self.assertEqual(value.returncode,0,value.stdout);return value.stdout.strip()
        run('init','-q')
        (origin/'.gitignore').write_text('output/\n')
        (origin/'.gitattributes').write_text('verification/** -whitespace\n')
        directory=origin/'game/character_studio';(directory/'src').mkdir(parents=True)
        (directory/'.gitignore').write_text('.godot/\n*.uid\n')
        (directory/'.gitattributes').write_text('*.gd text eol=lf\n')
        relative='game/character_studio/src/studio_session.gd'
        (origin/relative).write_text('extends RefCounted\n')
        run('add','.');run('-c','user.name=Fixture','-c','user.email=fixture@local.invalid','commit','-qm','fixture')
        run('config','uploadpack.allowFilter','true');run('config','uploadpack.allowReachableSHA1InWant','true')
        run('clone','--filter=blob:none','--no-checkout',origin.as_uri(),checkout)
        run('config','core.sparseCheckout','true',cwd=checkout)
        (checkout/'.git/info/sparse-checkout').write_text('/.gitattributes\n/.gitignore\n/'+relative+'\n')
        run('checkout','-q','HEAD',cwd=checkout)
        with patch.dict(os.environ,env,clear=True):
            with self.assertRaisesRegex(ValueError,'sparse Git metadata missing'):risk.git_metadata(checkout)
            # 远端仍存在且具备可读权限，身份读取也不能偷偷懒取缺blob。
            with self.assertRaises(subprocess.CalledProcessError):risk.git(checkout,'status','--porcelain=v1')
        run('sparse-checkout','set','/.gitattributes','/.gitignore','/**/.gitattributes','/**/.gitignore','/'+relative,cwd=checkout)
        run('remote','set-url','origin',(self.root/'unavailable').as_uri(),cwd=checkout)
        with patch.dict(os.environ,env,clear=True):
            rows=risk.git_metadata(checkout);self.assertEqual(len(rows),4)
            self.assertEqual(risk.git(checkout,'status','--porcelain=v1'),'')
            (checkout/relative).write_text('extends Node\n')
            self.assertIn('M '+relative,risk.git(checkout,'status','--porcelain=v1'))
            (checkout/'game/character_studio/.gitignore').write_text('different\n')
            with self.assertRaisesRegex(ValueError,'sparse Git metadata changed'):risk.git_metadata(checkout)

    def test_source_identity_still_rejects_wrong_head_dirty_source_and_probe(self):
        """补忽略元数据不减少原固定头、两树净源、导出器与probe逐字检查。"""
        product=self.root/'product';operator=self.root/'operator'
        def git_value(root,*args):
            if args==('rev-parse','HEAD'):return risk.SOURCE if root==product else 'a'*40
            return ''
        hashes={operator/risk.EXPORTER:risk.EXPORTER_SHA}
        hashes.update({product/path:value for path,value in risk.PROBES.items()})
        hashes.update({operator/path:value for path,value in risk.TOOL_PROBES.items()})
        with patch.object(risk,'git_metadata',return_value=[]):
            for bad in ['none','head','product-dirty','operator-dirty','exporter','probe','tool-probe','tool-helper']:
                def changed(root,*args):
                    if bad=='head' and root==product and args==('rev-parse','HEAD'):return '0'*40
                    if args==('status','--porcelain=v1') and ((bad=='product-dirty' and root==product) or (bad=='operator-dirty' and root==operator)):return ' M tracked'
                    return git_value(root,*args)
                values=dict(hashes)
                if bad=='exporter':values[operator/risk.EXPORTER]='0'*64
                if bad=='probe':values[product/next(iter(risk.PROBES))]='0'*64
                if bad=='tool-probe':values[operator/next(iter(risk.TOOL_PROBES))]='0'*64
                if bad=='tool-helper':values[operator/list(risk.TOOL_PROBES)[1]]='0'*64
                with self.subTest(bad=bad),patch.object(risk,'git',side_effect=changed),patch.object(risk,'sha',side_effect=values.__getitem__):
                    if bad=='none':self.assertEqual(risk.source_identity(product,operator)['product_source'],risk.SOURCE)
                    else:
                        with self.assertRaises(ValueError):risk.source_identity(product,operator)

    def test_git_protocol_policy_is_process_local(self):
        """只约束身份子进程，不修改checkout、作者或宿主的网络策略。"""
        with patch.dict(os.environ,{'GIT_ALLOW_PROTOCOL':'https'}):
            with patch.object(risk.subprocess,'check_output',return_value='value\n') as command:
                self.assertEqual(risk.git(self.root,'rev-parse','HEAD'),'value')
                self.assertEqual(command.call_args.kwargs['env']['GIT_ALLOW_PROTOCOL'],'')
                self.assertEqual(command.call_args.kwargs['env']['GIT_TERMINAL_PROMPT'],'0')
            self.assertEqual(os.environ['GIT_ALLOW_PROTOCOL'],'https')

    def test_completed_trade_exits_only_through_original_client_button(self):
        source=(HERE.parents[1]/next(iter(risk.TOOL_PROBES))).read_text()
        leave=source[source.index('void leave('):source.index('void talk(')]
        self.assertIn('trade_completed',leave)
        self.assertIn('"action_id").empty()',leave)
        self.assertIn('quote(w,"end_interaction"',leave)
        self.assertIn('accept(w,"end_interaction")',leave)
        self.assertIn('"phase")=="scene"',leave)
        self.assertNotIn('"advance"',leave)
        self.assertNotIn('"close_trade"',leave)
        self.assertIn('"dialogue_confirm"',source)

    def test_original_typed_order_quote_is_exact_not_generic_success(self):
        source=(HERE.parents[1]/next(iter(risk.TOOL_PROBES))).read_text()
        block=source[source.index('void check_order_quote('):source.index('// 只沿原Action推进')]
        for key in ['duration','order_id','task_id','item_id','quantity','contract_price','deadline_tick','revision']:
            self.assertIn('"'+key+'"',block)
        self.assertIn('seen==fields',block)
        self.assertIn('text(params,"order_id")',block)
        self.assertIn('text(params,"task_id")',block)
        self.assertIn('number(terms,name)',block)
        self.assertIn('accepting?0:1',block)
        self.assertIn('else check(boundary::get_bool(q,"ok","quote")',block)
        self.assertIn('before==need(w.checkpoint(),"after quote")',block)

    def test_protocol4_is_tool_side_and_original_product_identity_is_preserved(self):
        """新probe须独立具名且唯一变化，旧产品两源和正式库不借后继替换。"""
        self.assertEqual(risk.request(self.key)['probe_id'],'first-shop-daily-rent-protocol4')
        first,helper=risk.PROBES
        self.assertEqual(risk.PROBES[first],'5811b374af6010a9702f5e55ffe2fbd83ae52112d600a295842bd85da45a7fea')
        self.assertNotEqual(risk.TOOL_PROBES[first],risk.PROBES[first])
        self.assertEqual(risk.TOOL_PROBES[helper],risk.PROBES[helper])
        for path,expected in risk.TOOL_PROBES.items():self.assertEqual(risk.sha(HERE.parents[1]/path),expected)
        text=(HERE/'private_shop_risk.py').read_text()
        self.assertIn("operator/next(iter(TOOL_PROBES))",text)
        self.assertIn("'-I'+str(source/'native/world/src')",text)
        self.assertIn("artifacts/'libhanmo_resident.a'",text)

    def test_workflow_no_new_native_build_or_download_fallback(self):
        text=(HERE.parents[1]/'.github/workflows/private-shop-risk.yml').read_text()
        self.assertIn('fail-on-cache-miss: true',text)
        self.assertNotIn('restore-keys:',text);self.assertNotIn('release download',text)
        self.assertNotIn('bootstrap_locked_native',text);self.assertNotIn('cmake --build',text)
        self.assertIn('ubuntu:22.04',text);self.assertIn('ubuntu:20.04',text)
        self.assertIn('/content/packs/authored/private_shops_1_1/**',text)
        self.assertIn('/tools/private_shop_export_inputs.gd',text)
        self.assertNotIn('push:',text)
        self.assertEqual(text.count('            /**/.gitignore\n'),2)
        self.assertEqual(text.count('            /**/.gitattributes\n'),2)
        self.assertIn('private_shop_risk.py identity --source producer-source --operator tool-source',text)
        self.assertNotIn('GIT_ALLOW_PROTOCOL',text)


class ReservationOwnerProducerRisk(unittest.TestCase):
    """新配方只能复用真实指定生产者，旧请求及产物不能偷偷升级来源。"""
    def setUp(self):
        risk.activate_profile('reservation-owner-fix')
        self.addCleanup(risk.activate_profile,'original-protocol4')
        self.key='ubuntu2004-native-'+'a'*64+'-'+risk.SOURCE+'-run-38240212288-attempt-1'

    def test_exact_new_source_run_attempt_and_unchanged_probe(self):
        value=risk.request(self.key)
        self.assertEqual(value['source_commit'],'67cc70a2b126a68c6ffe4ca3f4bb73f80c53ec80')
        self.assertEqual(value['native_run_id'],38240212288)
        self.assertEqual(value['probe_id'],'first-shop-reservation-owner-fix')
        self.assertEqual(risk.PROBES,risk.TOOL_PROBES)
        self.assertEqual(value['scope'],'ORIGINAL_FIRST_SHOP_DAILY_RENT_ASSET_COLD_TRANSACTION_ONLY')
        for bad in [self.key.replace('38240212288','38238968502'),self.key.replace('attempt-1','attempt-0'),
                    self.key.replace('attempt-1','attempt-01'),self.key+'\n',self.key.rsplit('-run',1)[0],
                    self.key.replace(risk.SOURCE,risk.PROFILES['original-protocol4']['source'])]:
            with self.subTest(key=bad),self.assertRaises(ValueError):risk.request(bad)

    def test_request_cannot_select_unknown_or_wrong_recipe(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'request.json';value=risk.request(self.key);p.write_text(json.dumps(value))
            self.assertEqual(risk.read_request(p),value)
            for key,wrong in [('source_commit','0'*40),('native_run_id',38224828405),('probe_id','first-shop-daily-rent-protocol4'),('repository','other/repo'),('extra',True)]:
                bad=dict(value);bad[key]=wrong;p.write_text(json.dumps(bad))
                with self.subTest(key=key),self.assertRaises(ValueError):risk.read_request(p)
            p.write_text(json.dumps(value));risk.activate_profile('original-protocol4')
            with self.assertRaises(ValueError):risk.read_request(p)
            risk.activate_profile('reservation-owner-fix')
        with self.assertRaises(ValueError):risk.activate_profile('arbitrary-source')

    def test_new_selection_still_requires_success_and_exact_artifact(self):
        helper=risk.load(HERE/'native_bank_risk.py','reservation_fix_selector')
        req=risk.request(self.key);head='a'*40
        run={'id':risk.RUN,'repository':{'id':17,'full_name':risk.REPOSITORY},
             'head_repository':{'id':17,'full_name':risk.REPOSITORY},'head_sha':head,
             'path':'.github/workflows/native-ubuntu2004.yml','status':'completed','conclusion':'success','event':'workflow_dispatch'}
        artifact={'id':99,'name':'native-ubuntu2004-'+risk.SOURCE,'expired':False,'size_in_bytes':123,
                  'workflow_run':{'id':risk.RUN,'repository_id':17,'head_repository_id':17,'head_sha':head}}
        entries={'total_count':1,'artifacts':[artifact]}
        self.assertEqual(helper.select(req,run,entries)['sha'],risk.SOURCE)
        for change in ['running','failed','old-run','old-source','expired','duplicate']:
            bad=copy.deepcopy(run);art=copy.deepcopy(entries)
            if change=='running':bad['status']='in_progress'
            if change=='failed':bad['conclusion']='failure'
            if change=='old-run':bad['id']=38224828405
            if change=='old-source':art['artifacts'][0]['name']='native-ubuntu2004-'+risk.PROFILES['original-protocol4']['source']
            if change=='expired':art['artifacts'][0]['expired']=True
            if change=='duplicate':art['total_count']=2;art['artifacts']*=2
            with self.subTest(change=change),self.assertRaises(ValueError):helper.select(req,bad,art)

    def test_workflow_passes_same_named_profile_through_all_stages(self):
        text=(HERE.parents[1]/'.github/workflows/private-shop-risk.yml').read_text()
        self.assertIn('          - reservation-owner-fix',text)
        self.assertEqual(text.count('-e SHOP_PROFILE'),2)
        self.assertEqual(text.count('--profile "$SHOP_PROFILE"'),5)
        self.assertIn('SHOP_PROFILE: ${{ inputs.probe_profile }}',text)
        self.assertNotIn('cmake --build',text)
        self.assertNotIn('restore-keys:',text)


class SettlementPhaseProducerRisk(unittest.TestCase):
    """原真实成功c7fd只复用原protocol4，来源、缓存及两旧配方继续严格分开。"""
    def setUp(self):
        risk.activate_profile('settlement-phase-fix')
        self.addCleanup(risk.activate_profile,'original-protocol4')
        self.key=('ubuntu2004-native-6fe46581bd0a29c01048f2a57596a724093144057e3acac34c3c9d22e85449eb-'
                  'c7fd158d68b58abea84790098e6549cd0b8add0d-run-38250753551-attempt-1')

    def test_actual_producer_key_and_unchanged_execution_probe(self):
        value=risk.request(self.key)
        self.assertEqual(value['source_commit'],'c7fd158d68b58abea84790098e6549cd0b8add0d')
        self.assertEqual(value['native_run_id'],38250753551)
        self.assertEqual(value['probe_id'],'first-shop-settlement-phase-fix')
        self.assertEqual(risk.PROBES,risk.TOOL_PROBES)
        first=next(iter(risk.PROBES))
        self.assertEqual(risk.PROBES[first],'222ddafc0f06866d494978b76ff22c82af27bf3a72990edec3ca74334c5cf5c6')
        self.assertEqual(value['scope'],'ORIGINAL_FIRST_SHOP_DAILY_RENT_ASSET_COLD_TRANSACTION_ONLY')
        for path,expected in risk.PROBES.items():self.assertEqual(risk.sha(HERE.parents[1]/path),expected)
        for bad in [self.key.replace('38250753551','38240212288'),self.key.replace('attempt-1','attempt-0'),
                    self.key.replace('attempt-1','attempt-01'),self.key+'\n',self.key.rsplit('-run',1)[0],
                    self.key.replace(risk.SOURCE,risk.PROFILES['reservation-owner-fix']['source'])]:
            with self.subTest(key=bad),self.assertRaises(ValueError):risk.request(bad)

    def test_each_saved_profile_remains_distinct(self):
        req=risk.request(self.key)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'request.json';path.write_text(json.dumps(req))
            self.assertEqual(risk.read_request(path),req)
            for name in ['original-protocol4','reservation-owner-fix']:
                risk.activate_profile(name)
                with self.subTest(profile=name),self.assertRaises(ValueError):risk.read_request(path)
            risk.activate_profile('settlement-phase-fix')
            for key,wrong in [('source_commit','0'*40),('native_run_id',38240212288),
                              ('probe_id','first-shop-reservation-owner-fix'),('scope','ALL_WORLD'),('extra',True)]:
                bad=dict(req);bad[key]=wrong;path.write_text(json.dumps(bad))
                with self.subTest(field=key),self.assertRaises(ValueError):risk.read_request(path)
        self.assertEqual(risk.PROFILES['original-protocol4']['run'],38224828405)
        self.assertEqual(risk.PROFILES['reservation-owner-fix']['run'],38240212288)
        self.assertFalse(risk.PROFILES['original-protocol4']['cache_attempt'])
        with self.assertRaises(ValueError):risk.activate_profile('latest-success')

    def test_selected_artifact_requires_success_and_original_run_association(self):
        helper=risk.load(HERE/'native_bank_risk.py','settlement_fix_selector')
        req=risk.request(self.key);head='a'*40
        run={'id':risk.RUN,'repository':{'id':17,'full_name':risk.REPOSITORY},
             'head_repository':{'id':17,'full_name':risk.REPOSITORY},'head_sha':head,
             'path':'.github/workflows/native-ubuntu2004.yml','status':'completed','conclusion':'success','event':'workflow_dispatch'}
        artifact={'id':11751051516,'name':'native-ubuntu2004-'+risk.SOURCE,'expired':False,'size_in_bytes':108305012,
                  'workflow_run':{'id':risk.RUN,'repository_id':17,'head_repository_id':17,'head_sha':head}}
        entries={'total_count':1,'artifacts':[artifact]}
        self.assertEqual(helper.select(req,run,entries)['sha'],risk.SOURCE)
        for change in ['running','failed','old-run','old-source','wrong-association','expired','duplicate']:
            bad=copy.deepcopy(run);art=copy.deepcopy(entries)
            if change=='running':bad['status']='in_progress'
            if change=='failed':bad['conclusion']='failure'
            if change=='old-run':bad['id']=38240212288
            if change=='old-source':art['artifacts'][0]['name']='native-ubuntu2004-'+risk.PROFILES['reservation-owner-fix']['source']
            if change=='wrong-association':art['artifacts'][0]['workflow_run']['id']=38240212288
            if change=='expired':art['artifacts'][0]['expired']=True
            if change=='duplicate':art['total_count']=2;art['artifacts']*=2
            with self.subTest(change=change),self.assertRaises(ValueError):helper.select(req,bad,art)

    def test_workflow_selects_named_profile_without_broadening_runner(self):
        text=(HERE.parents[1]/'.github/workflows/private-shop-risk.yml').read_text()
        self.assertIn('default: settlement-phase-fix',text)
        for name in risk.PROFILES:self.assertIn('          - '+name+'\n',text)
        self.assertEqual(text.count('--profile "$SHOP_PROFILE"'),5)
        self.assertNotIn('restore-keys:',text)
        self.assertNotIn('cmake --build',text)


if __name__=='__main__':unittest.main()
