#!/usr/bin/env python3
"""统一验证及导出同一工程的 Linux 与 Web；内部联调包明确记录边界，不上传或覆盖旧发行。"""
from pathlib import Path
import argparse
import hashlib
import json
import uuid
import os
import re
import subprocess
import sys
import shutil
import tempfile
try:
    from tools import content_stage, build
    from tools.release_linux import package_directory
except ModuleNotFoundError:
    import content_stage
    import build
    from release_linux import package_directory
ROOT = Path(__file__).resolve().parents[1]
LOCAL = Path(os.environ.get('HANMO_ROOT', '<本地路径>'))
def release_config(root: Path, local: Path) -> dict:
    """读取工程版本并派生发布目录；非法版本不得改变路径。"""
    project = (root / 'game/project.godot').read_text()
    match = re.search(r'^config/version="([0-9]+\.[0-9]+\.[0-9]+)"$', project, re.MULTILINE)
    if not match:
        raise ValueError('项目必须配置完整的三段数字版本号')
    version = match.group(1)
    setting = re.search(r'^config/storage_profile="([^"]*)"$', project, re.MULTILINE)
    profile = setting.group(1) if setting else 'player-f05'
    if not re.fullmatch(r'[a-z][a-z0-9-]{0,63}', profile):
        raise ValueError('存档配置只能使用短字母数字标识')
    return {'version': version, 'folder': local / 'builds' / ('f' + version) / 'hanmo-linux', 'save_profile': profile}

CONFIG = release_config(ROOT, LOCAL)
LOGS = LOCAL / 'verification' / ('f' + CONFIG['version'])
FOLDER = CONFIG['folder']
try:
    from tools import verify as verification
    from tools.verify import (EXCLUDED_TESTS, TESTS, assertion_count, prepare_military_content, test_arguments)
except ModuleNotFoundError:
    import verify as verification
    from verify import (EXCLUDED_TESTS, TESTS, assertion_count, prepare_military_content, test_arguments)


def validate_test_registry(game: Path) -> dict:
    """兼容既有清单门禁接口；唯一执行规则来自 verify，当前调用方分类仍须完整。"""
    return verification.validate_test_registry(game, tests=TESTS, excluded=EXCLUDED_TESTS)


def write_launcher(folder: Path) -> None:
    """复制已检查的启动器，不安装引擎或修改系统设置。"""
    target = folder/'start.sh'
    target.write_text((ROOT/'docs/release/start.sh').read_text().replace('@SAVE_PROFILE@', CONFIG['save_profile']), encoding='utf-8')
    target.chmod(0o755)

def run_suite(engine: str, profile: str = 'release') -> dict:
    """委托唯一验证清单并检查新回执；失败或陈旧证据不得进入正式导出。"""
    validate_test_registry(build.GAME)
    report = verification.run_verification(ROOT, profile, LOGS, engine)
    return verification.require_receipt(Path(report['receipt']), ROOT, profile)

def stage_release_content(sources: list[Path]) -> list[dict]:
    """冻结显式选择的正式组合代并沿原staging安装；不隐式组合作者根或删除已登记代。"""
    root = build.GAME.parent
    selected = {}
    for source in sources:
        record = content_stage.composed_set_record(source, root)
        identity = record['pack_set_digest']
        if identity in selected and selected[identity] != record:
            raise RuntimeError('Selected content identity has different members')
        selected[identity] = record
    expected = [selected[key] for key in sorted(selected)]
    build.stage_content(root, content_sets=sources, expected_content_sets=expected)
    collect_content(expected)
    return expected


def collect_content(expected_sets: list[dict] | tuple = ()) -> dict:
    """核整代来源与所有成员，并复核显式选择未被后续验证或导出换成另一内容锁。"""
    root = build.GAME.parent
    manifest = content_stage.validate_stage(root, build.discover_author_sources(root))
    result = {'stage': manifest, 'expected_files': content_stage.package_files(root, manifest)}
    if expected_sets:
        installed = content_stage.read_pack(root / 'game/data/generated' / content_stage.INSTALLED_INDEX)
        actual = {row['pack_set_digest']: row for row in installed['sets']}
        for record in expected_sets:
            if actual.get(record['pack_set_digest']) != record:
                raise RuntimeError('Selected content set changed or disappeared before export')
        result['selected_content_sets'] = json.loads(json.dumps(expected_sets))
    return result


