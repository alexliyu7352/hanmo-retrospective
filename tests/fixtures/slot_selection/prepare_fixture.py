"""在独立轻工程内正式组合两套tiny作者内容；只复制最小运行源码与音频，不修改共享资源。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys


def prepare(root, template, resources, output):
    """两份作者只改变显式包身份，经同一生产composer与安装层生成真实受信索引。"""
    if output.exists():
        raise ValueError('Use a new output directory')
    output.mkdir(parents=True)
    sys.path.insert(0, str(root))
    from tools.content.io import canonical
    from tools.content.pack_compose import compose_pack_set
    from tools.content_stage import installed_index_records, validate_stage
    from tools import build
    for name in ["tools", "schemas"]:
        shutil.copytree(root / name, output / name, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    game = output / 'game'
    game.mkdir()
    for name in ['core', 'ui', 'storage']:
        shutil.copytree(root / 'game' / name, game / name)
    (game / 'tests').mkdir()
    shutil.copy2(root / 'game/tests/test_slot_selection_menu.gd', game / 'tests/test_slot_selection_menu.gd')
    shutil.copytree(root / 'game/data', game / 'data', ignore=shutil.ignore_patterns('generated'))
    for name in ['ambience.wav', 'paper.wav', 'LICENSE.json']:
        target = game / 'assets/audio' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(resources / 'assets/audio' / name, target)
    sources = []
    for namespace in ['slotone', 'slottwo']:
        author = output / namespace
        shutil.copytree(template, author, ignore=shutil.ignore_patterns('*.import', '*.uid'))
        manifest = json.loads((author / 'pack.json').read_text())
        manifest['id'] = manifest['namespace'] = namespace
        (author / 'pack.json').write_bytes(canonical(manifest))
        composed = output / (namespace + '-composed')
        compose_pack_set([author], {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                                  'packs': [{'id': namespace, 'version': '1.0.0'}]}, composed)
        sources.append(composed)
    foundation = output / 'content/foundation'
    foundation.mkdir(parents=True)
    shutil.copy2(template / 'content.json', foundation / 'procurement.json')
    shutil.copy2(template / 'presentation.json', game / 'data/presentation.json')
    shutil.copytree(template / 'assets/art', game / 'assets/art', ignore=shutil.ignore_patterns('*.import'))
    build.stage_content(output, content_sets=sources, foundation_sets=False)
    generated = game / 'data/generated'
    installed_index_records(generated)
    validate_stage(output, build.discover_author_sources(output))
    entries = json.loads((generated / 'installed-content.json').read_text())['sets']
    text = (root / 'game/project.godot').read_text()
    text = re.sub(r'^enabled=PackedStringArray\(.*\)$', 'enabled=PackedStringArray()', text, flags=re.M)
    text = re.sub(r'^run/main_scene=.*\n', '', text, flags=re.M)
    text = text.replace('[application]', '[application]\nconfig/slot_menu_isolated=true')
    text = re.sub(r'^config/custom_user_dir_name=.*$', 'config/custom_user_dir_name="hanmo-slot-selection-tests"', text, flags=re.M)
    (game / 'project.godot').write_text(text)
    historical = root / 'tests/fixtures/saves/a2/v1-4f16935-family-after-adoption.hmsave'
    shutil.copy2(historical, game / 'tests/original-v1.hmsave')
    receipt = {'game': str(game), 'installed': entries,
               'source_sha256': {str(path.relative_to(game)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in game.rglob('*') if path.is_file()},
               'stage_entry': 'build.stage_content(output, content_sets=sources, foundation_sets=False)',
               'scope': '真实双内容集，真实Kernel生成v2；无OS视觉验收'}
    (output / 'prepare-receipt.json').write_bytes(canonical(receipt))
    print(json.dumps({'game': str(game), 'pack_set_digests': [row['pack_set_digest'] for row in entries]}, ensure_ascii=False))


def main():
    """显式要求输入和新输出位置，不猜测历史工程或复用正在运行的GUI目录。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root', type=Path, required=True)
    parser.add_argument('--pack-template', type=Path, required=True)
    parser.add_argument('--resources-game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.project_root.resolve(), args.pack_template.resolve(), args.resources_game.resolve(), args.output.absolute())


if __name__ == '__main__':
    main()
