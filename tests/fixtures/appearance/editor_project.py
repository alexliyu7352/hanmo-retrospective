"""构造外貌面板的独立作者工程，保留同一协议和正式准备器；不改生产清单。"""
from __future__ import annotations

import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tests.fixtures.presentation.editor_project import create as create_base


def create(destination: Path) -> None:
    """增加可见候选与固定历史样本，资源和源文件都实际保存到隔离工程。"""
    create_base(destination)
    original = json.loads((ROOT / 'game/data/presentation.json').read_text())
    manifest = json.loads((ROOT / 'tests/fixtures/character_presentation/manifest.json').read_text())
    for group in ('backgrounds', 'portraits'):
        manifest[group].update(original[group])
    (destination / 'game/data/presentation.json').write_text(json.dumps(manifest, ensure_ascii=False))
    for group in ('backgrounds', 'portraits'):
        for item in manifest[group].values():
            relative = item['path'].removeprefix('res://')
            target = destination / 'game' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / 'game' / relative, target)
    source = destination / 'content/foundation/lived_city.json'
    content = json.loads(source.read_text())
    features = dict(face_shape='round', eyes='calm', brows='straight', nose='plain',
                    mouth='plain', hair='short', beard='none', skin_palette='warm', build='slender')
    candidates = {key: [value] for key, value in features.items()}
    candidates.update(face_shape=['round', 'angular'], hair=['short', 'bun'], build=['slender', 'broad'])
    for actor in content['actors']:
        if actor['id'] not in ('actor.he', 'actor.chen'):
            continue
        actor['initial_age_years'] = 24
        actor['presentation_profile_id'] = 'fixture.person'
        actor['appearance_definition'] = dict(kind='generated', generator_version='appearance-v1',
                                              attire_style='plain', candidates=candidates)
        if actor['id'] == 'actor.chen':
            actor['origin'] = 'historical'
            actor['appearance_definition'] = dict(kind='fixed', generator_version='appearance-v1',
                                                  attire_style='plain', base_features=features)
    source.write_text(json.dumps(content, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    create(Path(sys.argv[1]))
