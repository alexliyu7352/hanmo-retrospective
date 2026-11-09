"""原生作者试玩的协议与发布边界窄测；文件夹具不冒充真实原生运行验收。"""
from contextlib import ExitStack
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.content import pack_preview as preview
from tools.content.editor_protocol import EditorSession
from tools.content.io import canonical
from tools.content.pack_document import PackDocument
from tools.content.pack_errors import PackError


class NativePackPreviewTests(unittest.TestCase):
    """保留真实目录、字节复核和原子发布，只隔离已有compose与资源语义检查。"""

    def setUp(self):
        """每例构造独立可信源码与组合代，绝不读取或覆盖正式作者包、stage或存档。"""
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.root = self.base / 'trusted'
        self.composed = self.base / 'composed'
        self.composed.mkdir()
        self.rules = {'id': 'hanmo.rules', 'version': '1.0.0'}
        self.sources = {}
        for name, raw in {'game/main.tscn': b'fixture scene',
                          'game/data/pack_runtime.json': canonical(self.rules),
                          'schemas/content.schema.json': b'{}'}.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            self.sources[path] = raw
        (self.composed / 'pack-set.lock.json').write_bytes(canonical({'rules_api': self.rules}))
        (self.composed / 'presentation.json').write_bytes(b'{}')
        self.digest = hashlib.sha256((self.composed / 'pack-set.lock.json').read_bytes()).hexdigest()
        self.record = {'directory': 'content-sets/' + self.digest, 'pack_set_digest': self.digest}
        self.set_scenarios({'z.dynamic': {'dynamic_world': {}}, 'a.legacy': {}})
        self.destination = self.base / 'preview'
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        for target, kwargs in [
            ('ROOT', {'new': self.root}),
            ('_runtime_sources', {'side_effect': lambda: dict(self.sources)}),
            ('staging.composed_set_record', {'return_value': self.record}),
            ('staging.load_contract', {'return_value': {'capabilities': []}}),
            ('staging.runtime_declaration', {'return_value': {}}),
            ('staging.write_schema_index', {'return_value': None}),
            ('staging.stage_installed_sets', {'side_effect': self.stage}),
            ('staging.installed_index_records', {'return_value': []}),
            ('validate_registry', {'return_value': []}),
        ]:
            self.stack.enter_context(patch('tools.content.pack_preview.' + target, **kwargs))

    def set_scenarios(self, scenarios):
        """仅改隔离组合夹具，用非首位动态场景防止只检查默认入口。"""
        (self.composed / 'catalog.hmcatalog.json').write_bytes(canonical({'catalog': {'scenarios': scenarios}}))

    def stage(self, root, generated, previous, sources, *, foundation_sets):
        """在真实临时文件系统复制夹具，并按生产接口返回源字节和成员快照。"""
        self.assertEqual(sources, [self.composed])
        self.assertFalse(foundation_sets)
        installed = generated / self.record['directory']
        shutil.copytree(self.composed, installed)
        files = preview.staging.files_in(self.composed)
        return {path: path.read_bytes() for path in files.values()}, {self.composed: sorted(files)}

    def test_any_dynamic_scenario_requires_native_before_publication(self):
        """默认剧本为旧GD时，目录中另一动态剧本仍必须在发布前明确拒绝缺失原生。"""
        with self.assertRaises(PackError) as caught:
            preview.prepare_preview(self.composed, self.destination, expected_pack_set_digest=self.digest)
        self.assertEqual(caught.exception.code, 'MissingPreviewNativeRuntime')
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.base.glob('.pack-preview-*')), [])

    def test_legacy_pack_keeps_original_gd_path_and_uuid_storage(self):
        """无动态世界的旧包仍可无参准备，每次独立存档且不宣称运行验证。"""
        self.set_scenarios({'legacy.scenario': {}})
        first = preview.prepare_preview(self.composed, self.destination, expected_pack_set_digest=self.digest)
        second = preview.prepare_preview(self.composed, self.base / 'second', expected_pack_set_digest=self.digest)
        self.assertFalse(first['runtime_verified'])
        self.assertNotEqual(first['storage_profile'], second['storage_profile'])
        self.assertNotIn('native_runtime', first)
        project = (self.destination / 'game/project.godot').read_text()
        self.assertIn('hanmo-preview/' + first['storage_profile'], project)
        self.assertNotIn('native_resident_world_enabled', project)

    def native_fixture(self):
        """返回明确假的原生适配输出，仅证明同一发布器的文件与身份传递边界。"""
        source = self.base / 'native-stage/game'
        library = source / 'native_resident_bridge/libhanmo_world_bridge.so'
        library.parent.mkdir(parents=True)
        library.write_bytes(b'unit-fixture-not-an-ELF')
        return source, {'files': {'native_resident_bridge/libhanmo_world_bridge.so': library.read_bytes(),
                                 'native_resident_bridge/hanmo_world_bridge.gdextension': b'fixture descriptor',
                                 '.godot/extension_list.cfg': b'res://native_resident_bridge/hanmo_world_bridge.gdextension\n'},
                        'identity': {'format': 'TEST-ONLY', 'library_sha256': hashlib.sha256(library.read_bytes()).hexdigest()},
                        'source_paths': {library: library.read_bytes()}}

    def test_native_files_and_identity_use_original_manifest_and_publish(self):
        """动态目录合入真实适配接口的三项输出，身份归本次组合且存档不继承stage。"""
        source, native = self.native_fixture()
        with patch('tools.content.preview_native.prepare_native_runtime', return_value=native) as prepare:
            result = preview.prepare_preview(self.composed, self.destination,
                                             expected_pack_set_digest=self.digest, native_stage=source)
        self.assertEqual(prepare.call_count, 2)
        prepare.assert_called_with(self.root, source)
        self.assertEqual(result['native_runtime'], native['identity'])
        self.assertEqual(result['pack_set_digest'], self.digest)
        self.assertFalse(result['runtime_verified'])
        for relative, raw in native['files'].items():
            self.assertEqual((self.destination / 'game' / relative).read_bytes(), raw)
        project = (self.destination / 'game/project.godot').read_text()
        # Main已按dynamic_world分派；全局开关会把同包旧GD剧本错误强迁原生。
        self.assertNotIn('native_resident_world_enabled', project)
        self.assertIn('hanmo-preview/' + result['storage_profile'], project)
        self.assertIn('config/default_scenario="a.legacy"', project)
        manifest = json.loads((self.destination / preview.SOURCE_MANIFEST).read_bytes())
        self.assertEqual(manifest['native_runtime'], native['identity'])
        self.assertEqual(manifest['pack_set_digest'], self.digest)
        self.assertEqual(hashlib.sha256((self.destination / preview.SOURCE_MANIFEST).read_bytes()).hexdigest(),
                         result['source_manifest_sha256'])
        outputs = json.loads((self.destination / preview.OUTPUT_MANIFEST).read_bytes())['files']
        for relative in native['files']:
            self.assertIn('game/' + relative, outputs)

    def test_rejected_native_never_publishes_and_keeps_cause(self):
        """初次原生身份错误透传原错误；已验证后改变身份转为源变更并清除候选。"""
        source, native = self.native_fixture()
        failure = PackError('PreviewNativeIdentityMismatch', 'fixture identity mismatch')
        for outcomes, code in [([failure], failure.code), ([native, failure], 'PreviewInputsChanged')]:
            with self.subTest(code=code), patch('tools.content.preview_native.prepare_native_runtime', side_effect=outcomes):
                with self.assertRaises(PackError) as caught:
                    preview.prepare_preview(self.composed, self.destination,
                                            expected_pack_set_digest=self.digest, native_stage=source)
                self.assertEqual(caught.exception.code, code)
                if code == 'PreviewInputsChanged':
                    self.assertEqual(caught.exception.details['cause']['code'], failure.code)
                self.assertFalse(self.destination.exists())
                self.assertEqual(list(self.base.glob('.pack-preview-*')), [])

    def test_native_source_bytes_and_member_changes_block_publication(self):
        """不只比身份字符串：源字节、删除、新成员以及稀疏原生树改变均阻断发布。"""
        source, native = self.native_fixture()
        library = next(iter(native['source_paths']))
        for mutation in ('bytes', 'delete', 'members', 'identity'):
            with self.subTest(mutation=mutation):
                library.write_bytes(native['source_paths'][library])
                calls = 0

                def prepare(root, stage):
                    """在发布前第二次校验注入受控变更，区分源字节和成员边界。"""
                    nonlocal calls
                    calls += 1
                    if calls == 1:
                        return native
                    result = deepcopy(native)
                    if mutation == 'bytes':
                        library.write_bytes(b'changed')
                    elif mutation == 'delete':
                        library.unlink()
                    elif mutation == 'members':
                        result['source_paths'][source / 'new-source'] = b'new'
                    else:
                        result['identity']['changed'] = True
                    return result

                with patch('tools.content.preview_native.prepare_native_runtime', side_effect=prepare):
                    with self.assertRaises(PackError) as caught:
                        preview.prepare_preview(self.composed, self.destination,
                                                expected_pack_set_digest=self.digest, native_stage=source)
                self.assertEqual(caught.exception.code, 'PreviewInputsChanged')
                self.assertFalse(self.destination.exists())
                self.assertEqual(list(self.base.glob('.pack-preview-*')), [])

    def test_native_publish_keeps_concurrent_destination(self):
        """原生路径也使用唯一不覆盖原子发布，末刻出现的目标和原件完整保留。"""
        source, native = self.native_fixture()
        publish = preview._publish

        def raced_publish(candidate, destination):
            """在最终目标检查后模拟另一操作者先发布，不能退回覆盖目标。"""
            destination.mkdir()
            (destination / 'owner.txt').write_bytes(b'concurrent owner')
            publish(candidate, destination)

        with patch('tools.content.preview_native.prepare_native_runtime', return_value=native), \
                patch.object(preview, '_publish', side_effect=raced_publish):
            with self.assertRaises(PackError) as caught:
                preview.prepare_preview(self.composed, self.destination,
                                        expected_pack_set_digest=self.digest, native_stage=source)
        self.assertEqual(caught.exception.code, 'PreviewDestinationExists')
        self.assertEqual((self.destination / 'owner.txt').read_bytes(), b'concurrent owner')
        self.assertEqual(list(self.base.glob('.pack-preview-*')), [])

    def test_native_destination_cannot_write_into_source_stage(self):
        """候选创建前拒绝来源stage内部目标，连临时目录也不能写进正式来源。"""
        source, native = self.native_fixture()
        before = {name: path.read_bytes() for name, path in preview.staging.files_in(source.parent).items()}
        for destination in (source / 'preview', source.parent / 'preview'):
            with self.subTest(destination=destination), \
                    patch('tools.content.preview_native.prepare_native_runtime', return_value=native), \
                    patch.object(preview.tempfile, 'mkdtemp') as temporary:
                with self.assertRaises(PackError) as caught:
                    preview.prepare_preview(self.composed, destination,
                                            expected_pack_set_digest=self.digest, native_stage=source)
                self.assertEqual(caught.exception.code, 'ProtectedPreviewPath')
                temporary.assert_not_called()
                self.assertFalse(destination.exists())
        self.assertEqual(before, {name: path.read_bytes() for name, path in preview.staging.files_in(source.parent).items()})