def inspect_content_package(engine: str, package: Path, content: dict, evidence: Path) -> dict:
    """在独立空工程读取真实包的资源和摘要；不允许本地generated补齐缺漏。"""
    if collect_content(content.get('selected_content_sets', ())) != content:
        raise RuntimeError('Content changed during export')
    evidence.mkdir(parents=True, exist_ok=True)
    report_path = evidence / 'content-package.json'
    if report_path.exists():
        raise RuntimeError('Content package evidence already exists: ' + str(report_path))
    with tempfile.TemporaryDirectory(prefix='content-probe-') as temporary:
        isolated = Path(temporary)
        (isolated / 'project.godot').write_text('config_version=5\n')
        command = [engine, '--headless', '--path', str(isolated), '--script',
                   str(Path(__file__).with_name('content_package_probe.gd')), '--',
                   str(package.resolve()), str(report_path.resolve())]
        build.run_checked(command, timeout=120, log=evidence / 'content-package.log')
    try:
        actual = json.loads(report_path.read_text())
        if (not isinstance(actual, dict) or set(actual) != {'format', 'files'}
                or actual['format'] != 'HM-CONTENT-PACKAGE/1' or not isinstance(actual['files'], list)
                or any(not isinstance(row, dict) or set(row) != {'path', 'sha256'}
                       or not isinstance(row['path'], str) for row in actual['files'])
                or sorted(actual['files'], key=lambda row: row['path']) != content['expected_files']):
            raise ValueError('exported package members or hashes differ from verified content')
    except (OSError, ValueError, TypeError, KeyError) as exc:
        raise RuntimeError('Content package verification failed: ' + str(exc)) from exc
    actual['files'] = content['expected_files']
    actual['sha256'] = content_stage.digest(package)
    result = {'stage': content['stage'], 'package': actual}
    if 'selected_content_sets' in content:
        result['selected_content_sets'] = content['selected_content_sets']
    return result


