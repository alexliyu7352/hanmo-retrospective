"""V0.5最小证据入口：准备双配置、短规则对照和独立进程存读；未验输入/美术不得PASS。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import importlib.util
from io import BytesIO
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import warnings

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FORMAT = 'HM-PRESENTATION-SLICE/1'
ENGINE = '4.7.2.stable.official.ed1daf0bf'
GATES = ('structure', 'rules', 'actual_input', 'visual', 'save_resume', 'web')
LINUX_GATES = GATES[:-1]
LINUX_SCOPE = 'linux_native'


def read(path: Path):
    """只读取落盘JSON，缺证据由调用方明确报告。"""
    return json.loads(path.read_text())


def sha(path: Path) -> str:
    """对真实字节建立摘要，包含图像和完整世界而非自报成功。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_identity(root: Path) -> dict:
    """绑定当前源码、作者内容、清单及资源；提交号不能掩盖工作区改动。"""
    files = {}
    for directory in ('game', 'content', 'schemas', 'tools', 'tests/fixtures'):
        for path in sorted((root / directory).rglob('*')):
            relative = path.relative_to(root)
            if not path.is_file() or any(part in ('.godot', '__pycache__', 'generated', 'godot') for part in relative.parts):
                continue
            if path.suffix in ('.pyc', '.uid', '.import'):
                continue
            files[str(relative)] = sha(path)
    canonical = json.dumps(files, sort_keys=True, separators=(',', ':')).encode()
    commit = subprocess.run(['git', '-C', str(root), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
    dirty = subprocess.run(['git', '-C', str(root), 'status', '--porcelain', '--untracked-files=normal'], capture_output=True, text=True, check=True).stdout.splitlines()
    return {'commit': commit, 'sha256': hashlib.sha256(canonical).hexdigest(), 'files': files, 'worktree_changes': dirty}


def artifact(path: Path, output: Path) -> dict:
    """证据路径必须在本次输出目录内，同时记录不可混用的文件摘要。"""
    return {'path': str(path.resolve().relative_to(output.resolve())), 'sha256': sha(path)}


def checked_artifact(reference: dict, output: Path) -> Path:
    """拒绝缺失、逃逸、符号链接和已变化证据，防止回执仍引用旧身份。"""
    try:
        relative = Path(reference['path'])
        path = output / relative
        if relative.is_absolute() or '..' in relative.parts or path.is_symlink():
            raise ValueError('artifact path escapes output')
        resolved = path.resolve()
        resolved.relative_to(output.resolve())
        if not resolved.is_file() or sha(resolved) != reference['sha256']:
            raise ValueError('artifact missing or checksum mismatch')
        return resolved
    except (KeyError, TypeError, OSError) as exc:
        raise ValueError('invalid artifact reference') from exc


def checked_image(reference: dict, output: Path) -> Path:
    """按原字节摘要读取并完整解码PNG/JPEG，不以扩展名、魔数或转码图冒充原截图。"""
    path = checked_artifact(reference, output)
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != reference['sha256']:
        raise ValueError('screenshot changed while reading')
    try:
        with warnings.catch_warnings():
            warnings.simplefilter('error', Image.DecompressionBombWarning)
            with Image.open(BytesIO(raw)) as image:
                if image.format not in ('PNG', 'JPEG'):
                    raise ValueError('screenshot must be PNG or JPEG')
                image.verify()
            with Image.open(BytesIO(raw)) as image:
                image.load()
                if image.width <= 0 or image.height <= 0:
                    raise ValueError('screenshot has no pixels')
    except (OSError, ValueError, SyntaxError, Image.DecompressionBombError, Image.DecompressionBombWarning) as exc:
        raise ValueError('invalid PNG/JPEG screenshot: ' + str(exc)) from exc
    return path


def gameplay_content(pack: dict) -> dict:
    """仅排除允许变化的外貌与布局引用；年龄、属性、剧情和经济规则仍逐字段比较。"""
    result = deepcopy(pack)
    for actor in result['actors']:
        actor.pop('appearance_definition', None)
        actor.pop('presentation_profile_id', None)
    for place in result['places']:
        place.pop('scene_template_ids', None)
    for world_map in result.get('world_maps', []):
        world_map.pop('scene_template_id', None)
    return result


def gameplay_world(state: dict) -> dict:
    """双配置仅忽略明确允许不同的实际外貌与内容摘要，完整保留所有旧玩法字段。"""
    result = deepcopy(state)
    result.pop('content_digest')
    for actor in result['actors'].values():
        actor.pop('appearance')
    return result


def empty_report(root: Path) -> dict:
    """创建明确未验的分项；真实组合/儿童资源缺口不被几何fixture掩盖。"""
    fixture = read(root / 'tests/fixtures/presentation/vertical_slice.json')
    return {'format': FORMAT, 'scope': LINUX_SCOPE, 'status': 'BLOCKED', 'source': source_identity(root),
        'fixture': fixture, 'engine': {'required': ENGINE, 'actual': ''},
        'structure': {'status': 'NOT_RUN'}, 'rules': {'status': 'NOT_RUN'},
        'actual_input': {'status': 'BLOCKED', 'reason': '未采集操作系统鼠标/键盘贯通和作者UI保存重开；脚本调用不能替代'},
        'visual': {'status': 'BLOCKED', 'asset_scope': fixture['asset_scope'], 'reason': fixture['visual_blocker']},
        'save_resume': {'status': 'NOT_RUN'},
        'web': {'status': 'DEFERRED', 'reason': '本入口仅固定Linux短测，Web导出/浏览器实际输入另行验证'}}


def validate_report(report: dict, output: Path) -> list[str]:
    """校验分项和可核证据；缺实际输入、重启、完整状态或儿童实图不能升级为PASS。"""
    errors = []
    if report.get('format') != FORMAT:
        errors.append('format: unsupported')
    if not re.fullmatch(r'[0-9a-f]{40}', report.get('source', {}).get('commit', '')) or not report.get('source', {}).get('files'):
        errors.append('source: missing source identity')
    else:
        encoded = json.dumps(report['source']['files'], sort_keys=True, separators=(',', ':')).encode()
        if hashlib.sha256(encoded).hexdigest() != report['source'].get('sha256'):
            errors.append('source: tree digest mismatch')
    scope = report.get('scope')
    if scope not in (None, LINUX_SCOPE):
        errors.append('scope: unsupported')
    required = LINUX_GATES if scope == LINUX_SCOPE else GATES
    if scope == LINUX_SCOPE:
        web = report.get('web', {})
        if web.get('status') not in ('DEFERRED', 'NOT_RUN') or not isinstance(web.get('reason'), str) or not web['reason'].strip():
            errors.append('web: Linux scope requires explicit deferral, not Web certification')
    if report.get('status') == 'PASS' and any(report.get(gate, {}).get('status') != 'PASS' for gate in required):
        errors.append('status: incomplete scope gates cannot PASS')
    for gate in GATES:
        section = report.get(gate, {})
        if section.get('status') not in ('PASS', 'FAIL', 'BLOCKED', 'NOT_RUN', 'DEFERRED'):
            errors.append(gate + ': invalid status')
        if section.get('status') != 'PASS':
            continue
        try:
            if gate in ('rules', 'save_resume') and report.get('engine', {}).get('actual') != ENGINE:
                raise ValueError('missing pinned engine execution')
            if gate == 'structure':
                preparation = read(checked_artifact(section['evidence'], output))
                if len(preparation['variants']) != 2 or section.get('same_gameplay_content') is not True:
                    raise ValueError('missing two author variants')
                variants = section['variants']
                if {item['id'] for item in variants} != {'a', 'b'}:
                    raise ValueError('missing distinct author configurations')
                authors = [read(checked_artifact(item['author'], output)) for item in variants]
                if gameplay_content(authors[0]) != gameplay_content(authors[1]):
                    raise ValueError('author variants changed gameplay')
                for item in variants:
                    catalog = read(checked_artifact(item['catalog'], output))
                    if catalog['source_sha256'] != item['author']['sha256'] or item['catalog_sha256'] != item['catalog']['sha256']:
                        raise ValueError('catalog not bound to saved author bytes')
                for variant in preparation['variants']:
                    if variant['saved'] != 'AUTHOR_SAVED' or variant['reopened_equal'] is not True or variant['prepared'] != 'AUTHORING_ONLY':
                        raise ValueError('missing normal author save/reopen/prepare')
            elif gate in ('rules', 'save_resume'):
                if len(section['evidence']) != 2 or {entry['id'] for entry in section['evidence']} != {'a', 'b'}:
                    raise ValueError('both distinct variants required')
                for entry in section['evidence']:
                    evidence = read(checked_artifact(entry['proof'], output))
                    if evidence['failures'] or evidence['checks'] <= 0:
                        raise ValueError('missing successful assertions')
                    catalog = next(item for item in report['structure']['variants'] if item['id'] == entry['id'])
                    content_digest = read(checked_artifact(catalog['catalog'], output))['catalog_sha256']
                    if gate == 'rules':
                        if not {'actors', 'rng', 'tick', 'content_digest'} <= evidence['rendered_state'].keys() or evidence['rendered_state']['content_digest'] != content_digest:
                            raise ValueError('missing full state or mismatched content identity')
                        for actor, profile in evidence['profiles'].items():
                            if evidence['rendered_state']['actors'][actor]['appearance'] != profile:
                                raise ValueError('actor profile differs from full world state')
                        if not evidence['commands'] or not evidence['scenes'] or not evidence['profiles'] or evidence['rendered_state'] != evidence['headless_state']:
                            raise ValueError('missing full-state command/render comparison')
                    else:
                        checkpoint = read(checked_artifact(entry['checkpoint'], output))
                        if not {'actors', 'rng', 'tick', 'content_digest'} <= evidence['state'].keys() or evidence['state']['content_digest'] != content_digest:
                            raise ValueError('missing full restored state or content identity')
                        if evidence['reader_pid'] == evidence['writer_pid'] or evidence['writer_pid'] != checkpoint['writer_pid'] or evidence['state'] != checkpoint['state']:
                            raise ValueError('not an exact cross-process restore')
                        for actor, profile in checkpoint['profiles'].items():
                            if evidence['state']['actors'][actor]['appearance'] != profile:
                                raise ValueError('actor profile changed on restore')
                if gate == 'rules':
                    worlds = [gameplay_world(read(checked_artifact(entry['proof'], output))['rendered_state']) for entry in section['evidence']]
                    if worlds[0] != worlds[1]:
                        raise ValueError('appearance/layout changed gameplay between variants')
            elif gate == 'actual_input':
                evidence = read(checked_artifact(section['evidence'], output))
                if evidence['backend'] != 'native_os' or evidence['source_sha256'] != report['source']['sha256']:
                    raise ValueError('missing source-bound native input')
                if evidence['manifest_sha256'] != report['structure']['manifest']['sha256']:
                    raise ValueError('native input used a different manifest')
                if set(report['fixture']['required_input_steps']) - {step['id'] for step in evidence['steps']}:
                    raise ValueError('missing required real-input steps')
                for step in evidence['steps']:
                    if step['event'] not in ('mouse', 'keyboard'):
                        raise ValueError('signals/callbacks are not native input')
                    checked_image(step['screenshot'], output)
                    actual = read(checked_artifact(step['state'], output))
                    reference = read(checked_artifact(step['headless_state'], output))
                    catalog = next(item for item in report['structure']['variants'] if item['id'] == step['variant'])
                    content_digest = read(checked_artifact(catalog['catalog'], output))['catalog_sha256']
                    if not {'actors', 'rng', 'tick', 'content_digest'} <= actual.keys() or actual['content_digest'] != content_digest:
                        raise ValueError('native input lacks complete source-bound state')
                    if actual != reference:
                        raise ValueError('native input differs from full-state headless replay')
            elif gate == 'visual':
                if section.get('asset_scope') != 'production_art':
                    raise ValueError('geometric fixtures cannot prove real art or child proportions')
                manifest = read(checked_artifact(report['structure']['manifest'], output))
                if not all(manifest.get(group) for group in ('components', 'character_profiles', 'scene_templates')) or any('fixtures/' in asset['path'] for group in ('backgrounds', 'portraits') for asset in manifest[group].values()):
                    raise ValueError('geometric or missing production art cannot pass visual review')
                evidence = read(checked_artifact(section['evidence'], output))
                if evidence['manifest_sha256'] != report['structure']['manifest']['sha256']:
                    raise ValueError('visual review used a different manifest')
                if evidence['source_sha256'] != report['source']['sha256'] or not evidence['reviewer']:
                    raise ValueError('missing source-bound visual review')
                if set(report['fixture']['required_visual_cases']) - set(evidence['cases']):
                    raise ValueError('missing child/age/attire/identity visual cases')
                expected = {tuple(row) for row in report['fixture']['window_matrix']}
                if expected - {tuple(shot['window']) for shot in evidence['screenshots']}:
                    raise ValueError('missing window/font matrix')
                for shot in evidence['screenshots']:
                    checked_image(shot['image'], output)
                for case in evidence['cases'].values():
                    if case['result'] != 'PASS' or not case['notes']:
                        raise ValueError('missing actual visual judgment')
            elif gate == 'web':
                raise ValueError('Web requires a dedicated browser/export receipt; this Linux entry cannot certify it')
        except (KeyError, TypeError, ValueError, OSError, StopIteration) as exc:
            errors.append(gate + ': ' + str(exc))
    # 每个引用都核验，即使其他门BLOCKED，也不能把已有绿门的证据换成另一份。
    def walk(value):
        """遍历已登记证据引用，不扫描任意外部目录或读取未引用文件。"""
        if isinstance(value, dict):
            if set(value) == {'path', 'sha256'}:
                try:
                    checked_artifact(value, output)
                except ValueError as exc:
                    errors.append('artifact: ' + str(exc))
            else:
                for child in value.values(): walk(child)
        elif isinstance(value, list):
            for child in value: walk(child)
    walk(report)
    return errors


def run_process(command: list[str], log: Path, env: dict | None = None, timeout: int = 120) -> bool:
    """记录有限子进程，退出码和Godot错误必须一起检查；超时保留已得输出。"""
    try:
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=timeout)
        text = result.stdout + result.stderr
        log.write_text(text)
        return result.returncode == 0 and not re.search(r'(^|\n)(SCRIPT ERROR:|ERROR:|FAIL:)', text)
    except (subprocess.TimeoutExpired, OSError) as exc:
        log.write_text(str(exc))
        return False


def run(engine: str, output: Path, root: Path = ROOT) -> dict:
    """仅运行双配置短门，严格分离真实输入、素材及Web仍待完成的范围。"""
    output.mkdir(parents=True, exist_ok=False)
    report = empty_report(root)
    started = time.monotonic()
    report['timings_seconds'] = {}
    try:
        helper = root / 'tests/fixtures/presentation/prepare_vertical_slice.py'
        spec = importlib.util.spec_from_file_location('prepare_vertical_slice', helper)
        prepare = importlib.util.module_from_spec(spec); spec.loader.exec_module(prepare)
        project = output / 'project'
        prepared = prepare.prepare(project, root)
        report['timings_seconds']['prepare'] = round(time.monotonic() - started, 3)
        variants = prepared['variants']
        authors = [read(Path(item['author_path'])) for item in variants]
        same = gameplay_content(authors[0]) == gameplay_content(authors[1])
        report['structure'] = {'status': 'PASS' if same else 'FAIL', 'same_gameplay_content': same,
            'evidence': artifact(project / 'prepared.json', output),
            'manifest': artifact(project / 'game/data/presentation.json', output),
            'variants': [{key: item[key] for key in ('id', 'author_sha256', 'catalog_sha256')} | {
                'author': artifact(Path(item['author_path']), output), 'catalog': artifact(Path(item['catalog_path']), output)} for item in variants]}
        version = subprocess.run([engine, '--version'], capture_output=True, text=True, timeout=15)
        report['engine']['actual'] = version.stdout.strip()
        if version.returncode or report['engine']['actual'] != ENGINE:
            raise ValueError('需要固定引擎 ' + ENGINE)
        phase_started = time.monotonic()
        imported = run_process([engine, '--headless', '--editor', '--path', str(project / 'game'), '--import'], output / 'import.log')
        report['timings_seconds']['import'] = round(time.monotonic() - phase_started, 3)
        report['structure']['import_log'] = artifact(output / 'import.log', output)
        if not imported:
            report['structure']['status'] = 'FAIL'
            raise ValueError('隔离工程导入失败，见import.log')
        rules = []; resumes = []; successful = True
        for item in variants:
            folder = output / ('run-' + item['id']); folder.mkdir()
            env = dict(os.environ, HANMO_SLICE_OUTPUT=str(folder), HANMO_SLICE_CATALOG=item['catalog_path'])
            command = [engine, '--headless', '--path', str(project / 'game'), '--script', 'res://tests/test_presentation_vertical_slice.gd']
            phase_started = time.monotonic()
            wrote = run_process(command, folder / 'rules.log', dict(env, HANMO_SLICE_MODE='write'))
            wrote = wrote and bool(re.search(r'PRESENTATION_SLICE_CHECKS=[1-9][0-9]* FAILURES=0', (folder / 'rules.log').read_text())) and (folder / 'rules.json').exists()
            report['timings_seconds']['rules_' + item['id']] = round(time.monotonic() - phase_started, 3)
            successful = successful and wrote
            if wrote:
                rules.append({'id': item['id'], 'proof': artifact(folder / 'rules.json', output), 'initial': artifact(folder / 'initial.json', output), 'log': artifact(folder / 'rules.log', output)})
                phase_started = time.monotonic()
                restored = run_process(command, folder / 'resume.log', dict(env, HANMO_SLICE_MODE='resume'))
                report['timings_seconds']['resume_' + item['id']] = round(time.monotonic() - phase_started, 3)
                if restored and (folder / 'resume.json').exists():
                    resumes.append({'id': item['id'], 'proof': artifact(folder / 'resume.json', output),
                        'checkpoint': artifact(folder / 'checkpoint.json', output), 'slot': artifact(folder / 'saves/manual-1.B.hmsave', output), 'log': artifact(folder / 'resume.log', output)})
        if successful and len(rules) == 2:
            worlds = [gameplay_world(read(output / item['proof']['path'])['rendered_state']) for item in rules]
            successful = worlds[0] == worlds[1]
        report['rules'] = {'status': 'PASS' if successful and len(rules) == 2 else 'FAIL', 'scope': '现有护卫/庄园短轨迹完整状态对照；不含年龄跨界或原生UI取消/迟到回调', 'evidence': rules}
        report['world_state_matches_headless'] = report['rules']['status'] == 'PASS'
        report['save_resume'] = {'status': 'PASS' if len(resumes) == 2 else 'FAIL', 'scope': 'NativeStorage独立进程完整恢复；尚不代表主界面实际点击继续', 'evidence': resumes}
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        report['blocker'] = str(exc)
    report['timings_seconds']['total'] = round(time.monotonic() - started, 3)
    report['validation_errors'] = validate_report(report, output)
    if report['validation_errors'] or any(report[gate]['status'] == 'FAIL' for gate in LINUX_GATES):
        report['status'] = 'FAIL'
    elif all(report[gate]['status'] == 'PASS' for gate in LINUX_GATES):
        report['status'] = 'PASS'
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + '\n')
    return report


def main() -> int:
    """PASS/FAIL/BLOCKED分别返回0/1/2，不能被通用聚合当作无条件成功。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    report = run(args.engine, args.output.resolve())
    print(json.dumps({key: report[key] for key in ('format', 'scope', 'status')} | {gate: report[gate]['status'] for gate in GATES}, ensure_ascii=False))
    return {'PASS': 0, 'FAIL': 1, 'BLOCKED': 2}[report['status']]


if __name__ == '__main__':
    raise SystemExit(main())
