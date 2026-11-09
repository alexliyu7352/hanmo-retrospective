#!/usr/bin/env python3
"""Godot构建和测试入口；失败立即停止，导出成功不代表网页已经验收。"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'game'
FONTS = {'.ttf','.otf','.ttc','.woff','.woff2'}


def find_engine() -> str:
    """优先使用明确引擎路径，否则查询PATH；不下载或暗中替换引擎。"""
    requested = os.environ.get('GODOT_BIN')
    executable = shutil.which(requested) if requested else (shutil.which('godot') or shutil.which('godot4'))
    if not executable:
        raise RuntimeError('Godot executable not found. Set GODOT_BIN to your installed Godot 4 standard editor.')
    return executable


def run_checked(command: list[str], *, timeout: int = 180, log: Path | None = None) -> str:
    """执行真实子进程并保留输出；非零退出和引擎错误都必须上报。"""
    try:
        result = subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=timeout)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f'Command could not finish: {command[0]}: {exc}') from exc
    output = result.stdout + result.stderr
    if log is not None:
        log.parent.mkdir(parents=True,exist_ok=True)
        log.write_text(output,encoding='utf-8')
    if result.returncode != 0:
        raise RuntimeError(f'Command failed with status {result.returncode}: {command}\n{output}')
    if re.search(r'(^|\n)(SCRIPT ERROR:|ERROR:)',output):
        raise RuntimeError(f'Engine diagnostic detected: {command}\n{output}')
    return output


def require_font(game: Path = GAME) -> Path:
    """网页必须有已准备且许可摘要一致的 CJK 字体，失效依赖不能靠系统字体掩盖。"""
    try:
        from prepare_font import prepared_font
    except ModuleNotFoundError:
        from tools.prepare_font import prepared_font
    try:
        return prepared_font(game)['path']
    except ValueError as exc:
        raise RuntimeError(str(exc)) from exc


def source_files(root: Path = ROOT):
    """枚举可交付源码，排除引擎、构建、私人状态、缓存和全部字体格式。"""
    excluded = {'.git','.godot','.pytest_cache','__pycache__','build','.local','.worktrees'}
    local_directories = {'backups', 'builds', 'cache', 'config', 'data', 'tmp', 'venv'}
    installed_tools = {'godot', 'fonts', 'art-models'}
    files = []
    for current, directories, names in os.walk(root):
        directory = Path(current)
        relative = directory.relative_to(root)
        directories[:] = [name for name in directories
                          if name not in excluded
                          and not (relative == Path('.') and name in local_directories)
                          and not (relative == Path('tools') and name in installed_tools)
                          and name not in {'Default', 'Cache', 'GPUCache', 'Code Cache', 'ShaderCache', 'GrShaderCache'}
                          and not name.startswith(('browser-profile', 'profile-run.', 'browser-attempt-'))]
        files.extend(directory/name for name in names)
    for path in sorted(files):
        relative = path.relative_to(root)
        if not path.is_file():
            continue
        if relative.parts[:3] == ('game','data','generated'):
            continue
        if relative.parts[:3] == ('game','assets','fonts'):
            continue
        if path.suffix.lower() in FONTS | {'.pyc', '.pyo', '.bundle', '.hmsave', '.tmp', '.cache',
                                          '.pck', '.wasm', '.x86_64', '.exe', '.dll', '.so', '.o', '.a', '.apk'}:
            continue
        yield path


def discover_author_sources(root: Path) -> list[Path]:
    """列出本工程作者包；仅跳过已知能力清单，坏文件或未知格式阻止发布。"""
    sources = []
    for path in sorted((root/'content/foundation').glob('*.json')):
        try:
            data = json.loads(path.read_text(encoding='utf-8'))
        except (OSError, ValueError) as exc:
            raise RuntimeError('无法读取作者文件：'+str(path)) from exc
        kind = data.get('format') if isinstance(data, dict) else None
        if kind == 'HM-RUNTIME-CAPABILITIES':
            continue
        if kind != 'HM-CONTENT':
            raise RuntimeError('未知作者格式：'+str(path))
        sources.append(path)
    if not sources:
        raise RuntimeError('没有可准备的作者内容')
    return sources


def stage_content(root: Path = ROOT, *, content_sets: list[Path] | tuple = (), foundation_sets: bool = True,
                  expected_content_sets: list[dict] | tuple = ()) -> None:
    """先验证完整新代再顺序切换；异常可回滚，不承诺并发读写或崩溃原子性。"""
    try:
        from tools import content_stage as staging
    except ModuleNotFoundError:
        import content_stage as staging
    root = root.resolve()
    destination = root / 'game/data/generated'
    candidate = backup = None
    try:
        for relative in ('build', 'build/content', 'game', 'game/data', 'game/data/generated'):
            if (root / relative).is_symlink():
                raise ValueError('Content staging refuses symlink: ' + relative)
        sources = sorted(discover_author_sources(root))
        try:
            from tools.presentation import project_inputs
        except ModuleNotFoundError:
            from presentation import project_inputs
        presentation_inputs = project_inputs(root)
        schema_inputs = list(staging.schema_sources(root).values())
        inputs = sources + [root / staging.SCHEMA, root / staging.CAPABILITY,
                            root / 'schemas' / staging.PACK_SET_SCHEMA] + schema_inputs + presentation_inputs
        if any(path.is_symlink() for path in inputs):
            raise ValueError('Content staging inputs must be regular files')
        snapshots = {path: path.read_bytes() for path in inputs}
        build_dir = root / 'build/content'; build_dir.mkdir(parents=True, exist_ok=True)
        destination.parent.mkdir(parents=True, exist_ok=True)
        # 每次唯一的空编译目录；不接管编辑器preview，也不允许零退出复用旧文件。
        with tempfile.TemporaryDirectory(prefix='.stage-', dir=build_dir) as temporary:
            prepared = Path(temporary)
            for source in sources:
                target = prepared / (source.stem + '.hmcatalog.json')
                try:
                    process = subprocess.run(
                        [sys.executable, str(root / 'tools/content_pipeline.py'), 'prepare', str(source), '--output', str(target)],
                        cwd=root, capture_output=True, text=True, timeout=60)
                except (OSError, subprocess.TimeoutExpired) as exc:
                    raise RuntimeError(f'Content preparation could not finish: {exc}') from exc
                if process.returncode or not target.is_file() or target.is_symlink():
                    raise RuntimeError(f'Content preparation failed: {source.name}\n{process.stdout}\n{process.stderr}')
            contract = staging.load_contract(root / staging.CAPABILITY)
            declaration = staging.runtime_declaration(contract, path=root / staging.CAPABILITY)
            (prepared / 'runtime_capabilities.json').write_text(json.dumps(declaration, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
            shutil.copyfile(root / staging.SCHEMA, prepared / 'content.schema.json')
            staging.write_schema_index(root, prepared)
            installed_snapshots, installed_members = staging.stage_installed_sets(
                root, prepared, destination, [Path(path).absolute() for path in content_sets],
                foundation_sets=foundation_sets)
            if expected_content_sets:
                incoming = {}
                for value in content_sets:
                    source = Path(value).absolute()
                    identity = hashlib.sha256(installed_snapshots[source / 'pack-set.lock.json']).hexdigest()
                    incoming[identity] = {'pack_set_digest': identity,
                                          'directory': staging.SET_DIRECTORY + '/' + identity,
                                          'files': [{'path': name, 'sha256': hashlib.sha256(installed_snapshots[source / name]).hexdigest()}
                                                    for name in sorted(installed_members[source])]}
                if [incoming[key] for key in sorted(incoming)] != sorted(expected_content_sets, key=lambda row: row['pack_set_digest']):
                    raise ValueError('Selected content set changed during preparation')
                installed = staging.read_pack(prepared / staging.INSTALLED_INDEX)
                actual = {row['pack_set_digest']: row for row in installed['sets']}
                if any(actual.get(row['pack_set_digest']) != row for row in expected_content_sets):
                    raise ValueError('Selected content set changed during preparation')
            if any(path in snapshots and snapshots[path] != raw for path, raw in installed_snapshots.items()):
                raise ValueError('Content inputs changed during installed set preparation')
            snapshots.update(installed_snapshots)
            manifest = staging.make_manifest(root, prepared, sources)
            (prepared / staging.MANIFEST).write_bytes(staging.canonical(manifest))
            previous = staging.files_in(destination)
            if staging.MANIFEST in previous:
                old_manifest = staging.read_manifest(destination)
                owned = {row['path'] for row in old_manifest['files']} | {staging.MANIFEST}
            else:
                # 无manifest只能接管与当前已验证产物完全一致的legacy；歧义碰撞安全拒绝。
                owned = {name for name, path in previous.items() if (prepared / name).is_file()
                         and path.read_bytes() == (prepared / name).read_bytes()}
            fresh = staging.files_in(prepared)
            collisions = (set(previous) - owned) & set(fresh)
            if collisions:
                raise ValueError('Unowned content collision: ' + ', '.join(sorted(collisions)))
            candidate = Path(tempfile.mkdtemp(prefix='.content-candidate-', dir=destination.parent))
            if destination.exists():
                shutil.copytree(destination, candidate, dirs_exist_ok=True)
            for name in owned:
                (candidate / name).unlink()
            for name, path in fresh.items():
                (candidate / name).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, candidate / name)
            staging.validate_stage(root, sources, candidate, allow_unowned=True)
            if presentation_inputs != project_inputs(root):
                raise ValueError('Presentation inputs changed during preparation')
            if schema_inputs != list(staging.schema_sources(root).values()):
                raise ValueError('Schema archives changed during preparation')
            if any(sorted(staging.files_in(path)) != members for path, members in installed_members.items()):
                raise ValueError('Installed content membership changed during preparation')
            if any(part.is_symlink() for path in snapshots for part in (path, *path.parents)):
                raise ValueError('Content inputs became symlinks during preparation')
            if sources != sorted(discover_author_sources(root)) or any(path.read_bytes() != data for path, data in snapshots.items()):
                raise ValueError('Content inputs changed during preparation')
            if destination.exists():
                backup = Path(tempfile.mkdtemp(prefix='.content-backup-', dir=destination.parent))
                backup.rmdir()
                destination.replace(backup)
            try:
                candidate.replace(destination)
            except OSError as exc:
                if backup is not None:
                    try:
                        backup.replace(destination)
                    except OSError as rollback:
                        raise RuntimeError(f'Content publication and rollback failed; recover previous stage at {backup}: {rollback}') from exc
                raise RuntimeError('Content publication failed; previous stage restored: ' + str(exc)) from exc
            if backup is not None:
                shutil.rmtree(backup)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise RuntimeError('Content staging failed: ' + str(exc)) from exc
    finally:
        if candidate is not None and candidate.exists():
            shutil.rmtree(candidate)
        # 未成功恢复的backup故意保留；不得在finally删除最后完整一代。


def run_tests(engine: str) -> str:
    """导入实际资源，再执行规则、两代存档和真实场景测试。"""
    version = run_checked([engine,'--version']).strip()
    if not version.startswith('4.') or 'mono' in version.lower():
        raise RuntimeError(f'Godot 4 standard (GDScript) editor required, received {version!r}')
    stage_content()
    evidence = ROOT/'verification'/'runtime'
    run_checked([engine,'--headless','--path',str(GAME),'--editor','--quit'],log=evidence/'import.log')
    for test in ['test_catalog.gd','test_foundation.gd','test_rules.gd','test_storage.gd','test_scene.gd']:
        output = run_checked([engine,'--headless','--path',str(GAME),'--script',f'res://tests/{test}'],log=evidence/(test+'.log'))
        print(output,end='')
    return version



def main(argv: list[str] | None = None) -> int:
    """执行指定阶段；前提缺失或任何失败均返回非零状态。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage',choices=['stage','test','export'])
    parser.add_argument('--content-set', action='append', type=Path, default=[],
                        help='Install one verified compose generation; repeat to retain multiple exact versions')
    parser.add_argument('--profile', choices=['native-growth', 'native-travel', 'native-resident'], help='Explicit isolated Linux native profile')
    parser.add_argument('--output', type=Path, help='New isolated native stage or resident Linux export directory')
    parser.add_argument('--native-library', type=Path, help='Existing reviewed local Bridge; never downloaded or rebuilt')
    parser.add_argument('--native-build-receipt', type=Path, help='Actual completed resident build receipt')
    parser.add_argument('--profile-config', type=Path, help='Explicit resident release profile configuration')
    parser.add_argument('--font', type=Path, help='Licensed local CJK font for the resident export')
    parser.add_argument('--font-license', type=Path, help='Original redistribution license for --font')
    parser.add_argument('--font-face-index', type=int, default=0, help='Actual face index in the supplied font collection')
    parser.add_argument('--staged-project', type=Path, help='Previously prepared resident game directory for export')
    args = parser.parse_args(argv)
    try:
        resident_options = [args.native_build_receipt, args.profile_config, args.font, args.font_license, args.staged_project]
        if args.profile != 'native-resident' and (any(value is not None for value in resident_options) or args.font_face_index != 0):
            raise RuntimeError('Resident-only arguments require --profile native-resident')
        if args.profile == 'native-resident':
            if args.stage == 'stage':
                if args.output is None or args.native_library is None or args.native_build_receipt is None or args.staged_project is not None:
                    raise RuntimeError('native-resident stage requires --output, --native-library and --native-build-receipt')
                try:
                    from tools.native_resident_profile import prepare_profile
                except ModuleNotFoundError:
                    from native_resident_profile import prepare_profile
                result = prepare_profile(ROOT, args.output, args.native_library, args.content_set,
                    native_build_receipt=args.native_build_receipt, profile_config=args.profile_config,
                    font=args.font, font_license=args.font_license, font_face_index=args.font_face_index)
                print(json.dumps({'ok': True, 'profile': args.profile, 'game': result['game'],
                                  'manifest': result['manifest_path'], 'runtime': 'NOT_RUN'}, ensure_ascii=False))
                return 0
            if (args.stage != 'export' or args.output is None or args.staged_project is None or args.content_set or args.font_face_index != 0
                    or any(value is not None for value in [args.native_library, args.native_build_receipt, args.profile_config, args.font, args.font_license])):
                raise RuntimeError('native-resident export requires only --staged-project and --output; stage inputs must be frozen first')
            try:
                from tools.release import main as release_main
            except ModuleNotFoundError:
                from release import main as release_main
            return release_main(['export', '--profile', 'native-resident', '--staged-project', str(args.staged_project), '--output', str(args.output)])
        if args.stage == 'stage':
            if args.profile in {'native-growth', 'native-travel'}:
                if args.output is None or args.native_library is None:
                    raise RuntimeError('native-growth requires --output and --native-library')
                try:
                    from tools.native_growth_profile import prepare_profile
                except ModuleNotFoundError:
                    from native_growth_profile import prepare_profile
                result = prepare_profile(ROOT, args.output, args.native_library, content_sets=args.content_set, **({'profile': args.profile} if args.profile != 'native-growth' else {}))
                print(json.dumps({'ok': True, 'profile': args.profile, 'game': result['game'],
                                  'manifest': result['manifest_path'], 'runtime': 'NOT_RUN'}, ensure_ascii=False))
                return 0
            if args.output is not None or args.native_library is not None:
                raise RuntimeError('--output and --native-library require --profile native-growth')
            stage_content(content_sets=args.content_set)
            print('Content staged; runtime tests are not implied.')
            return 0
        if args.profile is not None or args.output is not None or args.native_library is not None:
            raise RuntimeError('Native development arguments are only valid with stage')
        if args.content_set:
            raise RuntimeError('--content-set is only valid with stage')
        if args.stage == 'export':
            from release import main as release_main
            return release_main(['export', '--platform', 'all'])
        run_tests(find_engine())
        return 0
    except RuntimeError as exc:
        print(f'BLOCKED/FAILED: {exc}',file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