def export_native(engine: str, tests: dict, *, expected_sets: list[dict] | tuple = ()) -> Path:
    """导出已提交源码和许可完整的中文字体；保留旧产物，图形操作验收单独记录。"""
    from prepare_font import prepared_font
    if FOLDER.exists() and any(FOLDER.iterdir()):
        raise RuntimeError('输出目录非空，保留此前产物：'+str(FOLDER))
    archive = FOLDER.parent/('hanmo_'+CONFIG['version']+'_linux_x86_64.zip')
    if archive.exists():
        raise RuntimeError('输出压缩包已存在，保留此前产物：'+str(archive))
    font = prepared_font(build.GAME)
    changes = subprocess.check_output(['git', 'status', '--porcelain', '--', 'game', 'content', 'schemas', 'tools'], cwd=ROOT, text=True)
    if changes.strip():
        raise RuntimeError('Commit the verified production changes before export:\n'+changes)
    if tests.get('format') != 'HM-VERIFY/1' or tests.get('profile') not in {'linux', 'release'}:
        raise RuntimeError('原生导出要求 linux/release HM-VERIFY/1 验证回执与 profile')
    tests = verification.require_receipt(Path(tests['receipt']), ROOT, tests['profile'])
    content = collect_content(expected_sets)
    evidence = Path(tests['receipt']).parent / ('native-export-' + uuid.uuid4().hex[:8])
    font_output = build.run_checked([engine,'--headless','--path',str(build.GAME),'--script','res://tests/test_export_font.gd'],
                                    log=evidence/'export-font.log')
    font_checks = assertion_count(font_output)
    FOLDER.mkdir(parents=True, exist_ok=True)
    binary = FOLDER/'hanmo.x86_64'
    command = [engine, '--headless', '--path', str(build.GAME), '--export-release', 'Linux', str(binary)]
    build.run_checked(command, timeout=300, log=evidence/'export-linux.log')
    if not binary.is_file() or binary.read_bytes()[:4] != b'\x7fELF':
        raise RuntimeError('No Linux executable was produced')
    content = inspect_content_package(engine, binary, content, evidence)
    binary.chmod(0o755)
    write_launcher(FOLDER)
    for source, name in [('docs/release/PLAYER.md', 'README.md'), ('docs/release/ENGINE-LICENSES.txt', 'ENGINE-LICENSES.txt')]:
        (FOLDER/name).write_bytes((ROOT/source).read_bytes())
    shutil.copy2(font['license_path'],FOLDER/'FONT-LICENSE.txt')
    manifest = {'version': CONFIG['version'], 'engine': tests['engine'],
                'code_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'binary': {'file': binary.name, 'bytes': binary.stat().st_size, 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()},
                'tests': tests, 'acceptance': {'native_mouse': 'PENDING', 'hardware_startup': 'PENDING', 'web': 'NOT_EXPORTED'},
                'content': content, 'font_files_included': True,
                'font': {key:value for key,value in font.items() if key not in ['path','license_path']},
                'font_glyph_checks': font_checks,
                'storage_default': 'XDG base directories', 'storage_root_override': 'HANMO_STATE_ROOT',
                'save_profile': CONFIG['save_profile']}
    (FOLDER/'BUILD.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    package_directory(FOLDER, archive)
    if tests.get('format') == 'HM-VERIFY/1':
        receipt = verification.record_build_artifacts(tests, ROOT, [path for path in FOLDER.iterdir() if path.is_file()] + [archive],
                                                      'native', command, evidence/'export-linux.log', LOGS)
        print('构建验证回执：' + receipt['receipt'])
    return binary


def export_web(engine: str, tests: dict, folder: Path | None = None, *, internal: bool = False, logs: Path | None = None, expected_sets: list[dict] | tuple = ()) -> Path:
    """导出完整浏览器资源及字体许可；内部包记录工作区差异，正式包仍要求已提交且验证通过。"""
    from prepare_font import prepared_font
    font = prepared_font(build.GAME)
    changes = subprocess.check_output(['git','status','--porcelain','--','game','content','schemas','tools','web'],cwd=ROOT,text=True)
    if changes.strip() and not internal:
        raise RuntimeError('正式 Web 导出前必须提交已验证的工程变更：\n'+changes)
    if internal and folder is None:
        raise RuntimeError('内部 Web 包必须显式指定新输出目录。')
    target_folder = folder or FOLDER.parent/'hanmo-web'
    if target_folder.exists() and any(target_folder.iterdir()):
        raise RuntimeError('输出目录非空，保留此前产物：'+str(target_folder))
    if not internal:
        if tests.get('format') != 'HM-VERIFY/1' or tests.get('profile') not in {'web', 'release'}:
            raise RuntimeError('Web 导出要求 web/release HM-VERIFY/1 验证回执与 profile')
        tests = verification.require_receipt(Path(tests['receipt']), ROOT, tests['profile'])
        evidence = Path(tests['receipt']).parent / ('web-export-' + uuid.uuid4().hex[:8])
    else:
        evidence = (logs or LOGS) / ('web-export-' + uuid.uuid4().hex[:8])
    content = collect_content(expected_sets)
    target_folder.mkdir(parents=True,exist_ok=True)
    web = target_folder/'index.html'
    font_output = build.run_checked([engine,'--headless','--path',str(build.GAME),'--script','res://tests/test_export_font.gd'],
                                    log=evidence/'export-font.log')
    font_checks = assertion_count(font_output)
    command = [engine,'--headless','--path',str(build.GAME),'--export-release','Web',str(web)]
    build.run_checked(command, timeout=600,log=evidence/'export-web.log')
    for extension in ['.html','.js','.wasm','.pck']:
        artifact = web.with_suffix(extension)
        if not artifact.is_file() or not artifact.stat().st_size:
            raise RuntimeError('Web 导出缺少必要文件：'+str(artifact))
    if web.with_suffix('.wasm').read_bytes()[:4] != b'\x00asm':
        raise RuntimeError('导出的引擎不是有效 WebAssembly 文件。')
    content = inspect_content_package(engine, web.with_suffix('.pck'), content, evidence)
    for source,name in [(ROOT/'web/storage_bridge.js','storage_bridge.js'),
                        (font['license_path'],'FONT-LICENSE.txt'),
                        (ROOT/'docs/release/ENGINE-LICENSES.txt','ENGINE-LICENSES.txt')]:
        shutil.copy2(source,target_folder/name)
    (target_folder/'README.md').write_text('汉末行记 Web\n\n使用 HTTP 服务打开整个目录，不能直接双击 index.html。\n'
        '进入旅途后可保存、继续、导出和导入存档；浏览器数据按独立发行配置保存。\n'
        '中文字库随游戏内嵌，其完整版权许可见 FONT-LICENSE.txt；引擎许可见 ENGINE-LICENSES.txt。\n'
        '此为内部联调包，尚未完成最终 Linux/Web 代表存档互通验收。\n' if internal else
        '汉末行记 Web\n\n使用 HTTP 服务打开整个目录。字体版权许可见 FONT-LICENSE.txt；引擎许可见 ENGINE-LICENSES.txt。\n',encoding='utf-8')
    artifacts = [{'file':path.name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
                 for path in sorted(target_folder.iterdir()) if path.is_file()]
    manifest = {'version':CONFIG['version'],'platform':'web','internal':internal,'engine':tests['engine'],
                'code_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'working_tree_changes':changes.splitlines(),'tests':tests,'artifacts':artifacts,'content':content,
                'font':{key:value for key,value in font.items() if key not in ['path','license_path']},
                'font_glyph_checks':font_checks,
                'save_profile':CONFIG['save_profile'],
                'acceptance':{'web_input':'PENDING','web_chinese':'PENDING','web_persistence':'PENDING',
                              'web_download_import':'PENDING','web_linux_save_roundtrip':'PENDING'}}
    (target_folder/'BUILD.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    if tests.get('format') == 'HM-VERIFY/1' and not internal:
        receipt = verification.record_build_artifacts(tests, ROOT, [path for path in target_folder.iterdir() if path.is_file()],
                                                      'web', command, evidence/'export-web.log', LOGS)
        print('构建验证回执：' + receipt['receipt'])
    return web


def main(argv: list[str] | None = None) -> int:
    """执行本地验证、导出或打包，不把编译成功当作玩家验收。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=['test', 'export', 'package'])
    parser.add_argument('--platform',choices=['linux','web','all'],default='linux')
    parser.add_argument('--internal',action='store_true',help='仅内部 Web 联调；不声明发行测试或实机验收已通过')
    parser.add_argument('--output',type=Path,help='内部 Web 包的新目录，不覆盖非空路径')
    parser.add_argument('--profile', choices=['native-resident'], help='本章独立resident Linux试玩导出/打包')
    parser.add_argument('--staged-project', type=Path, help='build.py stage准备的resident game目录')
    parser.add_argument('--archive', type=Path, help='resident package的新ZIP目标')
    parser.add_argument('--content-set', action='append', type=Path, default=[],
                        help='显式安装正式compose代并绑定发行锁/成员；可重复，原已登记代保留并列入发行清单')
    args = parser.parse_args(argv)
    if args.profile == 'native-resident':
        if args.platform != 'linux' or args.internal or args.content_set or args.output is None:
            parser.error('native-resident仅用于Linux，要求--output，不重选内容或使用--internal')
        try:
            from tools.native_resident_profile import export_profile, package_profile
        except ModuleNotFoundError:
            from native_resident_profile import export_profile, package_profile
        if args.stage == 'package':
            if args.staged_project is not None:
                parser.error('resident package只消费--output中的原BUILD.json')
            print(package_profile(args.output, args.archive))
            return 0
        if args.stage != 'export' or args.staged_project is None or args.archive is not None:
            parser.error('resident export要求--staged-project与新--output；此入口不代中央Main验收')
        result = export_profile(ROOT, args.staged_project, args.output, build.find_engine())
        print(json.dumps({'ok': True, 'profile': args.profile, 'folder': result['folder'],
                          'manifest': result['manifest_path'], 'acceptance': result['manifest']['acceptance']}, ensure_ascii=False))
        return 0
    if args.staged_project is not None or args.archive is not None:
        parser.error('--staged-project/--archive仅用于--profile native-resident')
    if args.content_set and args.stage == 'package':
        parser.error('--content-set只用于test/export；package必须消费原BUILD清单，不能重新选择内容')
    if args.internal and (args.stage != 'export' or args.platform != 'web' or args.output is None):
        parser.error('--internal 只用于 export --platform web --output 新目录')
    if args.output is not None and not args.internal:
        parser.error('--output 仅用于显式内部 Web 导出；正式发行目录来自工程版本')
    if args.stage == 'package':
        if args.platform == 'all': parser.error('打包时请逐个平台核对清单')
        if args.platform == 'web':
            folder = FOLDER.parent/'hanmo-web'
            manifest = json.loads((folder/'BUILD.json').read_text())
            for artifact in manifest['artifacts']:
                if hashlib.sha256((folder/artifact['file']).read_bytes()).hexdigest() != artifact['sha256']:
                    raise RuntimeError('Web 资源与发布清单不一致：'+artifact['file'])
            package_directory(folder,FOLDER.parent/('hanmo_'+CONFIG['version']+'_web.zip'))
            return 0
        manifest = json.loads((FOLDER/'BUILD.json').read_text())
        actual = hashlib.sha256((FOLDER/'hanmo.x86_64').read_bytes()).hexdigest()
        if actual != manifest['binary']['sha256']:
            raise RuntimeError('Binary does not match the release manifest')
        if manifest.get('font_files_included',False):
            try:
                license_bytes = (FOLDER/'FONT-LICENSE.txt').read_bytes()
                if not license_bytes.strip() or hashlib.sha256(license_bytes).hexdigest() != manifest['font']['license_sha256']:
                    raise RuntimeError('字体许可与发布清单不一致，停止打包。')
            except (OSError,KeyError,TypeError) as exc:
                raise RuntimeError('字体许可或其准备记录缺失，停止打包。') from exc
        package_directory(FOLDER, FOLDER.parent/('hanmo_'+CONFIG['version']+'_linux_x86_64.zip'))
        return 0
    engine = build.find_engine()
    if args.stage == 'export': build.require_font()
    expected_sets = stage_release_content(args.content_set) if args.content_set else []
    if args.internal:
        if args.output.exists() and any(args.output.iterdir()):
            raise RuntimeError('输出目录非空，保留此前产物：'+str(args.output))
        internal_logs = args.output.parent/(args.output.name+'-verification')
        if not expected_sets:
            build.stage_content()
        version = build.run_checked([engine,'--version']).strip()
        build.run_checked([engine,'--headless','--path',str(build.GAME),'--editor','--quit'],log=internal_logs/'import.log')
        report = {'engine':version,'tests':{},'scope':'内部导出；完整发行测试 NOT_RUN'}
    else:
        internal_logs = None
        profile = {'linux': 'linux', 'web': 'web', 'all': 'release'}[args.platform]
        report = run_suite(engine, profile)
    if expected_sets:
        collect_content(expected_sets)
    selection_options = {'expected_sets': expected_sets} if expected_sets else {}
    if args.stage == 'export':
        if args.platform in ['linux','all']: print(export_native(engine,report,**selection_options))
        if args.platform in ['web','all']: print(export_web(engine,report,args.output,internal=args.internal,logs=internal_logs,**selection_options))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
