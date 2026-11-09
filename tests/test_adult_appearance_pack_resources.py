"""从真实1.7作者包审查正式组合闭包；坏声明必须在compose时拒绝，不冒充引擎验收。"""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest

# 审查lane可显式只读中央源码；合入后默认使用当前仓库，不复制一套生产实现。
ROOT = Path(os.environ.get("HANMO_APPEARANCE_REVIEW_ROOT", Path(__file__).resolve().parents[1])).resolve()
sys.path.insert(0, str(ROOT))
from tools import native_resident_profile
from tools.content.pack_compose import compose_pack_set
from tools.content.pack_errors import PackError
from tools.content.packs import discover_packs


def read(path):
    """读取测试选定的真实作者文件，不回落到工作树其他版本。"""
    return json.loads(path.read_bytes())


class AdultAppearancePackResourcesTest(unittest.TestCase):
    """同时核正常安装路径与坏包拒绝；所有写操作只发生在本测试临时目录。"""

    @classmethod
    def setUpClass(cls):
        """一次从当前完整作者包compose基线，后续故障都从独立副本制造。"""
        cls.source = ROOT / "content/packs/authored/local_society_1_7"
        cls.selection = read(ROOT / "content/packs/selections/local-society-1.7.0.json")
        cls.temp = tempfile.TemporaryDirectory(prefix="adult-pack-closure-")
        cls.directory = Path(cls.temp.name)
        cls.output = cls.directory / "baseline"
        cls.result = compose_pack_set([cls.source], cls.selection, cls.output)

    @classmethod
    def tearDownClass(cls):
        """只删除测试自建临时根，不触中央作者包、正式compose代或工坊。"""
        cls.temp.cleanup()

    @contextmanager
    def broken_pack(self, name):
        """每种故障使用自己的完整物理副本，防止前一缺件污染后续案例。"""
        directory = self.directory / name
        source = directory / "pack"
        shutil.copytree(self.source, source)
        try:
            yield source, directory / "composed"
        finally:
            shutil.rmtree(directory)

    def replace_declared_bytes(self, source, logical, raw):
        """显式模拟作者修改资源并更新摘要，以检测语义闭包而非只测试旧摘要拒绝。"""
        (source / logical).write_bytes(raw)
        manifest = read(source / "pack.json")
        next(row for row in manifest["resources"] if row["path"] == logical)["sha256"] = hashlib.sha256(raw).hexdigest()
        (source / "pack.json").write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")

    def assert_compose_rejects(self, source, output, label):
        """坏包若仍发布STRUCTURE_VALID即失败；拒绝还须保留上一可安装代的全部字节。"""
        shutil.copytree(self.output, output)
        before = {str(path.relative_to(output)): hashlib.sha256(path.read_bytes()).hexdigest() for path in output.rglob("*") if path.is_file()}
        try:
            result = compose_pack_set([source], self.selection, output)
        except PackError:
            after = {str(path.relative_to(output)): hashlib.sha256(path.read_bytes()).hexdigest() for path in output.rglob("*") if path.is_file()}
            self.assertEqual(after, before, "坏包拒绝必须保留上一代全部成员与字节：" + label)
            return
        self.fail(label + "仍通过公开compose：" + result["status"])

    def test_real_source_pack_has_exact_frozen_asset_closure(self):
        """真实192元组所需43源PNG与六个目录文件全部已声明、固定摘要且尺寸正确。"""
        manifest = read(self.source / "pack.json")
        resources = {row["path"]: row for row in manifest["resources"]}
        catalog = read(self.source / "data/adult_appearance_compositions.json")
        self.assertEqual(catalog, read(ROOT / "game/data/adult_appearance_compositions.json"))
        expected_data = {"data/adult_appearance_compositions.json", "character_studio/fittings.json", "character_studio/representatives.json"}
        expected_images = set()
        for family in catalog["families"]:
            path = "character_studio/" + family["catalog"]
            expected_data.add(path)
            definition = read(self.source / path)
            self.assertEqual(definition, read(ROOT / "game" / path))
            for face in range(4):
                ids = {"head-" + str(face), "hair-" + str(face), "outfit-0"}
                for feature in ("brows", "eyes", "nose", "mouth"):
                    ids.update(definition["face_profiles"][face]["feature_sources"][feature])
                expected_images.update("character_studio/" + definition["assets"][identifier].removeprefix("res://") for identifier in ids)
        self.assertEqual(len(expected_images), 43)
        self.assertEqual(len(expected_data), 6)
        for kind, paths in (("data", expected_data), ("image", expected_images)):
            for logical in paths:
                row = resources[logical]
                raw = (self.source / logical).read_bytes()
                self.assertEqual((row["kind"], row["required"], row["sha256"]), (kind, True, hashlib.sha256(raw).hexdigest()))
                self.assertEqual(raw, (ROOT / "game" / logical).read_bytes())
                if kind == "image":
                    self.assertEqual(raw[:8], b"\x89PNG\r\n\x1a\n")
                    self.assertEqual(struct.unpack(">II", raw[16:24]), (1024, 1536))
        declared = {path for path in resources if path.startswith("character_studio/") or path == "data/adult_appearance_compositions.json"}
        self.assertEqual(declared, expected_data | expected_images)

    def test_composed_and_installed_six_field_roots_preserve_actual_bytes(self):
        """走生产stage_content形成安装代，根解析命中真实安装文件而非源工程副本。"""
        self.assertEqual(self.result["status"], "STRUCTURE_VALID")
        self.assertFalse(self.result["runtime_verified"])
        presentation = read(self.output / "presentation.json")
        binding = presentation["adult_appearance_composition"]
        prefix = "res://assets/art/packs/society/"
        self.assertEqual(binding, {"catalog": prefix + "data/adult_appearance_compositions.json", "resource_root": prefix + "character_studio/", "fittings": prefix + "character_studio/fittings.json", "representatives": prefix + "character_studio/representatives.json", "renderer": "res://ui/adult_appearance_renderer.gd", "session": "res://character_studio/src/studio_session.gd"})
        game = self.directory / "staged/game"
        (game / "data").mkdir(parents=True)
        config = {"content": {"pack_set_digest": self.result["pack_set_digest"], "scenario_id": "society::scenario.qingchuan_spring_reserve"}}
        installed = native_resident_profile._stage_content(ROOT, game, self.output, config)
        self.assertEqual(installed["pack_set_digest"], self.result["pack_set_digest"])
        self.assertEqual(installed["installed_set"]["directory"], "content-sets/" + self.result["pack_set_digest"])
        base = game / "data/generated" / installed["installed_set"]["directory"]
        for relative in ("catalog", "fittings", "representatives"):
            actual = base / binding[relative].removeprefix("res://")
            self.assertEqual(actual.read_bytes(), (self.output / binding[relative].removeprefix("res://")).read_bytes())
        for resource in read(self.source / "pack.json")["resources"]:
            if not resource["path"].startswith("character_studio/"):
                continue
            installed_path = base / "assets/art/packs/society" / resource["path"]
            self.assertEqual(hashlib.sha256(installed_path.read_bytes()).hexdigest(), resource["sha256"])
        presets = native_resident_profile._export_presets((ROOT / "game/export_presets.cfg").read_bytes()).decode()
        self.assertIn("data/adult_appearance_compositions.json", presets)
        self.assertIn("character_studio/src/studio_session.gd", presets)
        self.assertIn("assets/art/packs/**/*.json", presets)
        self.assertIn("game/character_studio/src/studio_session.gd", native_resident_profile.RUNTIME_FILES)
        self.assertIn("game/data/adult_appearance_compositions.json", native_resident_profile.RUNTIME_FILES)

    def test_declared_image_tamper_without_new_hash_is_rejected(self):
        """既有声明SHA仍能拒绝源字节篡改，不把新闭包缺口误写成全部摘要机制失效。"""
        with self.broken_pack("stale-hash") as (source, _output):
            target = source / "character_studio/assets/runtime/medium-v3/features-sturdy-base.png"
            target.write_bytes(b"broken-png")
            with self.assertRaises(PackError) as caught:
                discover_packs([source])
            self.assertEqual(caught.exception.code, "PackResourceDigestMismatch")

    def test_omitted_feature_declaration_and_file_is_rejected(self):
        """删掉真实五官的声明和文件仍须失败，完整的12旧头像不能遮住新组合缺件。"""
        with self.broken_pack("missing-feature") as (source, output):
            logical = "character_studio/assets/runtime/medium-v3/features-sturdy-base.png"
            manifest = read(source / "pack.json")
            manifest["resources"] = [row for row in manifest["resources"] if row["path"] != logical]
            (source / "pack.json").write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
            (source / logical).unlink()
            self.assert_compose_rejects(source, output, "缺少冻结范围内五官源")

    def test_catalog_missing_source_with_updated_hash_is_rejected(self):
        """目录本身SHA一致仍须校验实际引用；错误头部路径不能只到头像预览时才暴露。"""
        with self.broken_pack("missing-head-reference") as (source, output):
            logical = "character_studio/catalog_medium_v3.json"
            catalog = read(source / logical)
            catalog["assets"]["head-0"] = "res://assets/runtime/nonexistent-head.png"
            self.replace_declared_bytes(source, logical, (json.dumps(catalog, ensure_ascii=False) + "\n").encode())
            self.assert_compose_rejects(source, output, "族目录引用不存在头部")

    def test_corrupt_declared_source_with_updated_hash_is_rejected(self):
        """实际PNG坏字节即便有匹配声明SHA，也不能通过新组合资源结构检查。"""
        with self.broken_pack("invalid-png") as (source, output):
            logical = "character_studio/assets/runtime/medium-v3/features-sturdy-base.png"
            self.replace_declared_bytes(source, logical, b"not-an-image-but-correctly-hashed")
            self.assert_compose_rejects(source, output, "新组合源PNG无法解码")

    def test_composition_policy_without_registered_sources_is_rejected(self):
        """章节已声明192组合策略时资源登记为必需，不能留下首张旧preset可见但换件全失败的包。"""
        with self.broken_pack("missing-composition-binding") as (source, output):
            presentation = read(source / "presentation.json")
            del presentation["adult_appearance_composition"]
            (source / "presentation.json").write_text(json.dumps(presentation, ensure_ascii=False), encoding="utf-8")
            self.assert_compose_rejects(source, output, "新章节组合policy缺整组资源登记")

    def test_unknown_pack_catalog_version_is_rejected_before_runtime(self):
        """目录版本必须与正式引擎同表，不能只核192元组后把运行时必拒目录打包。"""
        with self.broken_pack("unknown-catalog-version") as (source, output):
            logical = "data/adult_appearance_compositions.json"
            catalog = read(source / logical)
            catalog["version"] = 999
            self.replace_declared_bytes(source, logical, json.dumps(catalog, ensure_ascii=False).encode())
            self.assert_compose_rejects(source, output, "包目录与正式引擎目录版本不一致")

    def test_representative_feature_semantics_must_match_old_preset(self):
        """骨相索引正确不足以代表样本正确，四槽必须对应旧12元组的本族语义。"""
        with self.broken_pack("sample-wrong-feature") as (source, output):
            logical = "character_studio/representatives.json"
            catalog = read(source / logical)
            catalog["samples"][0]["selection"]["brows"] = 1
            self.replace_declared_bytes(source, logical, json.dumps(catalog, ensure_ascii=False).encode())
            self.assert_compose_rejects(source, output, "原代表眉形语义被篡改")

    def test_malformed_compatibility_is_structured_rejection(self):
        """坏when结构必须走PackError和原子保留，不能抛未包装AttributeError中断作者入口。"""
        with self.broken_pack("malformed-compatibility") as (source, output):
            logical = "character_studio/catalog_medium_v3.json"
            catalog = read(source / logical)
            catalog["compatibility"][0]["when"] = []
            self.replace_declared_bytes(source, logical, json.dumps(catalog, ensure_ascii=False).encode())
            self.assert_compose_rejects(source, output, "兼容规则when不是字典")

    def test_original_fitting_field_types_match_runtime_contract(self):
        """语义覆盖选值前仍须核原始目录类型；字符串默认值和伪数组不能到运行时才拒绝。"""
        for case in ("noninteger-default", "nonarray-options"):
            with self.subTest(case=case), self.broken_pack(case) as (source, output):
                logical = "character_studio/catalog_medium_v3.json"
                catalog = read(source / logical)
                if case == "noninteger-default":
                    catalog["defaults"]["skin"] = "warm"
                else:
                    catalog["fields"]["eyes"]["options"] = "ab"
                self.replace_declared_bytes(source, logical, json.dumps(catalog, ensure_ascii=False).encode())
                self.assert_compose_rejects(source, output, "原始族目录类型与正式运行时不一致：" + case)


if __name__ == "__main__":
    unittest.main()
