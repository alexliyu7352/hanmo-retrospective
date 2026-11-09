"""复用既有军事作者生成器与正式compose；资源从已冻结Main包取实物，不造旧tiny素材或oracle。"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil


def sha(path):
    """只按实际资源字节登记新的fixture身份。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """所有输出限于一个新工区，原来源目录与锁定cache只读。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--recovered', type=Path, required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.absolute()
    if output.exists():
        raise ValueError('Use a fresh preparation root')
    source = output / 'source'
    source.mkdir(parents=True)
    for name in ['tools', 'schemas', 'game', 'content']:
        (source / name).symlink_to(args.source.resolve() / name, target_is_directory=True)
    (source / '.git').symlink_to(args.recovered.resolve() / '.git')
    for name in ['tests/military_content_fixture.py', 'tests/fixtures/military_march_kernel/prepare_fixture.py', 'tests/fixtures/content_packs/compose/new_packs/base/pack.json']:
        destination = source / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.recovered / name, destination)
    art = output / 'art'
    art.mkdir()
    presentation = json.loads((args.resources / 'data/presentation.json').read_text())
    for filename, table, key in [('pixel.png', 'backgrounds', 'default_background'), ('portrait.png', 'portraits', 'default_portrait')]:
        path = presentation[table][presentation[key]]['path']
        shutil.copy2(args.resources / path.removeprefix('res://'), art / filename)
    hashes = {name: sha(art / name) for name in ['pixel.png', 'portrait.png']}
    manifest_path = source / 'tests/fixtures/content_packs/compose/new_packs/base/pack.json'
    manifest = json.loads(manifest_path.read_text())
    for resource in manifest['resources']:
        resource['sha256'] = hashes[Path(resource['path']).name]
        resource['license'] = 'LicenseRef-Hanmo-Project-Assets'
    manifest['license'] = 'LicenseRef-Hanmo-Project-Assets'
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
    spec = importlib.util.spec_from_file_location('military_joint_existing_march', source / 'tests/fixtures/military_march_kernel/prepare_fixture.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ART_HASHES = hashes
    original_refresh = module.refresh
    def refresh_current(root, target):
        """旧夹具准备器补读当前正式编译器必需的作者外观清单。"""
        original_refresh(root, target)
        for name in ["adult_appearance_compositions.json", "accepted_adult_appearances.json"]:
            shutil.copy2(args.source / "game/data" / name, target / "game/data" / name)
    module.refresh = refresh_current
    module.prepare(source, output / 'stage', art, False)
    game = output / 'stage/game'
    for folder in ['ui', 'storage', 'tests']:
        shutil.copytree(args.source / 'game' / folder, game / folder, dirs_exist_ok=True)
    for folder in ['audio', 'fonts']:
        if (args.resources / 'assets' / folder).exists():
            shutil.copytree(args.resources / 'assets' / folder, game / 'assets' / folder, dirs_exist_ok=True)
    shutil.copy2(args.recovered / 'game/tests/test_military_preview_joint.gd', game / 'tests/test_military_preview_joint.gd')
    print(json.dumps({'game':str(game), 'actual_resource_sha256':hashes, 'scope':'new actual-author military parity fixture; not old tiny recovery'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
