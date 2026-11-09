"""准备剧本初态作者节点短门；沿提交内真实Editor源码，只复制少量作者JSON。"""
import argparse
from copy import deepcopy
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile


FIELDS = ('playable_actor_ids', 'start_date', 'enabled', 'initial')


def source_bytes(repository, commit, path):
    """读取精确Git对象，准备器不依赖旧工区或中央未提交字节。"""
    return subprocess.check_output(['git', '-C', str(repository), 'show', commit + ':' + path])


def write_json(path, value):
    """只写全新隔离工程的作者输入，保留可阅读原件供失败诊断。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def prepare(repository, source, commit, output):
    """物化精确生产源码和两个真实入口；不建替代协议、世界或资源导入入口。"""
    if output.exists() or not re.fullmatch(r'[0-9a-f]{7,40}', commit):
        raise ValueError('Require a fresh output and precise production commit')
    commit = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', '--verify', commit + '^{commit}'], text=True).strip()
    paths = ['tools', 'schemas', 'game/core', 'game/ui', 'game/addons', 'game/storage', 'game/data', 'game/project.godot']
    raw = subprocess.check_output(['git', '-C', str(repository), 'archive', commit, *paths])
    output.mkdir(parents=True)
    with tarfile.open(fileobj=BytesIO(raw), mode='r:') as archive:
        for member in archive:
            path = Path(member.name)
            if path.is_absolute() or '..' in path.parts or not (member.isdir() or member.isfile()):
                raise ValueError('Unexpected source member')
            target = output / path
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.extractfile(member).read())
    target = output / 'game/tests/test_editor_scenario_snapshot.gd'
    target.parent.mkdir(parents=True)
    shutil.copy2(source / 'game/tests/test_editor_scenario_snapshot.gd', target)
    settings = output / 'game/project.godot'
    text = settings.read_text()
    text = re.sub(r'^enabled=PackedStringArray\(.*\)$', 'enabled=PackedStringArray()', text, flags=re.M)
    text = re.sub(r'^run/main_scene=.*\n', '', text, flags=re.M)
    text = re.sub(r'^config/custom_user_dir_name=.*$', 'config/custom_user_dir_name="hanmo-author-snapshot-tests"', text, flags=re.M)
    text = text.replace('[application]', '[application]\nconfig/scenario_snapshot_isolated=true')
    settings.write_text(text)
    prefix = 'tests/fixtures/content_packs/compose/new_packs/base/'
    author = json.loads(source_bytes(repository, commit, prefix + 'content.json'))
    manifest = json.loads(source_bytes(repository, commit, prefix + 'pack.json'))
    manifest.update(id='snapshotauthor', namespace='snapshotauthor', content_entries=['content.json', 'people.json'],
                    presentation_entries=[], resources=[], license='NOASSERTION')
    appearance = {'kind': 'fixed', 'generator_version': 'appearance-v1', 'attire_style': 'plain',
                  'base_features': dict(zip(['face_shape', 'eyes', 'brows', 'nose', 'mouth', 'hair', 'beard', 'skin_palette', 'build'],
                                           ['round', 'calm', 'straight', 'plain', 'plain', 'short', 'none', 'warm', 'slender']))}
    for index, actor in enumerate(author['actors']):
        actor.update(initial_age_years=24 + index, appearance_definition=deepcopy(appearance),
                     initial_goods=[{'item_id': 'item.grain', 'quantity': index + 2}])
    # 把实际选中岗位对白缩成合法say/end，未选商店与委托仍完整保留在作者目录。
    author['dialogues'][0]['nodes'] = [{'id': 'entry', 'kind': 'say', 'speaker': '$provider',
                                       'text_key': 'office.hello', 'next': 'end'}, {'id': 'end', 'kind': 'end'}]
    selected = {'enabled': {'settlements': ['city.west'], 'places': ['west.street', 'west.office', 'west.grain'],
                            'actors': ['actor.player', 'actor.chen'], 'organizations': ['org.office'],
                            'roles': ['role.office'], 'wallets': ['wallet.player', 'wallet.office'],
                            'items': ['item.grain'], 'relations': ['relation.chen_player']},
                'playable_actor_ids': ['actor.player'], 'start_date': {'year': 208, 'day_of_year': 45, 'half_day': 1},
                'actor_options': {'actor.player': {'alive': True, 'available': True, 'copy_initial_goods': True},
                                  'actor.chen': {'alive': True, 'available': False, 'copy_initial_goods': False}},
                'role_options': {'role.office': {'enabled': False}}}
    initial = {key: [] for key in ['actors', 'offer_ids', 'wallets', 'inventories', 'roles', 'relations',
                                  'estates', 'garrisons', 'offices', 'strongholds']}
    for actor in author['actors'][:2]:
        row = {'actor_id': actor['id'], 'place_id': 'west.street', 'alive': True, 'available': True,
               'age_years': 67, 'attributes': deepcopy(actor['attributes']), 'skills': deepcopy(actor['skills']),
               'combat': {}, 'appearance_definition': deepcopy(appearance),
               'initial_goods': [{'item_id': 'item.grain', 'quantity': 1}]}
        row['attributes']['charisma'] = 17
        row['appearance_definition']['attire_style'] = 'formal'
        initial['actors'].append(row)
    initial['wallets'] = deepcopy(author['wallets'][:2])
    for row in initial['wallets']: row['initial_balance'] = 321
    initial['roles'] = [{'role_id': 'role.office', 'actor_id': 'actor.chen', 'enabled': True}]
    initial['relations'] = deepcopy(author['relations'])
    initial['relations'][0]['affinity'] = -2
    scenario = author['scenarios'][0]
    scenario.update(playable_actor_ids=['actor.player', 'actor.chen'],
                    start_date={'year': 190, 'day_of_year': 1, 'half_day': 0},
                    enabled=deepcopy(selected['enabled']), initial=initial)
    legacy = {key: value for key, value in scenario.items() if key not in FIELDS}
    legacy['id'] = 'scenario.legacy'
    author['scenarios'].append(legacy)
    people = {key: deepcopy(author[key]) for key in ['format', 'schema_version', 'id', 'version', 'required_capabilities']}
    people.update({key: [] for key, value in author.items() if isinstance(value, list) and key != 'required_capabilities'})
    people.update(actors=author['actors'][2:], texts={}, flags={})
    author['actors'] = author['actors'][:2]
    del people['actors'][0]['appearance_definition']
    del people['actors'][1]['initial_age_years']
    pack = output / 'test-inputs/pack-template'
    for name, value in [('pack.json', manifest), ('content.json', author), ('people.json', people)]:
        write_json(pack / name, value)
    identity = {path: hashlib.sha256((output / path).read_bytes()).hexdigest() for path in [
        'game/addons/hanmo_author/panel.gd', 'game/addons/hanmo_author/client.gd', 'tools/content/document.py',
        'tools/content/pack_document.py', 'tools/content/editor_protocol.py', 'tools/content/scenario_authoring.py']}
    fixture = {'production_commit': commit, 'source_sha256': identity, 'selection': selected,
               'scenario_id': scenario['id'], 'before': {key: deepcopy(scenario[key]) for key in FIELDS},
               'initial_fields': list(FIELDS), 'assets_copied': 0, 'runtime_verified': False}
    write_json(output / 'test-inputs/fixture.json', fixture)
    print(json.dumps({'project': str(output), 'production_commit': commit, 'author_entries': 2, 'assets_copied': 0}))


def main():
    """要求明确仓库、测试源码、生产提交和全新输出，不能覆盖既有工坊或证据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--test-source', type=Path, required=True)
    parser.add_argument('--production-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.repository.resolve(), args.test_source.resolve(), args.production_commit, args.output.absolute())


if __name__ == '__main__':
    main()
