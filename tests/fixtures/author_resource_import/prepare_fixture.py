"""准备真实项目原图的轻量作者资源测试工程；只复制三张精确原件，不推定任何对外许可。"""
import argparse
from io import BytesIO
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile


def digest(raw):
    """对保留的原字节求摘要，不能按文件名或图片外观推定来源。"""
    return hashlib.sha256(raw).hexdigest()


def extract(repository, commit, paths, output):
    """只物化明确Git源码成员，拒绝非普通归档项，不复制美术整库。"""
    raw = subprocess.check_output(['git', '-C', str(repository), 'archive', commit, *paths])
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


def prepare(repository, source, commit, output, client_commit=None):
    """源码与输入身份分开冻结，生成正常worker和明确的单点失败worker供同一节点门使用。"""
    if output.exists() or not re.fullmatch(r'[0-9a-f]{7,40}', commit):
        raise ValueError('Require a fresh output and precise source commit')
    commit = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', '--verify', commit + '^{commit}'], text=True).strip()
    output.mkdir(parents=True)
    extract(repository, commit, ['tools', 'schemas', 'game/core', 'game/ui', 'game/addons',
                                'game/data', 'game/storage', 'game/project.godot'], output)
    if client_commit is not None:
        if not re.fullmatch(r'[0-9a-f]{7,40}', client_commit):
            raise ValueError('Client overlay needs a precise source commit')
        client_commit = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', '--verify', client_commit + '^{commit}'], text=True).strip()
        raw = subprocess.check_output(['git', '-C', str(repository), 'show', client_commit + ':game/addons/hanmo_author/client.gd'])
        (output / 'game/addons/hanmo_author/client.gd').write_bytes(raw)
    (output / 'game/tests').mkdir()
    shutil.copy2(source / 'game/tests/test_editor_resource_import.gd', output / 'game/tests/test_editor_resource_import.gd')
    settings = output / 'game/project.godot'
    text = settings.read_text()
    text = re.sub(r'^enabled=PackedStringArray\(.*\)$', 'enabled=PackedStringArray()', text, flags=re.M)
    text = re.sub(r'^run/main_scene=.*\n', '', text, flags=re.M)
    text = re.sub(r'^config/custom_user_dir_name=.*$', 'config/custom_user_dir_name="hanmo-author-resource-tests"', text, flags=re.M)
    text = text.replace('[application]', '[application]\nconfig/resource_import_isolated=true')
    settings.write_text(text)
    pin_path = Path('content/packs/legacy/archive/identity-bindings.json')
    pin_raw = (repository / pin_path).read_bytes()
    pins = {row['path']: row['sha256'] for row in json.loads(pin_raw)['resources']}
    sources = {}
    for relative in [pin_path, Path('game/assets/art/alpha-visual/manifest.json')]:
        raw = (repository / relative).read_bytes()
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        sources[str(relative)] = digest(raw)
    # 真实默认两张和用户额外选择的hall均走原来源pin，原许可与生成记录不作改写。
    assets = []
    for name in ['city.png', 'merchant.png', 'hall.png']:
        relative = 'game/assets/art/alpha-visual/' + name
        raw = (repository / relative).read_bytes()
        if digest(raw) != pins.get(relative):
            raise ValueError('Original pin mismatch: ' + relative)
        (output / relative).write_bytes(raw)
        sources[relative] = digest(raw)
        assets.append({'source': relative, 'sha256': digest(raw), 'bytes': len(raw),
                       'target': 'assets/art/project/alpha-visual/' + name})
    if not any(row['bytes'] > 2 * 1024 * 1024 for row in assets):
        raise ValueError('Fixture must exercise a real image larger than the ordinary JSON budget')
    template = repository / 'tests/fixtures/content_packs/compose/new_packs/base'
    pack = output / 'test-inputs/pack-template'
    pack.mkdir(parents=True)
    manifest = json.loads((template / 'pack.json').read_text())
    manifest['presentation_entries'] = []
    manifest['resources'] = []
    (pack / 'pack.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    shutil.copy2(template / 'content.json', pack / 'content.json')
    wrapper = output / 'test-inputs/fail_second_resource_worker.py'
    wrapper.write_text('''"""测试专属单点写入故障；其余操作、协议及回复均委托真实生产作者后端。"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.content import pack_document
from tools.content.document import DocumentError
from tools.content.editor_protocol import run_stream
original_writer = pack_document._write_author_bytes
remaining = True


def fail_second(target, data, **kwargs):
    """仅第一次hall原图写入前拒绝，第一张已提交原件和之后重试都走真正writer。"""
    global remaining
    if remaining and target.name == 'hall.png':
        remaining = False
        raise DocumentError('WRITE_FAILED', 'Test-only interruption before second original image')
    return original_writer(target, data, **kwargs)


pack_document._write_author_bytes = fail_second
raise SystemExit(run_stream(sys.stdin.buffer, sys.stdout))
''')
    fixture = {'format': 'HM-AUTHOR-RESOURCE-NODE-FIXTURE', 'production_commit': commit, 'client_commit': client_commit,
               'assets': assets, 'selection': [{'group': 'backgrounds', 'id': 'city'},
                                             {'group': 'backgrounds', 'id': 'hall'},
                                             {'group': 'portraits', 'id': 'merchant'}],
               'project_inputs_sha256': sources, 'permission': 'project-local-authorized-use',
               'external_distribution_verified': False, 'fault_worker': str(wrapper),
               'partial_scope': 'one explicit injected DocumentError before second image; no fake protocol replies'}
    (output / 'test-inputs/fixture.json').write_text(json.dumps(fixture, ensure_ascii=False, indent=2) + '\n')
    for relative, expected in sources.items():
        if digest((repository / relative).read_bytes()) != expected:
            raise ValueError('Original source changed during preparation: ' + relative)
    print(json.dumps({'project': str(output), 'production_commit': commit, 'image_count': len(assets),
                      'image_bytes': sum(row['bytes'] for row in assets)}, ensure_ascii=False))


def main():
    """调用者明确给出精确生产提交和全新隔离输出，禁止覆盖已有测试工程。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--test-source', type=Path, required=True)
    parser.add_argument('--production-commit', required=True)
    parser.add_argument('--client-commit')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.repository.resolve(), args.test_source.resolve(), args.production_commit, args.output.absolute(), args.client_commit)


if __name__ == '__main__':
    main()
