"""复用三条已存在真实作者来源，准备GD codec/旧oracle；不自动下载、运行native或存读。"""
from __future__ import annotations
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def module(path: Path, name: str):
    """加载既有正式准备器，保留其作者与验证规则。"""
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec); spec.loader.exec_module(loaded)
    return loaded


def write(path: Path, value) -> None:
    """只写明确的新准备目录，JSON字节完整可复核。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n')


def command(args: list[str], cwd: Path, log: Path, env=None) -> None:
    """保留原stdout/stderr与退出，任意引擎诊断不作绿。"""
    result = subprocess.run(args, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(result.stdout + '\nEXIT_CODE=' + str(result.returncode) + '\n')
    if result.returncode or 'SCRIPT ERROR' in result.stdout or 'ERROR:' in result.stdout:
        raise RuntimeError('Preparation/run failed; inspect ' + str(log))


def prepare_family(root: Path, output: Path, art: Path, evidence: Path) -> None:
    """精确复用effective-adoption既有钱包变化，经同一compose/stage，不跑原冷恢复。"""
    output.mkdir()
    for name in ['tools','schemas','game/core']:
        shutil.copytree(root/name, output/name, ignore=shutil.ignore_patterns('__pycache__','*.pyc','*.uid','.godot'))
    data=output/'game/data'; data.mkdir(parents=True)
    for name in ['rule_capabilities.json','module_contracts.json','pack_runtime.json','presentation.schema.json']:
        shutil.copy2(root/'game/data'/name, data/name)
    base=root/'tests/fixtures/content_packs/compose/new_packs/base'; author=output/'author'
    shutil.copytree(base, author, ignore=shutil.ignore_patterns('assets'))
    for name in ['pixel.png','portrait.png']:
        path=author/'assets/art'/name; path.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(art/name,path)
    spec=json.loads((root/'tests/fixtures/a2_effective_adoption/author_changes.json').read_text())
    raw=(root/spec['source']).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=spec['source_sha256']: raise ValueError('Original adoption author changed')
    source=json.loads(raw); wallet=spec['wallet']; source['wallets'].append(deepcopy(wallet))
    for scenario in source['scenarios']:
        scenario['enabled']['wallets'].append(wallet['id']); scenario['initial']['wallets'].append(deepcopy(wallet))
    manifest=json.loads((author/'pack.json').read_text()); manifest.update(id='effective-adoption',namespace='adopt',required_capabilities=source['required_capabilities'])
    write(author/'pack.json',manifest); write(author/'content.json',source)
    write(output/'content/foundation/effective_adoption.json',source)
    shutil.copy2(author/'presentation.json', data/'presentation.json'); shutil.copytree(author/'assets',output/'game/assets')
    write(output/'selection.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'effective-adoption','version':'1.0.0'}]})
    command([sys.executable,str(output/'tools/content_pipeline.py'),'compose','--packs',str(author),'--selection',str(output/'selection.json'),'--output',str(output/'composed')],output,evidence/'family-compose.log')
    command([sys.executable,'-c',"from pathlib import Path; from tools.build import stage_content; stage_content(Path.cwd(),content_sets=[Path('composed')],foundation_sets=False)"],output,evidence/'family-stage.log')
    (output/'game/tests').mkdir()
    (output/'game/project.godot').write_text('config_version=5\n[application]\nconfig/name="Native legacy family sources"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')


def prepare(root: Path, output: Path, art: Path) -> dict:
    """三个轻工程只复制文字与原tiny图；production源与玩家数据不变。"""
    root,output,art=root.resolve(),output.resolve(),art.resolve(strict=True)
    if output.exists() or output==root or any(output.is_relative_to(root/p) for p in ['game','tools','tests','schemas','content','native']):
        raise ValueError('Require a fresh isolated output')
    output.mkdir(parents=True); evidence=output/'evidence'; evidence.mkdir()
    module(root/'tests/fixtures/native_inventory_projection/prepare_fixture.py','m1_procurement').prepare(root,output/'procurement',art)
    module(root/'tests/fixtures/legacy_activity_projection/prepare_fixture.py','m1_military').prepare(root,output/'military',art)
    prepare_family(root,output/'family',art,evidence)
    commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    metadata={'source_commit':commit,'binding':'trusted local build metadata; not world identity',
              'codec_sha256':hashlib.sha256((root/'game/core/backends/legacy_projection_codec.gd').read_bytes()).hexdigest()}
    for name in ['procurement','military','family']:
        game=output/name/'game'
        write(game/'data/native_legacy_export_source.json',metadata)
        shutil.copy2(root/'game/tests/export_native_legacy_projection.gd',game/'tests/export_native_legacy_projection.gd')
        scripts=['source_capture','capture_ownership']+{'procurement':['collect_procurement','collect_procurement_edges'],'military':['collect_military'],'family':['collect_family']}[name]
        for script in scripts: shutil.copy2(HERE/(script+'.gd'),game/'tests'/(script+'.gd'))
    write(output/'preparation.json',metadata)
    return metadata


def run(output: Path, godot: Path) -> None:
    """只执行源前缀、一次save_context直出与受影响codec门，不运行旧全门或Native。"""
    env=os.environ.copy()
    for key,name in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('TMPDIR','tmp')]:
        folder=output/'environment'/name; folder.mkdir(parents=True,exist_ok=True); env[key]=str(folder)
    version=subprocess.check_output([str(godot),'--version'],env=env,text=True).strip()
    if version!='4.7.2.stable.official.ed1daf0bf': raise ValueError('Require verified fixed Godot 4.7.2')
    for name in ['procurement','military','family']:
        command([str(godot),'--headless','--editor','--path',str(output/name/'game'),'--import','--quit'],output,output/'evidence'/(name+'-import.log'),env)
    for name,script in [('procurement','collect_procurement'),('procurement','collect_procurement_edges'),('military','collect_military'),('family','collect_family')]:
        command([str(godot),'--headless','--path',str(output/name/'game'),'--script','res://tests/'+script+'.gd','--','--output='+str(output/'sources')],output,output/'evidence'/(script+'.log'),env)
    command([str(godot),'--headless','--path',str(output/'procurement/game'),'--script','res://tests/capture_ownership.gd','--','--output='+str(output/'ownership-source.json')],output,output/'evidence/ownership.log',env)
    command([str(godot),'--headless','--path',str(output/'procurement/game'),'--script','res://tests/export_native_legacy_projection.gd','--','--sources='+str(output/'sources')],output,output/'evidence/codec.log',env)
    for mode in ['weak-facts-only','modules-only']:
        command([str(godot),'--headless','--path',str(output/'procurement/game'),'--script','res://tests/export_native_legacy_projection.gd','--','--sources='+str(output/'sources'),'--'+mode],output,output/'evidence'/(mode+'.log'),env)
    cases=[]
    for path in sorted((output/'exports').iterdir()):
        if path.is_dir(): cases.append({'id':path.name,'files':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(path.glob('*.json'))},'source_sha256':hashlib.sha256((output/'sources'/path.name/'source.json').read_bytes()).hexdigest()})
    write(output/'exports/cases.json',{'format':'HM-NATIVE-LEGACY-SAMPLES/1','cases':cases,'scope':'artifact-only; not-complete-M1; native=NOT_RUN'})


def main() -> None:
    """--run才运行已授权Godot；默认只准备，不隐式取得新软件或网络许可。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root',type=Path,default=ROOT); parser.add_argument('--output',type=Path,required=True); parser.add_argument('--original-art',type=Path,required=True)
    parser.add_argument('--run',action='store_true'); parser.add_argument('--godot',type=Path)
    args=parser.parse_args(); output=args.output.resolve(); metadata=prepare(args.source_root,output,args.original_art)
    if args.run:
        if not args.godot: parser.error('--run requires --godot')
        run(output,args.godot.resolve(strict=True))
    print(json.dumps({'output':str(output),**metadata},ensure_ascii=False))


if __name__=='__main__': main()
