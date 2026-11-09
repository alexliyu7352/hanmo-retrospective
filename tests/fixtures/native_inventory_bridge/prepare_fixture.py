"""复制已冻结真实采购轻工程和旧oracle，加入隔离扩展；不重建作者世界或下载依赖。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]


def digest(path: Path) -> str:
    """摘要针对实际字节，用于证明准备过程未改写旧工件。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def attach(output: Path, library: Path, template_path: Path) -> dict:
    """仅给已完成纯GDScript导入的本片工程加扩展，不能把该流程冒称首editor发现通过。"""
    output=output.resolve(strict=True)
    preparation=json.loads((output/'preparation.json').read_text())
    game=output/'game'; destination=game/'native_inventory_bridge'
    if preparation.get('format')!='HM-NATIVE-BRIDGE-FIXTURE/1' or preparation.get('extension_present'):
        raise ValueError('Expected this helper\'s unextended fixture')
    if not (game/'.godot/global_script_class_cache.cfg').is_file():
        raise ValueError('Import the pure GDScript project before attaching the extension')
    if (destination/'hanmo_world.gdextension').exists() or (destination/'bin').exists():
        raise ValueError('Extension destination already exists; do not replace a running or frozen library')
    for path,expected in preparation['unchanged_core_files'].items():
        if digest(game/path)!=expected:
            raise ValueError(f'Prepared core changed: {path}')
    source=library.resolve(strict=True); template=template_path.resolve(strict=True).read_text()
    if template.count('@HANMO_BRIDGE_LIBRARY@')!=1:
        raise ValueError('Extension template must have exactly one library token')
    target=destination/'bin'/source.name; target.parent.mkdir(); shutil.copy2(source,target)
    extension=destination/'hanmo_world.gdextension'
    extension.write_text(template.replace('@HANMO_BRIDGE_LIBRARY@',f'res://native_inventory_bridge/bin/{source.name}'))
    receipt={'format':'HM-NATIVE-BRIDGE-ATTACHMENT/1','game':str(game),'library_sha256':digest(target),'extension_sha256':digest(extension),'preparation_sha256':digest(output/'preparation.json'),'editor_first_discovery':'BLOCKED: new extension classes cause SIGABRT with this exact SDK/binding; this fixture imported pure GDScript before attachment','runtime_entry':'fresh headless process; explicit GDExtensionManager.load_extension'}
    (output/'attachment.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n')
    return receipt


def prepare(args: argparse.Namespace) -> dict:
    """只写全新输出；缺源或摘要不符立即失败，旧工程、预期与二进制保持原样。"""
    prepared, cases, audits = [value.resolve(strict=True) for value in (args.prepared_game, args.cases_root, args.cli_audits_root)]
    output = args.output.resolve()
    if output.exists() or any(source == output or source in output.parents for source in (prepared,cases,audits,ROOT/'game',ROOT/'native',ROOT/'tests',ROOT/'content')):
        raise ValueError('Choose a new isolated output directory outside input/source paths')
    manifest = json.loads((cases/'cases.json').read_text())
    if len(manifest['cases']) != 5:
        raise ValueError('Expected five frozen original oracle cases')
    copied = {}
    for row in manifest['cases']:
        for key, sha_key in [('artifact','input_sha256'),('queries','queries_sha256'),('oracle','oracle_sha256')]:
            path = cases/row[key]
            if digest(path) != row[sha_key]:
                raise ValueError(f'Frozen input mismatch: {path}')
    shutil.copytree(prepared,output/'game',ignore=shutil.ignore_patterns('.godot','*.log'))
    game = output/'game'; destination = game/'native_inventory_bridge'; destination.mkdir()
    for row in manifest['cases']:
        for key in ('artifact','queries','oracle'):
            source = cases/row[key]; target = destination/row[key]; target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,target); copied[row[key]]=digest(target)
        source = audits/row['id']/'audit.json'; target = destination/'cli_audits'/f"{row['id']}.json"
        target.parent.mkdir(exist_ok=True); shutil.copy2(source,target); copied[str(target.relative_to(destination))]=digest(target)
    shutil.copy2(cases/'cases.json',destination/'cases.json')
    test = ROOT/'game/tests/test_native_inventory_bridge.gd'
    shutil.copy2(test,game/'tests'/test.name)
    project = game/'project.godot'; text=project.read_text()
    if text.count('[application]') != 1:
        raise ValueError('Prepared project application section is not unique')
    project.write_text(text.replace('[application]','[application]\nconfig/native_inventory_bridge_isolated=true',1))
    source_core={str(p.relative_to(prepared)):digest(p) for p in (prepared/'core').rglob('*.gd')}
    copied_core={str(p.relative_to(game)):digest(p) for p in (game/'core').rglob('*.gd')}
    if source_core!=copied_core:
        raise ValueError('Kernel/core bytes changed while preparing isolated bridge')
    report={'format':'HM-NATIVE-BRIDGE-FIXTURE/1','game':str(game),'frozen_cases_sha256':digest(cases/'cases.json'),'test_sha256':digest(test),'library_sha256':None,'input_files':copied,'unchanged_core_files':source_core,'oracle_policy':'Frozen original Inventory.count expected values; never passed to native bridge','extension_present':False}
    (output/'preparation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return report


def main() -> int:
    """先准备纯GD输入，真实导入后再显式attach；不自动重试失败editor或修改默认游戏。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepared-game',type=Path)
    parser.add_argument('--cases-root',type=Path)
    parser.add_argument('--cli-audits-root',type=Path)
    parser.add_argument('--attach-to',type=Path)
    parser.add_argument('--library',type=Path)
    parser.add_argument('--extension-template',type=Path,default=ROOT/'native/godot/hanmo_world.gdextension.in')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.attach_to:
        if not args.library or any((args.prepared_game,args.cases_root,args.cli_audits_root,args.output)):
            parser.error('--attach-to requires --library and no preparation inputs/output')
        report=attach(args.attach_to,args.library,args.extension_template)
    else:
        if args.library or not all((args.prepared_game,args.cases_root,args.cli_audits_root,args.output)):
            parser.error('Preparation requires --prepared-game, --cases-root, --cli-audits-root and --output; attach library after pure GD import')
        report=prepare(args)
    print(json.dumps({key:report[key] for key in ('format','game','library_sha256')},ensure_ascii=False))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
