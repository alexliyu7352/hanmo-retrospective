"""用独立作者副本准备动态人物调度目录，不修改生产内容、原型资产或生成目录。"""
import argparse
import json
from pathlib import Path

from tests.military_content_fixture import military_pack
from tools.content.io import prepare_pack

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = ('travel-enabled', 'travel-default', 'military')


def dynamic_actor_pack(variant: str = 'travel-enabled') -> dict:
    """复用已合法的作者入口；军事与行旅分开原型，未选择或未知配置不能隐式启用。"""
    if variant not in VARIANTS:
        raise ValueError('Unknown dynamic actor fixture: ' + variant)
    pack = military_pack()
    family = next(role['family'] for role in pack['roles'] if role['id'] == 'role.estate_steward')
    if variant == 'military':
        family['prototype_id'] = 'actor.military.a.commander'
        return pack
    for actor in pack['actors']:
        actor.pop('military_plan', None)
    prototype = next(actor for actor in pack['actors'] if actor['id'] == family['prototype_id'])
    prototype['travel_plan'] = {
        'destinations': ['city.west', 'village.east'], 'dwell_ticks': 2, 'start_tick': 5,
    }
    prototype['social'] = {
        'kind': 'cooperative', 'target_actor_id': 'actor.shen',
        'interval_ticks': 1, 'share_news': True, 'food_item_id': 'item.grain',
    }
    if variant == 'travel-enabled':
        family['enable_travel_plan'] = True
    return pack


def prepare_dynamic_actor_fixture(folder: Path) -> Path:
    """正式准备每个测试配置并返回清单；所有源和编译输出仅进入调用方的独立目录。"""
    folder = Path(folder).resolve()
    for protected in (ROOT / name for name in ('content', 'schemas', 'game', 'tools', 'tests')):
        if folder == protected.resolve() or protected.resolve() in folder.parents:
            raise ValueError('Dynamic fixture must be outside authored/source directories')
    folder.mkdir(parents=True, exist_ok=True)
    variants = {}
    for variant in VARIANTS:
        pack = dynamic_actor_pack(variant)
        source = folder / ('dynamic-actor-' + variant + '.json')
        source.write_text(json.dumps(pack, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        output = folder / ('dynamic-actor-' + variant + '.hmcatalog.json')
        receipt = prepare_pack(source, output)
        family = next(role['family'] for role in pack['roles'] if role['id'] == 'role.estate_steward')
        variants[variant.replace('-', '_')] = {
            'catalog': str(output), 'source': str(source),
            'catalog_sha256': receipt['catalog_sha256'],
            'scenario': 'scenario.alpha_life', 'role_id': 'role.estate_steward',
            'prototype_id': family['prototype_id'], 'place_id': 'west.estate',
            'guardian_actor_id': 'actor.player',
        }
    manifest = folder / 'dynamic-actor-fixtures.json'
    manifest.write_text(json.dumps(variants, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest


def main() -> int:
    """向验证工具输出本轮真实准备的清单路径，内容失败按正式准备器直接终止。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    print(prepare_dynamic_actor_fixture(args.folder))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
