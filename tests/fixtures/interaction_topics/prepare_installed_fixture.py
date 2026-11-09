"""用冻结作者、真实既有美术和正式compose/stage生成独立M4安装工程，不改生产默认或原作者。"""
import argparse
from copy import deepcopy
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile



def sha(raw):
    """原字节摘要用于冻结输入与记录正式生成代，不从名字推测内容身份。"""
    return hashlib.sha256(raw).hexdigest()


def revision(repository, value):
    """只解析明确Git提交，不执行调用方提供的命令或模糊分支选择。"""
    if not re.fullmatch(r'[0-9a-f]{7,40}', value):
        raise ValueError('A precise Git commit is required')
    return subprocess.check_output(['git', '-C', str(repository), 'rev-parse', '--verify', value + '^{commit}'], text=True).strip()


def extract(repository, commit, paths, destination):
    """仅物化指定Git源码路径，拒绝链接和非正规成员，不复制仓库或生成大资源。"""
    raw = subprocess.check_output(['git', '-C', str(repository), 'archive', commit, *paths])
    with tarfile.open(fileobj=BytesIO(raw), mode='r:') as archive:
        for member in archive:
            relative = Path(member.name)
            if relative.is_absolute() or '..' in relative.parts or not (member.isdir() or member.isfile()):
                raise ValueError('Unexpected Git snapshot member: ' + member.name)
            target = destination / relative
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.extractfile(member).read())


def configure_game(path, catalog, scenario, profile):
    """只配置隔离工程的实际安装入口和持久测试profile，不修改UI或存储规则。"""
    value = path.read_text()
    updates = {'config/default_catalog': catalog, 'config/default_scenario': scenario,
               'config/storage_profile': profile, 'config/custom_user_dir_name': 'hanmo-m4-installed',
               'config/name': '汉末行记 · M4双岗位安装夹具'}
    for key, new_value in updates.items():
        value, count = re.subn(r'^' + re.escape(key) + r'=.*$', key + '=' + json.dumps(new_value, ensure_ascii=False), value, flags=re.MULTILINE)
        if count != 1:
            raise ValueError('Missing isolated project setting: ' + key)
    value = re.sub(r'^enabled=PackedStringArray\([^\n]*\)$', 'enabled=PackedStringArray()', value, flags=re.MULTILINE)
    path.write_text(value)


