"""用具名假Docker验证外层真实终止/缓存许可；不编译或启动实际容器。"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT=Path(__file__).with_name('supervise.sh')
LOCK=json.loads((SCRIPT.parents[3]/'native/dependencies.lock.json').read_text())
IMAGE=LOCK['toolchain']['gcc']['container_image']['reference']
DOCKER=r'''#!/usr/bin/env python3
import json,os,pathlib,sys
args=sys.argv[1:];state=pathlib.Path(os.environ['FAKE_STATE']);case=os.environ['FAKE_CASE']
log=state.with_suffix('.log')
with log.open('a') as f:f.write(args[0]+'\n')
if args[0]=='run':
 state.with_suffix('.argv.json').write_text(json.dumps(args))
 cid=pathlib.Path(args[args.index('--cidfile')+1]);cid.write_text(('d' if case=='foreign' else 'c')*64)
 (cid.parent/'native-only-receipt.json').write_text('{"status":"BUILD_RUNNING","original":true}\n')
 if case!='bootstrap-failed':
  (cid.parent/'locked-dependencies-ready.json').write_text(json.dumps({'status':'VERIFIED','lock_sha256':os.environ['FAKE_LOCK_SHA']}))
 running=case in ('timeout','unclear','foreign')
 state.write_text(json.dumps([{'Id':'c'*64,'State':{'Running':running,'Status':'running' if running else 'exited','ExitCode':0}}]));print('original product command ran');sys.exit(0)
if args[0]=='inspect':
 if not state.exists():sys.exit(1)
 print(state.read_text());sys.exit(0)
if args[0] in ('stop','kill'):
 if case=='unclear':sys.exit(1)
 rows=json.loads(state.read_text());rows[0]['State']={'Running':False,'Status':'exited','ExitCode':143};state.write_text(json.dumps(rows));sys.exit(0)
if args[0]=='rm':state.unlink();sys.exit(0)
if args[0]=='ps':
 if state.exists():print('c'*64)
 sys.exit(0)
sys.exit(1)
'''
TIMEOUT=r'''#!/usr/bin/env python3
import os,subprocess,sys
result=subprocess.run(sys.argv[4:])
sys.exit(124 if os.environ['FAKE_CASE'] in ('timeout','unclear','foreign') else result.returncode)
'''


class NativeSupervision(unittest.TestCase):
    """缓存许可只能来自同ID容器停止证据，不能把timeout包装成原生成功。"""
    def invoke(self,case,source_sha='a'*40):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=Path(temp.name)
        binary=root/'bin';binary.mkdir()
        for name,body in [('docker',DOCKER),('timeout',TIMEOUT)]:
            p=binary/name;p.write_text(body);p.chmod(0o755)
        ident=root/'identity.json';ident.write_text(json.dumps({'workflow_source_commit':'b'*40,'git_blob':'c'*40,
            'sha256':hashlib.sha256(SCRIPT.read_bytes()).hexdigest()}))
        source=root/'source/native';source.mkdir(parents=True);(source/'dependencies.lock.json').write_text(json.dumps(LOCK))
        out=root/'out';env=dict(os.environ,PATH=str(binary)+os.pathsep+os.environ['PATH'],GITHUB_RUN_ID='17',GITHUB_RUN_ATTEMPT='2',
            GITHUB_OUTPUT=str(root/'outputs'),HANMO_NATIVE_ONLY_SOFT_SECONDS='5',FAKE_LOCK_SHA=hashlib.sha256(json.dumps(LOCK).encode()).hexdigest(),FAKE_STATE=str(root/'state.json'),FAKE_CASE=case)
        result=subprocess.run(['bash',str(SCRIPT),str(root/'source'),str(root/'cache'),str(out),source_sha,str(ident),str(root/'terrain'),IMAGE,'14.2.0-19'],env=env,capture_output=True,text=True)
        self.assertTrue((out/'build-supervision.json').exists(),result.stderr)
        record=json.loads((out/'build-supervision.json').read_text())
        original=b'{"status":"BUILD_RUNNING","original":true}\n'
        self.assertEqual((out/'native-only-receipt.json').read_bytes(),original)
        self.assertEqual(record['native_receipt_sha256'],hashlib.sha256(original).hexdigest())
        return result,record,(root/'outputs').read_text(),root

    def test_completed_container_safe_to_preserve(self):
        result,record,outputs,root=self.invoke('success')
        self.assertEqual(result.returncode,0);self.assertTrue(record['cache_save_allowed'])
        self.assertFalse(record['timed_out']);self.assertFalse(record['native_receipt_rewritten'])
        self.assertIn('cache_save_allowed=true',outputs);self.assertFalse((root/'state.json').exists())

    def test_actual_docker_argv_binds_product_sha_not_supervisor_head(self):
        # 假Docker只记录真实监督器发出的argv；工具头与产品头刻意不同。
        source='e'*40
        result,record,outputs,root=self.invoke('success',source)
        self.assertEqual(result.returncode,0)
        args=json.loads((root/'state.argv.json').read_text())
        environments=[args[i+1] for i,argument in enumerate(args[:-1]) if argument=='-e']
        self.assertEqual(environments,['SOURCE_SHA='+source,'HANMO_TOOLCHAIN_IMAGE='+IMAGE,'HANMO_GCC_VERSION=14.2.0-19'])
        mounts=[args[i+1] for i,argument in enumerate(args[:-1]) if argument=='-v']
        self.assertIn(str(root/'source')+':/src:ro',mounts)
        self.assertEqual(args[-3:],[IMAGE,'bash','/src/tools/ci/native_only/build.sh'])
        self.assertEqual(args[args.index('--platform')+1],'linux/amd64')
        self.assertEqual(record['source_commit'],source)
        self.assertEqual(record['supervisor_source']['workflow_source_commit'],'b'*40)
        self.assertNotEqual(source,record['supervisor_source']['workflow_source_commit'])

    def test_soft_timeout_stops_exact_container_before_cache_signal(self):
        result,record,outputs,root=self.invoke('timeout')
        self.assertEqual(result.returncode,124);self.assertTrue(record['timed_out']);self.assertTrue(record['container_quiescent'])
        self.assertIn('cache_save_allowed=true',outputs)
        calls=(root/'state.log').read_text().splitlines();self.assertLess(calls.index('stop'),calls.index('rm'))

    def test_unconfirmed_stop_forbids_cache(self):
        result,record,outputs,root=self.invoke('unclear')
        self.assertEqual(result.returncode,125);self.assertFalse(record['container_quiescent']);self.assertFalse(record['cache_save_allowed'])
        self.assertIn('cache_save_allowed=false',outputs);self.assertTrue((root/'state.json').exists())
        self.assertEqual(record['error'],'NATIVE_CONTAINER_STOP_UNCONFIRMED')

    def test_wrong_container_id_never_stopped_or_cached(self):
        result,record,outputs,root=self.invoke('foreign')
        self.assertEqual(result.returncode,125);self.assertFalse(record['cache_save_allowed'])
        calls=(root/'state.log').read_text().splitlines();self.assertNotIn('stop',calls);self.assertNotIn('kill',calls);self.assertNotIn('rm',calls)


    def test_interrupted_bootstrap_never_saves_partial_downloads(self):
        """容器虽停稳，但依赖未核完时不能污染不可变缓存key。"""
        result,record,outputs,root=self.invoke('bootstrap-failed')
        self.assertEqual(result.returncode,0)
        self.assertTrue(record['container_quiescent'])
        self.assertFalse(record['cache_save_allowed'])
        self.assertIn('container_quiescent=true',outputs)
        self.assertIn('cache_save_allowed=false',outputs)


if __name__=='__main__':unittest.main()
