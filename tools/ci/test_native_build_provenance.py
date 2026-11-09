"""只测固定源与五件输出的拒绝/保全边界；不伪称CI或目标原生构建通过。"""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('record_build', Path(__file__).with_name('record_native_build.py'))
record = importlib.util.module_from_spec(spec)
spec.loader.exec_module(record)


class NativeBuildProvenanceTests(unittest.TestCase):
    """原run实际SHA是唯一来源，缓存与交付输出不相互覆盖。"""
    def test_fixed_clean_source(self):
        expected = 'a' * 40
        with patch.object(record, 'git', side_effect=[expected, '']):
            self.assertEqual(record.require_source(Path('.'), expected), expected)

    def test_floating_or_malformed_source_rejected(self):
        for source in ('main', 'a'*39, 'a'*41, 'A'*40, 'a'*40+'\n', '', 'x; echo test'):
            with self.subTest(source=source), patch.object(record, 'git') as git:
                with self.assertRaises(RuntimeError):
                    record.require_source(Path('.'), source)
                git.assert_not_called()

    def test_different_or_dirty_source_rejected(self):
        for outputs in (['b'*40], ['a'*40, ' M native/changed.cpp']):
            with patch.object(record, 'git', side_effect=outputs):
                with self.assertRaises(RuntimeError):
                    record.require_source(Path('.'), 'a'*40)

    def make_outputs(self, folder):
        """微小假二进制只验证保全算法；没有加载或运行它们。"""
        build, out = folder/'build', folder/'out'
        build.mkdir(); out.mkdir(); (build/'godot-bridge').mkdir()
        for relative in ('hanmo_resident_journey', 'godot-bridge/libhanmo_world_bridge.so',
                         'libhanmo_resident.a', 'libhanmo_world.a', 'libhanmo_flecs.a'):
            (build/relative).write_bytes(('fixture:' + relative).encode())
        return build, out

    def test_five_outputs_preserved_with_exact_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            build,out=self.make_outputs(Path(directory))
            rows=record.preserve_outputs(build,out)
            self.assertEqual(len(rows),5)
            self.assertEqual(len({Path(row['path']).name for row in rows}),5)
            for row in rows:
                p=Path(row['path'])
                self.assertEqual(row['sha256'],record.digest(p))
                self.assertEqual(row['bytes'],p.stat().st_size)
            self.assertTrue((build/'libhanmo_resident.a').is_file())

    def test_missing_output_does_not_publish_partial_set(self):
        with tempfile.TemporaryDirectory() as directory:
            build,out=self.make_outputs(Path(directory));(build/'libhanmo_world.a').unlink()
            with self.assertRaises(RuntimeError):record.preserve_outputs(build,out)
            self.assertEqual(list(out.iterdir()),[])

    def test_existing_destination_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            build,out=self.make_outputs(Path(directory));p=out/'libhanmo_world.a';p.write_bytes(b'original')
            with self.assertRaises(RuntimeError):record.preserve_outputs(build,out)
            self.assertEqual(p.read_bytes(),b'original')
            self.assertEqual(list(out.iterdir()),[p])

    def test_source_mutated_during_copy_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            build,out=self.make_outputs(Path(directory));copy=record.shutil.copy2
            def changing(source,target):
                copy(source,target);source.write_bytes(b'changed')
            with patch.object(record.shutil,'copy2',side_effect=changing):
                with self.assertRaises(RuntimeError):record.preserve_outputs(build,out)

    def test_workflow_binds_inputs_as_data_and_cache_to_actual_sha(self):
        root=Path(__file__).resolve().parents[2]
        workflow=(root/'.github/workflows/native-ubuntu2004.yml').read_text()
        self.assertIn('INPUT_SOURCE: ${{ inputs.source_ref }}',workflow)
        self.assertIn('EVENT_SOURCE: ${{ github.sha }}',workflow)
        self.assertIn('ref: ${{ steps.requested.outputs.sha }}',workflow)
        self.assertIn('test "$actual" = "$EXPECTED_SOURCE"',workflow)
        # 工作流先按原OID冻结监督工具，随后把已核产品SHA作为第四参数传入。
        freeze=workflow.index('Freeze the supervising tool before checking out product source')
        self.assertLess(freeze,workflow.index('ref: ${{ steps.requested.outputs.sha }}'))
        self.assertIn("source=pathlib.Path('tools/ci/supervise_native_build.sh')",workflow)
        self.assertIn("['git','rev-parse','HEAD:'+str(source)]",workflow)
        self.assertIn("raise SystemExit('Supervisor Git object differs')",workflow)
        self.assertIn('bash "$RUNNER_TEMP/hanmo-supervisor/supervise_native_build.sh"',workflow)
        self.assertIn("'${{ steps.source.outputs.sha }}' \"$RUNNER_TEMP/hanmo-supervisor/identity.json\"",workflow)
        supervisor=(root/'tools/ci/supervise_native_build.sh').read_text()
        self.assertIn('source_sha=$4',supervisor)
        self.assertIn('[[ "$source_sha" =~ ^[0-9a-f]{40}$ ]]',supervisor)
        self.assertIn("raise SystemExit('Frozen supervisor differs')",supervisor)
        self.assertIn('-e SOURCE_SHA="$source_sha"',supervisor)
        self.assertIn('-v "$source_root:/src:ro"',supervisor)
        self.assertIn('ubuntu:20.04 bash /src/tools/ci/build_ubuntu2004.sh',supervisor)
        build=(root/'tools/ci/build_ubuntu2004.sh').read_text()
        self.assertIn('record_native_build.py --root "$src" --cache "$cache" --output "$out" --expected-source "$SOURCE_SHA"',build)
        self.assertNotIn('ref: ${{ inputs.source_ref }}',workflow)


if __name__ == '__main__':unittest.main()
