"""原生专用CI的轻量合同测试；小夹具不代表真实产品编译或玩法通过。"""
import copy
import importlib.util
import json
import os
import textwrap
from pathlib import Path
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


def load(name):
    """只导入本目录新增脚本，不执行真实构建入口。"""
    spec = importlib.util.spec_from_file_location(name, HERE / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class NativeOnlyContractTests(unittest.TestCase):
    """记录必须覆盖精确目标、原K1门、固定输入和失败，而非只检查退出码。"""

    def setUp(self):
        self.module = load('verify')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out, self.build = self.root / 'out', self.root / 'build'
        self.out.mkdir()
        self.build.mkdir()
        self.sha = 'a' * 40

    def test_production_has_no_test_fixture_factory(self):
        self.assertFalse(hasattr(self.module, 'test_fixture_receipt'))

    def test_exact_test_selection(self):
        self.module.require_k1_selection({'tests': [{'name': 'checklist_K1_1'}]})
        for names in ([], ['checklist_K1_2'], ['checklist_K1_1', 'checklist_K1_2']):
            with self.assertRaises(ValueError):
                self.module.require_k1_selection({'tests': [{'name': n} for n in names]})

    def test_junit_requires_real_single_passing_test(self):
        path = self.out / 'k1.junit.xml'
        path.write_text('<testsuite tests="1" failures="0" errors="0"><testcase name="checklist_K1_1"/></testsuite>')
        self.module.require_k1_junit(path)
        for body in ('<testsuite tests="0"/>', '<testsuite><testcase name="checklist_K1_1"><failure/></testcase></testsuite>', '<testsuite><testcase name="checklist_K1_1"><skipped/></testcase></testsuite>', '<testsuite><testcase name="checklist_K1_2"/></testsuite>'):
            path.write_text(body)
            with self.assertRaises(ValueError):
                self.module.require_k1_junit(path)

    def test_official_package_metadata_requires_exact_archive_bytes(self):
        """同名同版本仍必须有锁中的原包摘要；缺件或冲突重复记录均拒绝。"""
        row='Package: g++-14\nVersion: 14.2.0-19\nFilename: pool/compiler.deb\nSHA256: '+('a'*64)+'\n'
        expected={'path':'pool/compiler.deb','sha256':'a'*64}
        self.module.require_package_metadata(row,'g++-14','14.2.0-19',expected)
        for text in ('',row.replace('14.2.0-19','14.2.0-20'),
                     row.replace('a'*64,'b'*64),row+'\n'+row.replace('a'*64,'b'*64)):
            with self.assertRaises(ValueError):
                self.module.require_package_metadata(text,'g++-14','14.2.0-19',expected)

    def test_source_sha_must_be_immutable(self):
        runner = load('run')
        with self.assertRaises(ValueError):
            runner.require_commit('integration/native')
        with self.assertRaises(ValueError):
            runner.require_commit('a' * 39)
        runner.require_commit(self.sha)

    def test_source_inventory_detects_changed_and_missing_bytes(self):
        runner = load('run')
        repo = self.root / 'source'
        repo.mkdir()
        subprocess.run(['git', 'init', '-q', str(repo)], check=True)
        subprocess.run(['git', '-C', str(repo), 'config', 'user.name', 'CI test'], check=True)
        subprocess.run(['git', '-C', str(repo), 'config', 'user.email', 'ci-test@example.invalid'], check=True)
        file = repo / 'native' / 'sample.cpp'
        file.parent.mkdir()
        file.write_text('original source\n')
        subprocess.run(['git', '-C', str(repo), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(repo), 'commit', '-qm', 'fixture'], check=True)
        self.assertEqual(len(runner.verify_tracked_inputs(repo, ['native'])), 1)
        file.write_text('changed source\n')
        with self.assertRaises(ValueError):
            runner.verify_tracked_inputs(repo, ['native'])
        file.unlink()
        with self.assertRaises(ValueError):
            runner.verify_tracked_inputs(repo, ['native'])

    def test_only_native_target_and_k1_commands(self):
        runner = load('run')
        commands = runner.build_commands(Path('/src'), Path('/cache'), Path('/out'))
        text = json.dumps(commands, default=str)
        self.assertIn('hanmo_native_complete', text)
        self.assertIn('^checklist_K1_1$', text)
        self.assertIn('-DHANMO_BUILD_GODOT_BRIDGE=OFF', text)
        self.assertIn('-DHANMO_SOURCE_ARCHIVE=OFF', text)
        self.assertNotIn('hanmo_world_bridge', text)
        self.assertNotIn('checklist_A4', text)
        self.assertNotIn('checklist_K1_2', text)
        self.assertIn('--no-tests=error', text)

    def test_original_workflow_not_replaced(self):
        workflow = HERE.parents[2] / '.github/workflows/native-only-debian-trixie.yml'
        text = workflow.read_text()
        self.assertIn('只验原生层', text)
        self.assertIn('521c49bc88083157722261cb8dd97000d9db9d12', text)
        self.assertNotIn('continue-on-error', text)
        self.assertNotIn('native-ubuntu2004.yml', text)

    def test_only_immutable_source_is_accepted(self):
        """自动事件与手动执行都绑定完整源码SHA，不再依赖已废弃的PPA批准门。"""
        workflow = HERE.parents[2] / '.github/workflows/native-only-debian-trixie.yml'
        command = workflow.read_text().split('        run: |\n', 1)[1].split('      - name: Check out immutable supervision source', 1)[0]
        command = textwrap.dedent(command)
        cases = [('workflow_dispatch','a'*40,'b'*40,0,'a'*40),
                 ('workflow_dispatch','main','b'*40,1,None),
                 ('push','','b'*40,0,'b'*40), ('pull_request','','main',1,None)]
        for event, manual, event_sha, expected, sha in cases:
            output=self.root/'gate-output'
            output.write_text('')
            result = subprocess.run(['bash', '-e', '-c', command], capture_output=True, text=True,
                env=dict(os.environ, EVENT_NAME=event, INPUT_SOURCE=manual, EVENT_SOURCE=event_sha,
                         GITHUB_OUTPUT=str(output)))
            self.assertEqual(result.returncode, expected, result.stderr)
            if sha:
                self.assertEqual(output.read_text(),'sha='+sha+'\n')

    def test_official_image_and_package_lock_match_workflow(self):
        """映像摘要及Debian包版本同时进入workflow和原锁，不容退回浮动tag或PPA。"""
        spec=json.loads((HERE.parents[2]/'native/dependencies.lock.json').read_text())['toolchain']['gcc']
        workflow=(HERE.parents[2]/'.github/workflows/native-only-debian-trixie.yml').read_text()
        self.assertIn(spec['container_image']['reference'],workflow)
        self.assertIn(spec['package_version'],workflow)
        self.assertEqual(spec['acquisition_method'],'official_debian_packages')
        self.assertEqual(spec['package_version'],'14.2.0-19')
        self.assertRegex(spec['container_image']['reference'],r'^debian:trixie@sha256:[0-9a-f]{64}$')
        self.assertNotIn('PPA',workflow)
        self.assertNotIn('launchpad', (HERE/'build.sh').read_text())
        self.assertNotIn('ubuntu:20.04', (HERE/'supervise.sh').read_text())


class NativeOnlyReceiptTests(unittest.TestCase):
    """只用小型合同夹具测验证器的拒绝路径，绝不冒充真实产品或K1运行。"""

    def setUp(self):
        """建立独占临时目录和可修改的最小文件形状。"""
        self.module = load('verify')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out, self.build = self.root / 'out', self.root / 'build'
        self.out.mkdir()
        self.build.mkdir()
        self.sha = 'a' * 40
        (self.build / 'member.o').write_bytes(b'unit-test-contract-object')
        artifacts = []
        for name in sorted(self.module.ARTIFACTS):
            path = self.build / name
            if name.endswith('.a'):
                subprocess.run(['ar', 'rcs', str(path), str(self.build / 'member.o')], check=True)
                row = {'archive_kind': 'normal', 'member_count': 1}
            else:
                path.write_bytes(b'\x7fELFunit-test-contract-only')
                row = {}
            row.update(path=name, bytes=path.stat().st_size, sha256=self.module.digest(path))
            artifacts.append(row)
        (self.out / 'k1-selection.json').write_text(json.dumps({'tests': [{'name': 'checklist_K1_1'}]}))
        (self.out / 'k1.junit.xml').write_text('<testsuite tests="1" failures="0" errors="0"><testcase name="checklist_K1_1"/></testsuite>')
        self.toolchain=json.loads((HERE.parents[2]/'native/dependencies.lock.json').read_text())['toolchain']['gcc']
        for name in ('source-inputs.json', 'dependency-inputs.json', 'terrain-inputs.json'):
            (self.out / name).write_text('{}')
        # 此记录仅是验证器合同小夹具，不是安装、编译或真实探针证据。
        self.compiler={'image':self.toolchain['container_image']['reference'],'platform':'linux/amd64',
            'acquisition_method':'official_debian_packages','toolchain_repository':self.toolchain['repository'],
            'suite':'trixie','archive_keyring':'/usr/share/keyrings/debian-archive-keyring.gpg',
            'package_versions':dict(self.toolchain['packages']),
            'cxx20_span_bit_cast_and_libstdcxx14_probe':'PASS',
            'tools':{name:{'version':name+' (Debian 14.2.0-19) 14.2.0\n'} for name in ('gcc-14','g++-14')}}
        (self.out/'compiler.json').write_text(json.dumps(self.compiler))
        evidence = {p.name: self.module.digest(p) for p in self.out.iterdir()}
        self.receipt = {'format': 'HANMO-NATIVE-ONLY-CI/1', 'scope': self.module.SCOPE,
            'status': 'PASS', 'source_commit': self.sha, 'source_commit_after': self.sha,
            'inputs_unchanged': True, 'godot_bridge': 'NOT_RUN', 'other_checklist_rows': 'NOT_RUN',
            'stages': [{'name': name, 'exit_code': 0} for name in self.module.STAGES],
            'artifacts': artifacts, 'evidence': evidence}

    def check(self):
        """每次重写独立小夹具回执后调用真实验证器。"""
        (self.out / 'native-only-receipt.json').write_text(json.dumps(self.receipt))
        return self.module.verify(self.out, self.build, self.sha, self.toolchain)

    def test_complete_contract_shape_verifies(self):
        """控制组只说明验证器接受完整形状，不说明产品通过。"""
        self.assertEqual(self.check()['native_output_count'], 7)

    def test_failed_incomplete_or_wrong_source_receipt_rejected(self):
        """失败、旧身份与缺阶段都必须保持失败。"""
        original = copy.deepcopy(self.receipt)
        changes = [{'status': 'RUNNING'}, {'source_commit_after': 'b' * 40},
                   {'stages': []}, {'godot_bridge': 'PASS'}, {'inputs_unchanged': False}]
        for change in changes:
            self.receipt = dict(copy.deepcopy(original), **change)
            with self.assertRaises(ValueError):
                self.check()

    def test_changed_output_and_evidence_rejected(self):
        """内容变化不能只凭历史摘要行或文件名取得通过。"""
        original = (self.build / 'hanmo_sim').read_bytes()
        (self.build / 'hanmo_sim').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            self.check()
        (self.build / 'hanmo_sim').write_bytes(original)
        self.check()
        (self.out / 'compiler.json').write_text('{"changed":true}')
        with self.assertRaises(ValueError):
            self.check()

    def test_compiler_package_image_and_probe_drift_rejected(self):
        """即使证据摘要被同步更新，错误工具链身份也不能取得通过。"""
        original=copy.deepcopy(self.compiler)
        changes=[{'image':'debian:trixie'}, {'acquisition_method':'third_party'},
                 {'package_versions':{'g++-14':'14.3.0'}},
                 {'cxx20_span_bit_cast_and_libstdcxx14_probe':'NOT_RUN'},
                 {'tools':{'g++-14':{'version':'g++-14 (Debian 14.2.0-20) 14.2.0'}}}]
        for change in changes:
            (self.out/'compiler.json').write_text(json.dumps(dict(copy.deepcopy(original),**change)))
            self.receipt['evidence']['compiler.json']=self.module.digest(self.out/'compiler.json')
            with self.assertRaises(ValueError):
                self.check()

    def test_thin_archive_cannot_hide_member_inventory(self):
        """实际薄归档不得删标记或漏成员后绕过复核。"""
        path = self.build / 'libhanmo_resident.a'
        path.unlink()
        subprocess.run(['ar', 'rcsT', str(path), 'member.o'], cwd=self.build, check=True)
        row = next(r for r in self.receipt['artifacts'] if r['path'] == path.name)
        row.update(bytes=path.stat().st_size, sha256=self.module.digest(path))
        with self.assertRaises(ValueError):
            self.check()
        row['archive_kind'] = 'thin'
        with self.assertRaises(ValueError):
            self.check()
        row['thin_members'] = [{'path': 'member.o', 'bytes': (self.build / 'member.o').stat().st_size,
                                'sha256': self.module.digest(self.build / 'member.o')}]
        self.check()
        (self.build / 'member.o').write_bytes(b'changed member')
        with self.assertRaises(ValueError):
            self.check()


if __name__ == '__main__':
    unittest.main()
