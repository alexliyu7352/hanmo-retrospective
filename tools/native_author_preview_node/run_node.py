"""真实作者worker打开副本、编辑保存、compose、preview_prepare，再由正常Main控件执行窄门。"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


class ExpectedPreviewRejection(Exception):
    """保存真实协议拒绝，不将该负向门误报为游戏运行成功。"""


def digest(path: Path) -> str:
    """流式核对来源原件，不读取或改写旧银行。"""
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write(path: Path, value: object) -> None:
    """证据只写全新文件，失败不被后续重跑覆盖。"""
    with path.open('x', encoding='utf8') as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write('\n')


def inventory(root: Path) -> dict:
    """记录真实原始输入，排除仅由Godot导入产生的缓存。"""
    return {str(path.relative_to(root)): digest(path) for path in sorted(root.rglob('*'))
            if path.is_file() and '.godot' not in path.relative_to(root).parts and path.suffix != '.import'}


def main() -> None:
    """单次真实协议全链使用新输出和独立XDG，任何失败保留原回执并停止。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--author-pack', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--native-stage', type=Path)
    parser.add_argument('--expect-missing-backend', action='store_true')
    parser.add_argument('--expect-preview-error', default='')
    parser.add_argument('--legacy-gd', action='store_true')
    parser.add_argument('--source-commit', default='')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    source, output, editor = args.author_pack.resolve(strict=True), args.output.absolute(), args.editor.resolve(strict=True)
    if output.exists() or source in output.parents or output in source.parents:
        raise ValueError('Require a new independent output outside author inputs.')
    output.mkdir(parents=True)
    manifest = json.loads((source / 'pack.json').read_text())
    members = ['pack.json', *manifest['content_entries'], *manifest['presentation_entries'],
               *(row['path'] for row in manifest['resources'])]
    baseline = {name: digest(source / name) for name in members}
    author = output / 'author-copy'
    for name in members:
        target = author / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, target)
    snapshot = args.source_commit or subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    source_paths = ['tools/content/' + name + '.py' for name in ('editor_protocol', 'pack_document', 'pack_preview')]
    if (root / 'tools/content/preview_native.py').exists():
        source_paths.append('tools/content/preview_native.py')
    source_paths += ['game/ui/main.gd', 'game/addons/hanmo_author/panel.gd']
    product_sources = {}
    for name in source_paths:
        raw = subprocess.check_output(['git', 'show', snapshot + ':' + name], cwd=root)
        if raw != (root / name).read_bytes():
            raise ValueError('Product source differs from declared freeze: ' + name)
        product_sources[name] = hashlib.sha256(raw).hexdigest()
    write(output / 'source-binding.json', {'source': str(source), 'source_files': baseline,
          'source_commit': snapshot, 'product_sources': product_sources,
          'runner_sha256': digest(Path(__file__)), 'editor_sha256': digest(editor),
          'expect_missing_backend': args.expect_missing_backend})
    env = dict(os.environ, GODOT_SILENCE_ROOT_WARNING='1')
    state = output / 'state'
    for variable, leaf in [('XDG_DATA_HOME', 'data'), ('XDG_CONFIG_HOME', 'config'),
                           ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('TMPDIR', 'tmp')]:
        folder = state / leaf
        folder.mkdir(parents=True)
        env[variable] = str(folder)
    env['TMP'] = env['TEMP'] = env['TMPDIR']
    env['HANMO_STATE_ROOT'] = str(state)
    transcript = output / 'author-protocol.jsonl'
    worker_log = (output / 'author-worker.log').open('xb')
    worker = subprocess.Popen([sys.executable, str(root / 'tools/authoring_worker.py')], cwd=root,
                              env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=worker_log, text=True)
    document = None
    serial = 0

    def request(operation: str, arguments: dict) -> dict:
        """每次请求沿最新document/revision/source guard，不直接调用文档私有方法。"""
        nonlocal document, serial
        serial += 1
        frame = {'id': serial, 'op': operation, 'args': arguments}
        if document:
            frame.update({key: document[key] for key in ('document_id', 'revision', 'source_sha256')})
        worker.stdin.write(json.dumps(frame, ensure_ascii=False) + '\n')
        worker.stdin.flush()
        line = worker.stdout.readline()
        response = json.loads(line)
        with transcript.open('a', encoding='utf8') as stream:
            stream.write(json.dumps({'request': frame, 'response': response}, ensure_ascii=False) + '\n')
        document = response.get('document')
        if not response['ok']:
            if operation == 'preview_prepare' and response['error']['code'] == args.expect_preview_error:
                raise ExpectedPreviewRejection(json.dumps(response['error'], ensure_ascii=False))
            raise RuntimeError(json.dumps(response['error'], ensure_ascii=False))
        return response['result']

    report = {'format': 'HM-NATIVE-AUTHOR-PREVIEW-NODE/1', 'ok': False,
              'gui_pixels_accepted': False, 'gui_mouse_evidence': False, 'scope': 'headless_main_control_signals'}
    try:
        request('open_pack', {'path': str(author / 'pack.json')})
        content = json.loads((author / manifest['content_entries'][0]).read_text())
        title_key = content['scenarios'][0]['name_key']
        request('edit', {'entry': manifest['content_entries'][0], 'operation': 'set_text',
                        'arguments': {'key': title_key, 'value': content['texts'][title_key] + ' · 作者试玩核验'}})
        request('save', {})
        composed = request('compose', {'output': str(output / 'compose')})
        arguments = {'destination': str(output / 'preview'), 'expected_pack_set_digest': composed['pack_set_digest']}
        if args.native_stage:
            arguments['native_stage'] = str(args.native_stage.absolute())
        prepared = request('preview_prepare', arguments)
        if args.expect_preview_error:
            raise RuntimeError('Expected preview rejection, but the product published a preview.')
        worker.stdin.close()
        report['worker_exit_code'] = worker.wait(timeout=10)
        game = Path(prepared['game_path'])
        before = inventory(game)
        phase = output / 'main'
        phase.mkdir()
        binding = dict(prepared, state_root=str(state), expect_missing_backend=args.expect_missing_backend,
                       legacy_gd=args.legacy_gd,
                       scenario_id=manifest['namespace'] + '::' + content['scenarios'][0]['id'],
                       player_actor_id='society::actor.study_swordswoman', runner_kind='fixed_editor_loading_source_stage')
        write(output / 'binding.json', binding)
        private_editor = output / 'editor' / editor.name
        private_editor.parent.mkdir()
        os.link(editor, private_editor)
        (private_editor.parent / '_sc_').touch()
        import_command = [str(private_editor), '--headless', '--path', str(game), '--editor', '--import', '--quit']
        with (output / 'preview-import.log').open('xb') as log:
            imported = subprocess.run(import_command, cwd=game, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=60)
        report['import'] = {'command': import_command, 'exit_code': imported.returncode}
        if imported.returncode != 0:
            raise RuntimeError('The original panel import step failed; do not launch Main.')
        driver = Path(__file__).with_name('main_entry.gd')
        command = [str(private_editor), '--headless', '--max-fps', '60', '--path', str(game),
                   '--script', str(driver), '--', str(output / 'binding.json'), 'produce', str(phase)]
        started = time.monotonic()
        with (phase / 'process.log').open('xb') as log:
            process = subprocess.run(command, cwd=game, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
        result_path = phase / 'result.json'
        result = json.loads(result_path.read_text()) if result_path.exists() else {}
        after = inventory(game)
        report.update(command=command, exit_code=process.returncode, result=result,
                      elapsed_seconds=round(time.monotonic() - started, 3),
                      preview_inputs_unchanged=all(after.get(name) == sha for name, sha in before.items()),
                      engine_generated_files={name: sha for name, sha in after.items() if name not in before})
        report['ok'] = process.returncode == 0 and result.get('ok') is True and report['preview_inputs_unchanged']
    except ExpectedPreviewRejection as error:
        report.update(ok=not (output / 'preview').exists(), expected_preview_rejection=json.loads(str(error)),
                      destination_absent=not (output / 'preview').exists(), main_started=False)
    except (ValueError, OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        report['error'] = str(error)
    finally:
        if worker.poll() is None:
            worker.stdin.close()
            report['worker_exit_code'] = worker.wait(timeout=10)
        worker_log.close()
        report['original_author_unchanged'] = baseline == {name: digest(source / name) for name in members}
        report['ok'] = report['ok'] and report['original_author_unchanged']
        write(output / 'report.json', report)
    print(json.dumps({'ok': report['ok'], 'report': str(output / 'report.json')}, ensure_ascii=False))
    raise SystemExit(0 if report['ok'] else 1)


if __name__ == '__main__':
    main()
