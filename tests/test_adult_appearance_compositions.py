"""核实正式192元组真实资产与工坊语义来源；不把静态检查当作引擎或美术验收。"""
from collections import Counter
from itertools import product
import json
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "game"
STUDIO = GAME / "character_studio"
FEATURES = ("brows", "eyes", "nose", "mouth")


def read(path):
    """只读精确作者文件，不扫描目录或补默认值。"""
    return json.loads(path.read_text(encoding="utf-8"))


class AdultCompositionAssetsTest(unittest.TestCase):
    """以现有比例族、样本和PNG头检查每条正式元组的可还原性。"""

    def setUp(self):
        """独立读取源与正式表，测试修改不能影响下一断言。"""
        self.catalog = read(GAME / "data/adult_appearance_compositions.json")
        self.accepted = read(GAME / "data/accepted_adult_appearances.json")["records"]
        self.representatives = read(STUDIO / "representatives.json")["samples"]
        self.fittings = read(STUDIO / "fittings.json")["fittings"]

    def test_exact_cartesian_scope_and_old_presets(self):
        """三族各四骨相各16组完整元组，旧12项完全包含且无第二持久身份或路径。"""
        records = self.catalog["records"]
        faces = self.catalog["faces"]
        self.assertEqual(self.catalog["age_domain"], [18, 59])
        self.assertEqual(len(records), 192)
        self.assertEqual(len({row["id"] for row in records}), 192)
        self.assertEqual(Counter(row["fitting_id"] for row in records), {row["id"]: 64 for row in self.fittings})
        self.assertEqual({face["id"] for face in faces}, {row["id"] for row in self.accepted})
        for face in faces:
            rows = [row for row in records if row["base_features"]["face_shape"] == face["id"]]
            actual = {tuple(row["base_features"][field] for field in FEATURES) for row in rows}
            expected = set(product(*[[item["id"] for item in self.catalog["options"][field]] for field in FEATURES]))
            self.assertEqual(actual, expected)
        for row in records:
            self.assertEqual(set(row), {"id", "label", "base_features", "attire_style", "fitting_id"})
            self.assertEqual(set(row["base_features"]), {"face_shape", *FEATURES, "hair", "beard", "skin_palette", "build"})
            self.assertEqual({key: row["base_features"][key] for key in ("hair", "beard", "skin_palette", "build")}, {"hair": "bun", "beard": "none", "skin_palette": "warm", "build": "normal"})
            self.assertEqual(row["attire_style"], "plain")
        for old in self.accepted:
            self.assertEqual(sum(row["base_features"] == old["base_features"] and row["attire_style"] == old["attire_style"] for row in records), 1)

    def test_source_identity_semantics_compatibility_and_real_asset_dimensions(self):
        """每个语义先映射到目标族索引，再使用目标骨相源和作者区域，不跨脸偷换素材。"""
        families = {row["id"]: row for row in self.catalog["families"]}
        definitions = {row["id"]: read(STUDIO / row["catalog"].removeprefix("res://")) for row in self.fittings}
        samples = {row["id"]: row for row in self.representatives}
        faces = {row["id"]: row for row in self.catalog["faces"]}
        checked_paths = set()
        for fitting in self.fittings:
            self.assertEqual("res://" + families[fitting["id"]]["catalog"], fitting["catalog"])
        for face in faces.values():
            source = samples[face["id"]]
            self.assertEqual((face["fitting_id"], face["face_index"]), (source["fitting_id"], source["selection"]["face"]))
        for row in self.catalog["records"]:
            definition = definitions[row["fitting_id"]]
            face = faces[row["base_features"]["face_shape"]]
            selected = dict(definition["defaults"], face=face["face_index"])
            semantics = {**{field: row["base_features"][field] for field in FEATURES}, "hair": "bun", "beard": "none", "skin": "warm", "hair_color": "black", "outfit": "plain", "cloth_color": "original"}
            for field, token in semantics.items():
                selected[field] = definition["choice_semantics"][field].index(token)
            for rule in definition.get("compatibility", []):
                if all(selected[key] == value for key, value in rule["when"].items()):
                    self.assertTrue(all(selected[key] == value for key, value in rule["requires"].items()))
            profile = definition["face_profiles"][selected["face"]]
            assets = ["head-" + str(selected["face"]), "hair-" + str(selected["face"]), "outfit-0"]
            for field in FEATURES:
                self.assertEqual(len(profile["feature_sources"][field]), len(definition["choice_semantics"][field]))
                assets.append(profile["feature_sources"][field][selected[field]])
                for x, y, rx, ry in profile["regions"][field]:
                    self.assertTrue(rx > 0 and ry > 0 and x - rx >= 0 and y - ry >= 0 and x + rx <= 512 and y + ry <= 768)
            for asset in assets:
                original = definition["assets"][asset]
                self.assertTrue(original.startswith("res://assets/runtime/"))
                path = STUDIO / original.removeprefix("res://")
                if path in checked_paths:
                    continue
                data = path.read_bytes()
                self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
                self.assertEqual(struct.unpack(">II", data[16:24]), (1024, 1536))
                checked_paths.add(path)
            x, y, width, height = definition["avatar_crop"]
            self.assertTrue(width == height and x >= 0 and y >= 0 and x + width <= 512 and y + height <= 768)
        self.assertGreater(len(checked_paths), 30)


if __name__ == "__main__":
    unittest.main()