def main():
    """新建轻工程，按typed refs选择真实美术，再经生产入口发布唯一a2demo包并记录确切锁。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--resources-game', type=Path, required=True)
    parser.add_argument('--author', type=Path, required=True)
    parser.add_argument('--expected-author-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tools-commit', default='ffeefef')
    parser.add_argument('--role-schema-commit', default='7fc4297')
    parser.add_argument('--runtime-commit', '--runtime-ref', dest='runtime_commit', default='f7bbeb2')
    parser.add_argument('--namespace', default='a2demo')
    parser.add_argument('--scenario', default='scenario.two_stories')
    parser.add_argument('--storage-profile', default='m4-installed-interaction')
    args = parser.parse_args()
    repository, resources_game = args.repository.resolve(), args.resources_game.resolve()
    output = args.output.absolute()
    if output.exists():
        raise ValueError('Fixture output already exists; choose a fresh explicit directory')
    if output.resolve().is_relative_to(repository):
        raise ValueError('Fixture output must be outside the source repository')
    if not re.fullmatch(r'[a-z][a-z0-9_-]{0,31}', args.namespace):
        raise ValueError('Invalid explicit fixture namespace')
    author_raw = args.author.read_bytes()
    if sha(author_raw) != args.expected_author_sha256:
        raise ValueError('Frozen author bytes changed')
    commits = {name: revision(repository, value) for name, value in [
        ('tools', args.tools_commit), ('role_schema', args.role_schema_commit), ('runtime', args.runtime_commit)]}
    output.mkdir(parents=True)
    project = output / 'project'
    extract(repository, commits['tools'], ['tools', 'schemas', 'game/data'], project)
    extract(repository, commits['role_schema'], ['schemas/content.schema.json', 'tools/content/links.py'], project)
    extract(repository, commits['runtime'], ['game/core', 'game/ui', 'game/storage', 'game/addons',
                                           'game/project.godot', 'game/main.tscn',
                                           'game/assets/audio/ambience.wav', 'game/assets/audio/paper.wav',
                                           'game/assets/audio/LICENSE.json'], project)
    sys.path.insert(0, str(project))
    from tools import build, content_stage
    from tools.content.io import canonical, read_pack
    from tools.content.pack_compose import compose_pack_set, _annotated_schema
    from tools.content.references import _iter_annotated
    from tools.presentation import _asset_path, read_registry, validate_registry
    source = read_pack(args.author)
    if args.scenario not in {row['id'] for row in source['scenarios']}:
        raise ValueError('Requested scenario is absent from the frozen author')
    registry = read_registry(resources_game / 'data/presentation.json')
    presentation_schema = read_registry(project / 'game/data/presentation.schema.json')
    wanted = {'backgrounds': {registry['default_background']}, 'portraits': {registry['default_portrait']}}
    for _path, group, reference in _iter_annotated(source, annotation='x-hanmo-presentation'):
        if group in wanted and reference:
            wanted[group].add(reference)
    # 扩展表现组保留，追随其正式Schema引用补齐位图；不按ID名称猜测图像用途。
    for _path, annotation, group, reference in _annotated_schema(registry, presentation_schema, presentation_schema):
        if annotation == 'x-hanmo-asset-ref':
            group, reference = reference.split('/', 1)
        if annotation in ('x-hanmo-ref', 'x-hanmo-asset-ref') and group in wanted:
            wanted[group].add(reference)
    selected = deepcopy(registry)
    for group in wanted:
        selected[group] = {identifier: registry[group][identifier] for identifier in sorted(wanted[group])}
    pack_root = output / 'author-pack'
    pack_root.mkdir()
    (pack_root / 'content.json').write_bytes(author_raw)
    (pack_root / 'presentation.json').write_bytes(canonical(selected))
    (project / 'game/data/presentation.json').write_bytes(canonical(selected))
    resources, snapshots = [], {}
    for logical in sorted({record['path'].removeprefix('res://') for group in wanted for record in selected[group].values()}):
        original = _asset_path(resources_game, 'res://' + logical)
        raw = original.read_bytes(); snapshots[original] = sha(raw)
        for target in (pack_root / logical, project / 'game' / logical):
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(raw)
        resources.append({'path': logical, 'kind': 'image', 'required': True, 'sha256': sha(raw), 'license': 'NOASSERTION'})
    issues = validate_registry(selected, project / 'game')
    if issues:
        raise ValueError('Real presentation fixture is invalid: ' + json.dumps(issues, ensure_ascii=False))
    manifest = {'format': 'HM-PACK', 'format_version': 1, 'id': args.namespace, 'namespace': args.namespace,
                'version': '1.0.0', 'rules_api': {'id': 'hanmo.rules', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'},
                'dependencies': [], 'conflicts': [], 'required_capabilities': source['required_capabilities'],
                'content_entries': ['content.json'], 'presentation_entries': ['presentation.json'],
                'resources': resources, 'author': 'Hanmo M4 installed interaction fixture', 'license': 'NOASSERTION'}
    (pack_root / 'pack.json').write_bytes(canonical(manifest))
    selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                 'packs': [{'id': args.namespace, 'version': '1.0.0'}]}
    (output / 'selection.json').write_bytes(canonical(selection))
    composed = output / 'composed'
    result = compose_pack_set([pack_root], selection, composed)
    authors = project / 'content/foundation'; authors.mkdir(parents=True)
    (authors / 'interaction_topics.json').write_bytes(author_raw)
    # 本夹具只安装本次新包，明确排除无关六套历史大资源；生产stage默认保持True。
    build.stage_content(project, content_sets=[composed], foundation_sets=False)
    installed = project / 'game/data/generated'
    content_stage.validate_stage(project, build.discover_author_sources(project))
    identity = result['pack_set_digest']
    catalog = 'res://data/generated/content-sets/' + identity + '/catalog.hmcatalog.json'
    configure_game(project / 'game/project.godot', catalog, args.namespace + '::' + args.scenario, args.storage_profile)
    if args.author.read_bytes() != author_raw or any(sha(path.read_bytes()) != digest for path, digest in snapshots.items()):
        raise ValueError('Frozen fixture author or real resources changed during preparation')
    source_manifest = {'format': 'HM-M4-FIXTURE-SOURCE-MANIFEST', 'format_version': 1,
                       'commits': commits, 'files': [
                           {'path': path.relative_to(project).as_posix(), 'sha256': sha(path.read_bytes())}
                           for path in build.source_files(project)]}
    source_manifest_raw = canonical(source_manifest)
    (output / 'source-manifest.json').write_bytes(source_manifest_raw)
    receipt = {'format': 'HM-M4-INSTALLED-FIXTURE', 'commits': commits, 'project': str(project),
               'game': str(project / 'game'), 'author': str(args.author.resolve()), 'author_sha256': sha(author_raw),
               'schema_sha256': sha((project / 'schemas/content.schema.json').read_bytes()),
               'manifest_sha256': sha((pack_root / 'pack.json').read_bytes()),
               'presentation_source_sha256': sha((pack_root / 'presentation.json').read_bytes()),
               'pack_set_digest': identity, 'catalog_sha256': result['catalog_sha256'],
               'catalog_file_sha256': sha((composed / 'catalog.hmcatalog.json').read_bytes()),
               'installed_index_sha256': sha((installed / 'installed-content.json').read_bytes()),
               'source_manifest_sha256': sha(source_manifest_raw),
               'preparer_sha256': sha(Path(__file__).read_bytes()),
               'scenario': args.namespace + '::' + args.scenario, 'storage_profile': args.storage_profile,
               'foundation_sets': False, 'runtime_verified': False, 'resources': resources,
               'resource_bytes': sum((pack_root / row['path']).stat().st_size for row in resources)}
    (output / 'prepare-installed-receipt.json').write_bytes(canonical(receipt))
    print(json.dumps(receipt, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
