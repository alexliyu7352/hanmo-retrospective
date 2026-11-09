#!/usr/bin/env python3
"""仅复用已成功原生缓存内的固定引擎，执行三面板真实控件的 mock 合同。"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = '（原私有仓库）'
RUN = 38365397879
NATIVE_SOURCE = '0406e06686dcce90aefb1c43ebb2e6566949ab2f'
CACHE_KEY = 'ubuntu2004-native-6fe46581bd0a29c01048f2a57596a724093144057e3acac34c3c9d22e85449eb-'+NATIVE_SOURCE+'-run-38365397879-attempt-1'
ENGINE_FILE = 'Godot_v4.7.2-stable_linux.x86_64'
ENGINE_VERSION = '4.7.2.stable.official.ed1daf0bf'
ENGINE_SHA = '8d106cbe6144c2dc7e881d61d2429c1a8a76e6b22ef48bd5e48dcf934953f71e'
ENGINE_BYTES = 146414384
LOCK = 'tools/toolchain/godot-4.7.2.json'
TEST = 'game/tests/test_personal_message_single_action_panels.gd'
GD_FILES = ('game/ui/task_personal_acceptances_panel.gd',
            'game/ui/task_personal_messages_panel.gd',
            'game/ui/task_personal_reports_panel.gd', TEST)
CLOSURE = GD_FILES+(LOCK, 'tools/ci/task_panel_risk.py', 'tools/ci/test_task_panel_risk.py',
                    '.github/workflows/task-panel-risk.yml')
PASS_MARKER = 'HANMO_TASK_PANEL_MOCK_PASS'
SCOPE = 'HEADLESS_TASK_PANEL_MOCK_ONLY'
PROJECT = 'config_version=5\n\n[application]\nconfig/name="Hanmo task panel mock risk"\n\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n'


def regular_size(path):
    """只读既有普通文件，拒绝以链接改换已核输入。"""
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise ValueError('Regular fixed input required: '+str(path))
    return path.stat().st_size


def sha256(path):
    regular_size(path)
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


def git(root, *args):
    """这些只读身份命令禁止补取 promisor 对象，缺白名单原件即停。"""
    env = dict(os.environ, GIT_ALLOW_PROTOCOL='', GIT_TERMINAL_PROMPT='0', GIT_NO_LAZY_FETCH='1')
    return subprocess.check_output(['git', '-C', str(root), *args], env=env, stderr=subprocess.PIPE)


def request(key):
    if key != CACHE_KEY:
        raise ValueError('Only the verified successful native cache key is accepted')
    return {'repository':REPOSITORY, 'native_run_id':RUN, 'native_source':NATIVE_SOURCE,
            'native_run_attempt':1, 'native_cache_key':key, 'scope':SCOPE}


def validate_run(run):
    """固定缓存的原生产必须实际成功；新控件源码并不冒其旧产品身份。"""
    expected = {'id':RUN, 'run_attempt':1, 'head_sha':NATIVE_SOURCE,
                'status':'completed', 'conclusion':'success', 'path':'.github/workflows/native-ubuntu2004.yml'}
    if any(run.get(key) != value for key,value in expected.items()) or run.get('repository',{}).get('full_name') != REPOSITORY:
        raise ValueError('Original successful native run metadata differs')


def source_identity(root, expected):
    """只核同提交的八件有限源码；不扫描完整稀疏树触发网络。"""
    if not re.fullmatch(r'[0-9a-f]{40}', expected):
        raise ValueError('A full fixed source commit is required')
    if git(root,'rev-parse','HEAD').decode().strip() != expected:
        raise ValueError('Actual source HEAD differs')
    rows = {}
    for relative in CLOSURE:
        path = root/relative
        regular_size(path)
        data = path.read_bytes()
        raw = git(root,'ls-tree','-z',expected,'--',relative)
        entries = [entry for entry in raw.split(b'\0') if entry]
        if len(entries) != 1:
            raise ValueError('Missing exact tree entry: '+relative)
        header,name = entries[0].split(b'\t',1)
        mode,kind,oid = header.decode().split(' ')
        actual = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        if name.decode() != relative or mode not in ('100644','100755') or kind != 'blob' or oid != actual:
            raise ValueError('Actual source blob differs: '+relative)
        rows[relative] = {'git_blob':oid, 'bytes':len(data), 'sha256':hashlib.sha256(data).hexdigest()}
    return {'source_commit':expected, 'files':rows}


def cached_engine(cache, lock):
    """不下载、不复制引擎，仅核原缓存文件与固定官方原包身份。"""
    entry = lock.get('archives',{}).get('editor',{}).get('entries',{}).get(ENGINE_FILE,{})
    if lock.get('editor_build') != ENGINE_VERSION or entry.get('sha256') != ENGINE_SHA or entry.get('bytes') != ENGINE_BYTES:
        raise ValueError('Source engine lock differs')
    engine = cache/ENGINE_FILE
    if regular_size(engine) != ENGINE_BYTES or sha256(engine) != ENGINE_SHA:
        raise ValueError('Cached fixed engine differs')
    return engine


def prepare_project(source, target, identity):
    """只生成四 GD 的独立小工程，不载入原 autoload、插件或原生库。"""
    target.mkdir(parents=True, exist_ok=False)
    for relative in GD_FILES:
        data = (source/relative).read_bytes()
        if hashlib.sha256(data).hexdigest() != identity['files'][relative]['sha256']:
            raise ValueError('Source changed before staging: '+relative)
        dest = target/(relative[len('game/'):] if relative.startswith('game/') else relative)
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(data)
    (target/'project.godot').write_text(PROJECT)


def validate_output(code, log):
    """Godot 某些脚本错误仍可能退出零；必须有唯一成功标记且无实际错误。"""
    if code != 0 or log.splitlines().count(PASS_MARKER) != 1 or re.search(r'(?m)^\s*(?:SCRIPT |PARSE )?ERROR:',log):
        raise ValueError('Actual task panel mock test did not pass')


def write_json(path, value):
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')


def execute(args):
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    receipt={'scope':SCOPE, 'world_tested':False, 'native_tested':False, 'visual_tested':False,
             'started_at':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'status':'STARTED'}
    source=args.source.resolve(); cache=args.cache.resolve(); before=None;engine=None;exit_code=1
    try:
        receipt['request']=request(args.cache_key)
        run=json.loads(args.native_run.read_text());validate_run(run)
        write_json(output/'native-run.json',run)
        before=source_identity(source,args.source_sha);receipt['source']=before
        write_json(output/'source-before.json',before)
        engine=cached_engine(cache,json.loads((source/LOCK).read_text()))
        receipt['engine']={'path':str(engine),'sha256':ENGINE_SHA,'bytes':ENGINE_BYTES,'version':ENGINE_VERSION}
        project=output/'project';prepare_project(source,project,before)
        env=dict(os.environ,HOME=str(output/'home'),XDG_CACHE_HOME=str(output/'xdg-cache'),
                 XDG_DATA_HOME=str(output/'xdg-data'),XDG_CONFIG_HOME=str(output/'xdg-config'))
        for name in ('HOME','XDG_CACHE_HOME','XDG_DATA_HOME','XDG_CONFIG_HOME'):Path(env[name]).mkdir()
        with (output/'version.log').open('w') as log:
            version=subprocess.run([str(engine),'--headless','--version'],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=30)
        actual=(output/'version.log').read_text().strip()
        if version.returncode != 0 or actual != ENGINE_VERSION:
            raise ValueError('Actual cached engine version differs')
        argv=[str(engine),'--headless','--path',str(project),'--script','res://'+(TEST[len('game/'):] if TEST.startswith('game/') else TEST)]
        receipt['argv']=argv
        with (output/'godot.log').open('w') as log:
            completed=subprocess.run(argv,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        receipt['godot_exit_code']=completed.returncode
        validate_output(completed.returncode,(output/'godot.log').read_text())
        receipt['status']='PASS_NAMED_TASK_PANEL_MOCK_ONLY';exit_code=0
    except (ValueError,OSError,subprocess.SubprocessError,json.JSONDecodeError) as error:
        receipt['status']='FAILED';receipt['error']=str(error)
    finally:
        try:
            if before is not None:
                after=source_identity(source,args.source_sha);write_json(output/'source-after.json',after)
                if after != before:raise ValueError('Source changed during execution')
                if engine is not None and sha256(engine) != ENGINE_SHA:raise ValueError('Cached engine changed')
                project=output/'project'
                if project.exists():
                    for relative in GD_FILES:
                        if sha256(project/(relative[len('game/'):] if relative.startswith('game/') else relative)) != before['files'][relative]['sha256']:
                            raise ValueError('Staged script changed during execution')
                    if (project/'project.godot').read_text() != PROJECT:raise ValueError('Minimal project changed')
                receipt['postflight']='PASS'
        except (ValueError,OSError,subprocess.SubprocessError) as error:
            receipt['postflight']='FAILED';receipt['postflight_error']=str(error);receipt['status']='FAILED';exit_code=1
        receipt['finished_at']=datetime.datetime.now(datetime.timezone.utc).isoformat()
        write_json(output/'receipt.json',receipt)
    return exit_code


def main():
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('request');p.add_argument('--cache-key',required=True)
    p=sub.add_parser('run')
    for name in ('source','cache','output','native-run'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('source-sha','cache-key'):p.add_argument('--'+name,required=True)
    args=parser.parse_args()
    if args.command=='request':print(json.dumps(request(args.cache_key)));return 0
    return execute(args)


if __name__=='__main__':sys.exit(main())
