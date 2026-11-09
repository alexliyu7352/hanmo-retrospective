"""为真实Godot面板与Python作者进程创建同一个隔离工程清单。"""
import json
from pathlib import Path
import shutil
import sys


def create(destination):
    """复制真实管线、能力输入和引用资源；源工程不写入测试别名。"""
    root = Path(__file__).resolve().parents[3]
    destination = Path(destination); destination.mkdir(parents=True, exist_ok=False)
    for relative in ('tools', 'schemas', 'game/core'):
        shutil.copytree(root / relative, destination / relative,
                        ignore=shutil.ignore_patterns('__pycache__', 'godot', 'fonts', 'art-models'))
    data = destination / 'game/data'; data.mkdir(parents=True)
    for name in ('rule_capabilities.json', 'module_contracts.json', 'presentation.schema.json'):
        shutil.copyfile(root / 'game/data' / name, data / name)
    original = json.loads((root / 'game/data/presentation.json').read_text())
    fixture = json.loads((Path(__file__).parent / 'layered-v2.json').read_text())
    for group in ('backgrounds', 'portraits'): fixture[group].update(original[group])
    fixture['backgrounds']['study'] = dict(original['backgrounds']['hall'], entrance=[32, 46])
    fixture['portraits']['scholar'] = dict(original['portraits']['official'])
    (data / 'presentation.json').write_text(json.dumps(fixture))
    for group in ('backgrounds', 'portraits'):
        for entry in fixture[group].values():
            relative = entry['path'][6:]; target = destination / 'game' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(root / 'game' / relative, target)
    folder = destination / 'content/foundation'; folder.mkdir(parents=True)
    shutil.copyfile(root / 'content/foundation/lived_city.json', folder / 'lived_city.json')
    print(destination)


if __name__ == '__main__':
    create(sys.argv[1])
