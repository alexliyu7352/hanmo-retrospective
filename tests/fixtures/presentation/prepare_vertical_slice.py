"""隔离准备V0.5候选人物样板：正式作者保存、重开及prepare，不在运行时注入清单或世界。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = Path(__file__).with_name('vertical_slice.json')


def digest(path: Path) -> str:
    """绑定已落盘文件字节，不使用文件名充当验证身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value) -> None:
    """写本次隔离产物，调用者负责只提供新建任务目录。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n')


def raster_geometry(root: Path, manifest_value: dict, asset_id: str, view: str, pose: str,
                    logical_canvas: list | None = None, safe_crop: list | None = None) -> dict:
    """从已登记原图读取像素边界；只生成几何元数据，不裁切、重画或修改图像字节。"""
    group, key = asset_id.split('/')
    path = root / 'game' / manifest_value[group][key]['path'].removeprefix('res://')
    with Image.open(path) as image:
        width, height = image.size
        bounds = image.getbbox()
    return {'source_size': [width, height], 'logical_canvas': logical_canvas or [width, height],
            'alpha_bounds': [bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]],
            'safe_crop': safe_crop or [0, 0, width, height],
            'support_kind': 'half_body_bottom' if view == 'half_body' else 'foot',
            'support_point': [50, 100], 'pose': pose, 'facing': 'front', 'view': view,
            'mask_ids': [], 'palette_slots': []}


def bind_representative_art(root: Path, value: dict, spec: dict) -> None:
    """仅在纵切片绑定既有专绘及成人静态战斗图；儿童战斗原夹具保留并明确限界。"""
    dedicated = deepcopy(value['character_profiles']['fixture.dedicated.a'])
    dedicated.update(id=spec['dedicated']['profile'], age_domain=spec['dedicated']['age_domain'])
    dedicated['age_variants'] = [deepcopy(dedicated['age_variants'][1])]
    dedicated['age_variants'][0]['ages'] = spec['dedicated']['age_domain']
    portrait = deepcopy(dedicated['overrides'][1])
    portrait.update(id='slice.shen.sender', ages=spec['dedicated']['age_domain'], asset_id=spec['dedicated']['asset_id'])
    portrait['geometry'] = raster_geometry(root, value, portrait['asset_id'], 'half_body', 'idle',
                                          safe_crop=spec['dedicated']['safe_crop'])
    dedicated['overrides'] = [portrait]
    value['character_profiles'][dedicated['id']] = dedicated
    battle = deepcopy(portrait)
    battle.update(id='slice.adult.swordsman', ages=spec['adult_battle']['ages'],
                  asset_id=spec['adult_battle']['asset_id'], view='battle', pose='combat', context='battle')
    battle['geometry'] = raster_geometry(root, value, battle['asset_id'], 'battle', 'combat')
    value['character_profiles']['fixture.battle']['overrides'] = [battle]


def manifest(root: Path, spec: dict, representatives: bool = False) -> dict:
    """接入冻结人物、已登记代表资源及保留的建筑结构夹具，不修改生产清单。"""
    value = json.loads((root / 'tests/fixtures/character_presentation/manifest.json').read_text())
    art = spec['character_art']
    candidate = json.loads((root / art['manifest']).read_text())
    for group in ('portraits', 'components'):
        value[group].update(candidate[group])
    previous = value['character_profiles'][art['target_profile']]
    profile = deepcopy(candidate['character_profiles'][art['source_profile']])
    # 沿用作者身份、服饰语义及battle引用；成人有限战斗覆盖在同一隔离清单声明。
    for field in ('id', 'identity_group', 'battle_profile'):
        profile[field] = deepcopy(previous[field])
    for authored, supplied in art['attire_aliases'].items():
        attire = deepcopy(next(row for row in profile['attire_sets'] if row['id'] == supplied))
        attire['id'] = authored
        profile['attire_sets'].append(attire)
    value['character_profiles'][art['target_profile']] = profile
    original = json.loads((root / 'game/data/presentation.json').read_text())
    for group in ('backgrounds', 'portraits'):
        value[group].update(original[group])
    bind_representative_art(root, value, spec['representative_art'])
    for level in (1, 2):
        value['backgrounds'][f'fixture.granary.{level}'] = {'path': f'res://assets/art/presentation-fixtures/granary-{level}.png'}
    template = json.loads((root / 'tests/fixtures/presentation/layered-v2.json').read_text())['scene_templates']['fixture.room']
    variants = spec['variants'] + (spec['representative_variants'] if representatives else [])
    for variant in variants:
        for kind in ('world', 'city', 'interior'):
            scene = deepcopy(template)
            scene.update(id=f'slice.{variant["id"]}.{kind}', view_kind=kind)
            scene['layers'] = scene['layers'][:1]
            scene['layers'][0]['anchor'] = [50, 100]
            background = spec['representative_art']['backgrounds'][kind]
            scene['layers'][0]['asset_id'] = background
            geometry = raster_geometry(root, value, background, 'scene', 'idle')
            scene['logical_canvas'] = geometry['logical_canvas']
            scene['layers'][0]['geometry'] = geometry
            scene['ui_safe_areas'] = [[0, 0, 1, 1]]
            for actor in ('actor.player', 'actor.carter', 'actor.shen'):
                scene['layers'].append({'id': actor, 'kind': 'actor', 'position': variant['actor_positions'][actor],
                    'extent': spec['actor_extent'], 'target_ref': {'kind': 'actor', 'id': actor}, 'after': ['granary.1', 'granary.2']})
            for level in (1, 2):
                layer = deepcopy(scene['layers'][0])
                layer.update(id=f'granary.{level}', kind='building', after=['back'],
                    asset_id=f'backgrounds/fixture.granary.{level}',
                    target_ref={'kind': 'estate', 'id': 'estate.riverside'},
                    visible_if=[{'field': 'facilities.granary', 'equals': level}])
                layer['geometry']['source_size'] = [10, 10]
                layer['geometry']['alpha_bounds'] = [6, 4-level, 3, level+1]
                layer['geometry']['safe_crop'] = [6, 4-level, 3, level+1]
                scene['layers'].append(layer)
                scene['layers'].append({'id': f'build.{level}', 'kind': 'hotspot',
                    'rect': [60, (4-level)*10, 30, (level+1)*10],
                    'target_layer_id': layer['id'], 'after': [layer['id']],
                    'candidate_selector': {'command_kind': 'estate_build', 'params': {'facility': 'granary'}}})
            value['scene_templates'][scene['id']] = scene
    return value


def write_building_fixtures(project: Path) -> None:
    """只在新建样板工程生成透明几何建筑；两等级有不同高度，不能充当生产美术。"""
    folder = project / 'game/assets/art/presentation-fixtures'
    folder.mkdir(parents=True, exist_ok=True)
    for level in (1, 2):
        image = Image.new('RGBA', (10, 10), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        draw.rectangle((6, 4-level, 8, 4), fill=(184, 139, 77, 255))
        draw.line((6, 4-level, 8, 4-level), fill=(91, 56, 34, 255))
        image.save(folder / f'granary-{level}.png')
    (folder / 'SLICE_BUILDING_PROVENANCE.md').write_text(
        '# 纵切片几何建筑测试资源\n\n'
        '由prepare_vertical_slice.py以Pillow矩形生成两个10×10透明PNG，仓廪等级仅以矩形高度区别。'
        '无第三方素材，不是生产建筑美术；只在本次隔离样板工程生成。\n', encoding='utf-8')


def author_age_boundary(project: Path, spec: dict) -> dict:
    """通过原作者API保存重开最小既有剧本，合法15岁初态仍需正常720半日才跨界。"""
    from tools.content.document import AuthorDocument
    age = spec['age_boundary']
    doc = AuthorDocument.open(project / age['source'])
    actor_id = age['actor_id']
    fields = {'initial_age_years': age['initial_age_years'], 'presentation_profile_id': spec['character_art']['target_profile'],
              'appearance_definition': {'kind': 'fixed', 'generator_version': 'appearance-v1',
                  'base_features': deepcopy(spec['base_features']), 'attire_style': 'plain'}}
    for field, value in fields.items():
        if field in doc.record('actors', actor_id):
            doc.set_value('actors', actor_id, '/' + field, value)
        else:
            doc.add_field('actors', actor_id, field, value)
    expected = doc.snapshot()
    authored = project / 'authored/age_boundary.json'
    saved = doc.save(authored)
    reopened = AuthorDocument.open(authored)
    if reopened.snapshot() != expected or reopened.status()['dirty']:
        raise ValueError('年龄样板保存重开后作者内容不一致')
    catalog = project / 'prepared/age_boundary.hmcatalog.json'
    prepared = reopened.prepare(catalog)
    return {**age, 'author_path': str(authored), 'author_sha256': digest(authored),
            'catalog_path': str(catalog), 'catalog_sha256': digest(catalog), 'saved': saved['status'],
            'reopened_equal': True, 'prepared': prepared['status']}


def author_variants(project: Path, representatives: bool = False) -> dict:
    """在隔离工具进程中走生产AuthorDocument流程，未保存/重开/prepare不生成成功回执。"""
    sys.path.insert(0, str(project))
    from tools.content.document import AuthorDocument
    spec = json.loads(FIXTURE.read_text())
    source = project / spec['source']
    results = []
    variants = spec['variants'] + (spec['representative_variants'] if representatives else [])
    for variant in variants:
        doc = AuthorDocument.open(source)
        for actor, options in spec['actors'].items():
            appearance = {'kind': 'fixed', 'generator_version': 'appearance-v1',
                          'base_features': deepcopy(spec['base_features']), 'attire_style': 'plain'}
            if 'generated_candidates' in options:
                appearance['kind'] = 'generated'
                appearance['candidates'] = {key: options['generated_candidates'].get(key, [value])
                                            for key, value in appearance.pop('base_features').items()}
            if actor == variant['actor_id']:
                appearance['base_features']['face_shape'] = variant['face_shape']
                appearance['attire_style'] = variant['attire_style']
            for field, value in {'appearance_definition': appearance, 'presentation_profile_id': options['profile'],
                                 'initial_age_years': options['age']}.items():
                if field in doc.record('actors', actor):
                    doc.set_value('actors', actor, '/' + field, value)
                else:
                    doc.add_field('actors', actor, field, value)
        for place in spec['places']:
            value = {kind: f'slice.{variant["id"]}.{kind}' for kind in ('city', 'interior')}
            if 'scene_template_ids' in doc.record('places', place):
                doc.set_value('places', place, '/scene_template_ids', value)
            else:
                doc.add_field('places', place, 'scene_template_ids', value)
        doc.add_field('world_maps', spec['world_map_id'], 'scene_template_id', f'slice.{variant["id"]}.world')
        expected = doc.snapshot()
        authored = project / f'authored/{variant["id"]}.json'
        saved = doc.save(authored)
        reopened = AuthorDocument.open(authored)
        if reopened.snapshot() != expected or reopened.status()['dirty']:
            raise ValueError('保存重开后的完整作者内容不一致')
        catalog = project / f'prepared/{variant["id"]}.hmcatalog.json'
        prepared = reopened.prepare(catalog)
        results.append({'id': variant['id'], 'author_path': str(authored), 'author_sha256': digest(authored),
            'catalog_path': str(catalog), 'catalog_sha256': digest(catalog), 'saved': saved['status'],
            'reopened_equal': True, 'prepared': prepared['status']})
    generated = project / 'game/data/generated'
    generated.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(project / 'schemas/content.schema.json', generated / 'content.schema.json')
    shutil.copyfile(Path(results[0]['catalog_path']), generated / 'alpha_life.hmcatalog.json')
    return {'format': 'HM-PRESENTATION-SLICE-PREPARE/1', 'asset_scope': spec['asset_scope'],
            'manifest_sha256': digest(project / 'game/data/presentation.json'),
            'representative_art': deepcopy(spec['representative_art']),
            'age_transition': deepcopy(spec['age_transition']),
            'clothing_pair': spec['clothing_pair'] if representatives else [],
            'age_boundary': author_age_boundary(project, spec) if representatives else {},
            'character_art': {**spec['character_art'],
                'manifest_sha256': digest(project / spec['character_art']['manifest'])}, 'variants': results}


def prepare(destination: Path, root: Path = ROOT, representatives: bool = False) -> dict:
    """新建独立工程并复制正式代码/资源；源仓和生产清单永不写入。"""
    destination = Path(destination).resolve()
    for relative in ('game', 'tools', 'schemas', 'tests/fixtures', 'content'):
        protected = (root / relative).resolve()
        if destination == protected or protected in destination.parents:
            raise ValueError('样板输出不能位于被复制的源码目录内')
    destination.mkdir(parents=True, exist_ok=False)
    for relative in ('game', 'tools', 'schemas', 'tests/fixtures'):
        shutil.copytree(root / relative, destination / relative,
            ignore=shutil.ignore_patterns('.godot', '__pycache__', '*.pyc', 'godot', '.pytest_cache'))
    spec = json.loads((root / 'tests/fixtures/presentation/vertical_slice.json').read_text())
    source = destination / spec['source']
    source.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / spec['source'], source)
    if representatives:
        age_source = destination / spec['age_boundary']['source']
        age_source.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / spec['age_boundary']['source'], age_source)
    write_building_fixtures(destination)
    write_json(destination / 'game/data/presentation.json', manifest(root, spec, representatives))
    worker = destination / 'tests/fixtures/presentation/prepare_vertical_slice.py'
    command = [sys.executable, str(worker), '--author', str(destination)]
    if representatives:
        command.append('--representatives')
    completed = subprocess.run(command,
                               cwd=destination, capture_output=True, text=True, timeout=90)
    (destination / 'prepare.log').write_text(completed.stdout + completed.stderr)
    if completed.returncode:
        raise ValueError('正式作者prepare失败: ' + completed.stdout + completed.stderr)
    return json.loads((destination / 'prepared.json').read_text())


def main() -> None:
    """提供独立准备入口和隔离作者子进程入口；失败保留诊断而不打印PASS。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--author', action='store_true')
    parser.add_argument('--representatives', action='store_true', help='显式额外准备同脸换衣两配置；默认仍仅a/b')
    args = parser.parse_args()
    result = author_variants(args.destination, args.representatives) if args.author else prepare(args.destination, representatives=args.representatives)
    if args.author:
        write_json(args.destination / 'prepared.json', result)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
