"""按精确源码提交准备独立标准Alpha；复用正式stage，不修改共享工程或旧安装代。"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys


def sha(raw):
    """绑定实际字节，不以旧目录名称充当本次来源。"""
    return hashlib.sha256(raw).hexdigest()


def main():
    """仅物化运行和正常内容编译所需文件；缺对象如实失败，不补造资源或放宽身份。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--resource-root', type=Path, action='append', default=[])
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', args.source_commit + '^{commit}'], text=True).strip()
    if commit != args.source_commit or len(commit) != 40:
        raise ValueError('Exact source commit required.')
    target = args.output.resolve()
    target.mkdir(parents=True, exist_ok=False)
    root = target / 'source'
    root.mkdir()
    names = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '--name-only', commit], text=True).splitlines()
    selected = {name for name in names if (
        name.startswith(('game/core/', 'game/ui/', 'game/storage/', 'game/addons/')) and Path(name).suffix in {'.gd', '.cfg'}
        or name.startswith('tools/') and Path(name).suffix in {'.py', '.gd'}
        or name.startswith('schemas/') and name.endswith('.json')
        or name.startswith('content/foundation/') and name.endswith('.json')
        or name.startswith('content/packs/legacy/') and name.endswith('/pack.json')
        or name.startswith('game/data/') and '/generated/' not in name and name.endswith('.json')
        or name.startswith('game/assets/audio/') and name.endswith('.wav')
        or name in {'game/project.godot', 'game/main.tscn'})}

    def raw(name):
        """只从同一Git提交读取，避免复制中央进行中的未提交修改。"""
        reply = subprocess.run(['git', '-C', str(repo), 'show', commit + ':' + name], capture_output=True)
        if reply.returncode == 0:
            return reply.stdout
        oid = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', commit + ':' + name], text=True).strip()
        for directory in args.resource_root:
            path = directory / name
            if path.is_file() and not path.is_symlink():
                data = path.read_bytes()
                if hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == oid:
                    return data
        raise ValueError('Missing exact Git blob: ' + name + ' ' + oid)

    registry = json.loads(raw('game/data/presentation.json'))
    selected.update('game/' + row['path'].removeprefix('res://')
                    for group in ['backgrounds', 'portraits'] for row in registry[group].values())
    inputs = []
    for name in sorted(selected):
        data = raw(name)
        destination = root / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        inputs.append({'path': name, 'sha256': sha(data)})
    engine = args.engine.resolve(strict=True)
    if subprocess.check_output([str(engine), '--version'], text=True).strip() != '4.7.2.stable.official.ed1daf0bf':
        raise ValueError('Pinned Godot 4.7.2 required.')
    xdg = {}
    for key, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'), ('XDG_CACHE_HOME', 'cache')]:
        directory = target / 'xdg-prepare' / leaf
        directory.mkdir(parents=True)
        xdg[key] = str(directory)
        os.environ[key] = str(directory)
    os.environ['GODOT_BIN'] = str(engine)
    sys.path.insert(0, str(root))
    from tools.build import stage_content, discover_author_sources
    from tools.content_stage import validate_stage
    stage_content(root)
    manifest = validate_stage(root, discover_author_sources(root))
    binding = {'format': 'HM-ALPHA-LETTER-ESCORT-STANDARD/1', 'source_commit': commit,
               'profile': 'standard-alpha-legacy', 'stage_game': str(root / 'game'),
               'default_scenario': 'scenario.alpha_life', 'engine': str(engine),
               'engine_sha256': sha(engine.read_bytes()), 'inputs': inputs,
               'content_stage': manifest, 'gameplay_executed': False, 'xdg_prepare': xdg}
    (target / 'binding.json').write_text(json.dumps(binding, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'binding': str(target / 'binding.json'), 'inputs': len(inputs), 'gameplay_executed': False}))


if __name__ == '__main__':
    main()