class NativePreviewProtocolTests(unittest.TestCase):
    """实际会话与包门面共同验证版本、文档、原件摘要，适配仅替换最终准备操作。"""

    def setUp(self):
        """构造已保存的最小包快照，文件guard仍读取磁盘原字节。"""
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.author = self.base / 'author'
        self.author.mkdir()
        path = self.author / 'pack.json'
        raw = canonical({'id': 'fixture', 'version': '1.0.0'})
        path.write_bytes(raw)
        document = object.__new__(PackDocument)
        document._path = path
        document._manifest = json.loads(raw)
        document._saved_manifest = raw
        document._documents = {}
        document._inputs = {path: raw}
        document._revision = 3
        document._import_pending = False
        document._dependency_context = {'records': [], 'snapshots': {}}
        self.digest = 'a' * 64
        document._last_compose = {'output': self.base / 'composed', 'pack_set_digest': self.digest, 'revision': 3}
        self.session = EditorSession()
        self.session.document = document
        self.session.document_id = 'current-document'
        self.request = {'id': 1, 'op': 'preview_prepare', 'document_id': self.session.document_id,
                        'revision': 3, 'source_sha256': hashlib.sha256(raw).hexdigest(),
                        'args': {'destination': str(self.base / 'preview'), 'expected_pack_set_digest': self.digest,
                                 'native_stage': str(self.base / 'native/game')}}

    def test_protocol_passes_optional_native_stage_through_same_document(self):
        """新增字符串参数经过原包门面，省略和空字符串均保留旧无原生接口。"""
        description = self.session.handle({'id': 0, 'op': 'describe'})
        self.assertEqual(description['result']['pack_operations']['preview_prepare']['optional'], ['native_stage'])
        for mode in ('selected', 'empty', 'omitted'):
            with self.subTest(mode=mode):
                request = deepcopy(self.request)
                if mode == 'empty':
                    request['args']['native_stage'] = ''
                elif mode == 'omitted':
                    request['args'].pop('native_stage')
                with patch.object(preview, 'prepare_preview', return_value={'status': 'PREVIEW_PREPARED'}) as prepare:
                    reply = self.session.handle(request)
                self.assertTrue(reply['ok'], reply)
                self.assertEqual(reply['result']['revision'], 3)
                self.assertFalse(reply['result']['runtime_verified'])
                prepare.assert_called_once_with(self.base / 'composed', self.base / 'preview',
                    expected_pack_set_digest=self.digest,
                    native_stage=self.base / 'native/game' if mode == 'selected' else None)

    def test_protocol_keeps_original_write_guard_priority(self):
        """即使提供原生来源，也不能用错文档、过期版本、错误源token或非字符串启动准备。"""
        for field, value, expected in [('document_id', 'old', 'STALE_DOCUMENT'),
                                        ('revision', 2, 'STALE_REVISION'),
                                        ('source_sha256', 'b' * 64, 'SOURCE_CHANGED'),
                                        ('native_stage', None, 'BAD_ARGUMENTS'),
                                        ('native_stage', 1, 'BAD_ARGUMENTS')]:
            with self.subTest(field=field, value=value):
                request = deepcopy(self.request)
                (request['args'] if field == 'native_stage' else request)[field] = value
                with patch.object(preview, 'prepare_preview') as prepare:
                    reply = self.session.handle(request)
                self.assertFalse(reply['ok'], reply)
                self.assertEqual(reply['error']['code'], expected)
                prepare.assert_not_called()

    def test_document_still_rejects_unsaved_uncomposed_and_external_changes(self):
        """可选原生来源不能绕过已保存组合约束或作者原件前后复核。"""
        document = self.session.document
        original = document._inputs[document._path]
        for mutation, expected in [('dirty', 'UNSAVED_CHANGES'), ('uncomposed', 'COMPOSE_REQUIRED'),
                                   ('before', 'SOURCE_CHANGED'), ('during', 'SOURCE_CHANGED')]:
            with self.subTest(mutation=mutation):
                document._manifest = json.loads(original)
                document._path.write_bytes(original)
                document._last_compose = {'output': self.base / 'composed', 'pack_set_digest': self.digest, 'revision': 3}
                if mutation == 'dirty':
                    document._manifest['changed'] = True
                elif mutation == 'uncomposed':
                    document._last_compose = None
                elif mutation == 'before':
                    document._path.write_bytes(b'changed author')

                def prepare(*args, **kwargs):
                    """模拟准备期间原件改变，包门面必须拒绝旧结果进入启动路径。"""
                    document._path.write_bytes(b'changed during preparation')
                    return {'status': 'PREVIEW_PREPARED'}

                with patch.object(preview, 'prepare_preview', side_effect=prepare) as call:
                    reply = self.session.handle(self.request)
                self.assertFalse(reply['ok'], reply)
                self.assertEqual(reply['error']['code'], expected)
                self.assertEqual(call.call_count, 1 if mutation == 'during' else 0)


if __name__ == '__main__':
    unittest.main()
