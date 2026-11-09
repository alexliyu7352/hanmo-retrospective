"""用中央已提交tiny分层资源正式组合两代新锁；不生成图、不依赖旧工作树或改生产文件。"""
import argparse
from copy import deepcopy
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile

FEATURES = {'face_shape': 'round', 'eyes': 'calm', 'brows': 'straight', 'nose': 'plain',
            'mouth': 'plain', 'hair': 'short', 'beard': 'none', 'skin_palette': 'warm', 'build': 'slender'}
NAMESPACE = 'lockedlook'


def raw(repository, commit, path):
    """只读取精确提交的原始对象，旧工区清理不影响输入来源。"""
    return subprocess.check_output(['git', '-C', str(repository), 'show', commit + ':' + path])


def extract(repository, commit, paths, output):
    """仅展开明确源码成员，拒绝链接及非普通归档项。"""
    value = subprocess.check_output(['git', '-C', str(repository), 'archive', commit, *paths])
    with tarfile.open(fileobj=BytesIO(value), mode='r:') as archive:
        for member in archive:
            path = Path(member.name)
            if path.is_absolute() or '..' in path.parts or not (member.isdir() or member.isfile()):
                raise ValueError('Unexpected Git member')
            target = output / path
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.extractfile(member).read())


def assign(value, path, replacement):
    """仅按生产Schema walker给出的表现字段位置改为已有tiny资源，不猜普通字符串。"""
    for part in path[:-1]:
        value = value[part]
    value[path[-1]] = replacement


def author(journey, alpha, presentation, age, attire):
    """在真实作者定义增加一个可当面显示的路遇对手，初态显式给齐且行程只由正常命令启动。"""
    from tools.content.references import _iter_annotated
    source = deepcopy(journey)
    source['id'] = 'fixture.locked_appearance'
    source['required_capabilities'].append('personal_combat')
    bandit = deepcopy(next(a for a in alpha['actors'] if a['id'] == 'actor.bandit'))
    bandit['initial_place'] = 'west.street'
    bandit['dialogue_id'] = 'dialog.locked_person'
    source['actors'].append(bandit)
    source['wallets'].append(deepcopy(next(w for w in alpha['wallets'] if w['owner'].get('id') == bandit['id'])))
    sword = deepcopy(next(item for item in alpha['items'] if item['id'] == 'item.sword'))
    source['items'].append(sword)
    source['personal_battlefields'] = [deepcopy(next(f for f in alpha['personal_battlefields'] if f['id'] == 'field.ford'))]
    source['personal_objectives'] = [deepcopy(next(o for o in alpha['personal_objectives'] if o['id'] == 'objective.road'))]
    source['personal_skills'] = []
    for key in ['name.actor.bandit', 'name.item.sword', 'name.field.ford', 'name.objective.road']:
        source['texts'][key] = alpha['texts'][key]
    source['texts'].update({'name.role.locked_person': '在场旅人', 'locked.hello': '稍后沿路再会。',
                            'name.scenario.locked': '新锁人物身份节点夹具'})
    source['roles'].append({'id': 'role.locked_person', 'name_key': 'name.role.locked_person',
                            'organization_id': None, 'initial_actor': bandit['id'],
                            'place_id': 'west.street', 'services': ['information']})
    source['dialogues'].append({'id': 'dialog.locked_person', 'context_role': 'role.locked_person',
                                'entry': 'hello', 'nodes': [{'id': 'hello', 'kind': 'say',
                                'speaker': '$provider', 'text_key': 'locked.hello', 'next': 'end'},
                                {'id': 'end', 'kind': 'end'}]})
    for person in source['actors']:
        person.pop('travel_plan', None)
        person['initial_age_years'] = 77
        person['appearance_definition'] = {'kind': 'fixed', 'generator_version': 'appearance-v1',
                                           'base_features': deepcopy(FEATURES), 'attire_style': 'plain'}
        person['presentation_profile_id'] = 'fixture.person'
        if person['id'] == 'actor.player':
            person['combat'] = {'weapon_id': 'item.sword', 'learned': []}
            person['initial_goods'] = [{'item_id': 'item.sword', 'quantity': 1}]
    for path, group, _reference in _iter_annotated(source, annotation='x-hanmo-presentation'):
        if group == 'backgrounds': assign(source, path, presentation['default_background'])
        elif group == 'portraits': assign(source, path, presentation['default_portrait'])
    initial = {'actors': [], 'offer_ids': [row['id'] for row in source['offers']]}
    for person in source['actors']:
        definition = deepcopy(person['appearance_definition'])
        definition['attire_style'] = attire if person['id'] == bandit['id'] else 'plain'
        initial['actors'].append({'actor_id': person['id'], 'place_id': person['initial_place'],
                                  'alive': True, 'available': True,
                                  'age_years': age if person['id'] == bandit['id'] else 30,
                                  'attributes': deepcopy(person['attributes']), 'skills': deepcopy(person['skills']),
                                  'combat': deepcopy(person.get('combat', {})), 'appearance_definition': definition,
                                  'initial_goods': deepcopy(person.get('initial_goods', []))})
    for table in ['wallets', 'inventories', 'relations']:
        initial[table] = deepcopy(source[table])
    initial['roles'] = [{'role_id': row['id'], 'actor_id': row['initial_actor'], 'enabled': True} for row in source['roles']]
    scenario = {'id': 'scenario.locked', 'name_key': 'name.scenario.locked', 'player_actor': 'actor.player',
                'playable_actor_ids': ['actor.player'], 'seed_text': 'locked-appearance-bounded',
                'start_date': {'year': 184, 'day_of_year': 30, 'half_day': 0}, 'initial': initial}
    source['scenarios'] = [scenario]
    scenario['enabled'] = {name: [row['id'] for row in records] for name, records in source.items()
                           if isinstance(records, list) and name != 'required_capabilities'}
    scenario['enabled'].update(texts=sorted(source['texts']), flags=sorted(source['flags']))
    return source


