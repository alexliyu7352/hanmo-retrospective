"""微小假文件只验独立回执检查器；没有运行游戏或原生构建。"""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('output_check',Path(__file__).with_name('verify_native_outputs.py'))
check=importlib.util.module_from_spec(spec);spec.loader.exec_module(check)

class NativeOutputVerificationTests(unittest.TestCase):
    """完整五件、真实状态与锁来源必须同时匹配，不能由旧两件回执冒充。"""
    def fixture(self,folder):
        sha='c'*40
        bridge={'build_identity':'fixture-not-a-native-library'}
        lock={'libraries':{'json':{'files':{'a':'123'}}},'godot_bridge':{'binding':{'files':{'b':'456'}},'sdk':{'engine_sha256':'789'}}}
        for name in check.ARTIFACTS:(folder/name).write_bytes(b'fixture-only:'+name.encode())
        for name,value in [('resident_bridge_identity.json',bridge),('dependencies.lock.json',lock)]:
            (folder/name).write_text(json.dumps(value))
        (folder/'locked.cmake').write_text('# fixture only\n');(folder/'source-commit.txt').write_text(sha+'\n')
        def rows(names):return [{'path':'/old-container/'+name,'sha256':hashlib.sha256((folder/name).read_bytes()).hexdigest(),'bytes':(folder/name).stat().st_size} for name in sorted(names)]
        receipt={'format':'HANMO-RECOVERY-NATIVE-BUILD/1','status':'BUILD_COMPLETE_RUNTIME_NOT_RUN',
                 'source_commit':sha,'source_commit_after':sha,'requested_source_commit':sha,
                 'source_status_after':'','dependencies_unchanged_during_build':True,
                 'stages':[{'name':n,'exit_code':0} for n in ['configure','build-hanmo_resident_journey','build-hanmo_world_bridge']],
                 'artifacts':rows(check.ARTIFACTS),'identity_files':rows(check.IDENTITIES),
                 'bridge_identity':bridge,'dependencies':{'json':{'files':{'a':'123'}},'godot-cpp':{'files':{'b':'456'}}},
                 'engine':{'sha256':'789'}}
        return sha,receipt

    def write(self,folder,receipt):
        (folder/'native-build-receipt.json').write_text(json.dumps(receipt))

    def test_exact_set_and_source(self):
        with tempfile.TemporaryDirectory() as d:
            folder=Path(d);sha,receipt=self.fixture(folder);self.write(folder,receipt)
            result=check.verify(folder,sha);self.assertEqual(result['artifact_count'],5);self.assertEqual(result['dependency_file_count'],2)

    def test_old_two_artifact_receipt_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            folder=Path(d);sha,receipt=self.fixture(folder);receipt['artifacts']=receipt['artifacts'][:2];self.write(folder,receipt)
            with self.assertRaises(ValueError):check.verify(folder,sha)

    def test_mutated_native_file_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            folder=Path(d);sha,receipt=self.fixture(folder);self.write(folder,receipt);(folder/'libhanmo_resident.a').write_bytes(b'changed')
            with self.assertRaises(ValueError):check.verify(folder,sha)

    def test_wrong_source_and_incomplete_stages_rejected(self):
        for change in ('source','stage','dirty','status'):
            with self.subTest(change=change),tempfile.TemporaryDirectory() as d:
                folder=Path(d);sha,receipt=self.fixture(folder)
                if change=='source':receipt['source_commit_after']='d'*40
                elif change=='stage':receipt['stages'][1]['exit_code']=1
                elif change=='dirty':receipt['source_status_after']=' M file'
                else:receipt['status']='BUILD_RUNNING'
                self.write(folder,receipt)
                with self.assertRaises(ValueError):check.verify(folder,sha)

    def test_actual_identity_and_dependency_evidence_rejected(self):
        for change in ('bridge','dependency','engine'):
            with self.subTest(change=change),tempfile.TemporaryDirectory() as d:
                folder=Path(d);sha,receipt=self.fixture(folder)
                if change=='bridge':receipt['bridge_identity']={'wrong':True}
                elif change=='dependency':receipt['dependencies']['json']['files']['a']='changed'
                else:receipt['engine']['sha256']='changed'
                self.write(folder,receipt)
                with self.assertRaises(ValueError):check.verify(folder,sha)

if __name__=='__main__':unittest.main()