def prepare(repository, test_source, commit, output):
    """每代经过生产compose与完整stage，记录精确锁及复用图像原SHA，测试工程只保留必要资源。"""
    if output.exists() or not re.fullmatch(r'[0-9a-f]{7,40}', commit):
        raise ValueError('Require a fresh output and precise commit')
    commit = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', '--verify', commit + '^{commit}'], text=True).strip()
    output.mkdir(parents=True)
    project = output / 'project'
    extract(repository, commit, ['tools', 'schemas', 'game/core', 'game/ui', 'game/storage', 'game/data',
                                'game/project.godot', 'game/assets/audio/ambience.wav',
                                'game/assets/audio/paper.wav', 'game/assets/audio/LICENSE.json'], project)
    (project / 'game/tests').mkdir()
    shutil.copy2(test_source / 'game/tests/test_pack_appearance_identity.gd', project / 'game/tests/test_pack_appearance_identity.gd')
    sys.path.insert(0, str(project))
    from tools.content.io import canonical
    from tools.content.pack_compose import compose_pack_set
    from tools import build, content_stage
    presentation = json.loads(raw(repository, commit, 'tests/fixtures/character_presentation/manifest.json'))
    journey = json.loads(raw(repository, commit, 'content/foundation/journey_life.json'))
    alpha = json.loads(raw(repository, commit, 'content/foundation/alpha_life.json'))
    resources, originals = [], {}
    for path in sorted({row['path'].removeprefix('res://') for group in ['backgrounds', 'portraits'] for row in presentation[group].values()}):
        value = raw(repository, commit, 'game/' + path)
        originals[path] = value
        target = project / 'game' / path
        target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(value)
        resources.append({'path': path, 'kind': 'image', 'required': True, 'sha256': hashlib.sha256(value).hexdigest(),
                          'license': 'project-local-authorized-use'})
    (project / 'game/data/presentation.json').write_bytes(canonical(presentation))
    variants, composed = [], []
    for name, version, age, attire in [('adult_plain', '1.0.0', 24, 'plain'), ('child_formal', '1.0.1', 12, 'formal')]:
        source = author(journey, alpha, presentation, age, attire)
        pack = output / name
        pack.mkdir()
        (pack / 'content.json').write_bytes(canonical(source))
        (pack / 'presentation.json').write_bytes(canonical(presentation))
        for path, value in originals.items():
            target = pack / path
            target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(value)
        manifest = {'format': 'HM-PACK', 'format_version': 1, 'id': NAMESPACE, 'namespace': NAMESPACE, 'version': version,
                    'rules_api': {'id': 'hanmo.rules', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'},
                    'dependencies': [], 'conflicts': [], 'required_capabilities': source['required_capabilities'],
                    'content_entries': ['content.json'], 'presentation_entries': ['presentation.json'],
                    'resources': resources, 'author': '新锁身份节点fixture', 'license': 'NOASSERTION'}
        (pack / 'pack.json').write_bytes(canonical(manifest))
        destination = output / (name + '-composed')
        result = compose_pack_set([pack], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                                          'packs': [{'id': NAMESPACE, 'version': version}]}, destination)
        composed.append(destination)
        variants.append({'id': name, 'version': version, 'age': age, 'attire': attire,
                         'pack_set_digest': result['pack_set_digest'], 'scenario': NAMESPACE + '::scenario.locked',
                         'actor_id': NAMESPACE + '::actor.bandit', 'player_id': NAMESPACE + '::actor.player'})
        if name == 'adult_plain':
            foundation = project / 'content/foundation'
            foundation.mkdir(parents=True)
            (foundation / 'locked_appearance.json').write_bytes(canonical(source))
    build.stage_content(project, content_sets=composed, foundation_sets=False)
    content_stage.validate_stage(project, build.discover_author_sources(project))
    settings = project / 'game/project.godot'
    text = settings.read_text()
    text = re.sub(r'^enabled=PackedStringArray\(.*\)$', 'enabled=PackedStringArray()', text, flags=re.M)
    text = re.sub(r'^run/main_scene=.*\n', '', text, flags=re.M)
    text = re.sub(r'^config/custom_user_dir_name=.*$', 'config/custom_user_dir_name="hanmo-locked-appearance-tests"', text, flags=re.M)
    text = text.replace('[application]', '[application]\nconfig/locked_appearance_isolated=true')
    settings.write_text(text)
    fixture = {'format': 'HM-LOCKED-APPEARANCE-FIXTURE', 'source_commit': commit, 'namespace': NAMESPACE,
               'variants': variants, 'base_features': FEATURES, 'resources': resources,
               'advance_budget_per_leg': 4, 'scope': 'existing tiny fixture identity/node contract; no OS visual acceptance or natural age advancement'}
    (project / 'fixture.json').write_bytes(canonical(fixture))
    print(json.dumps({'project': str(project), 'locks': [row['pack_set_digest'] for row in variants],
                      'resource_count': len(resources), 'resource_bytes': sum(len(v) for v in originals.values())}))


def main():
    """显式选择中央精确提交和新的隔离目录，不接管现有运行或作者工程。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--test-source', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.repository.resolve(), args.test_source.resolve(), args.source_commit, args.output.absolute())


if __name__ == '__main__':
    main()
