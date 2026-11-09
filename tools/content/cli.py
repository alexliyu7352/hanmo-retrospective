"""执行内容校验、单源prepare与显式包组合；失败保留非零状态和机器诊断。"""
import argparse
import json
from pathlib import Path
from .validation import validate_pack
from .io import read_pack, prepare_pack, check_target
from .pack_compose import compose_pack_set
from .pack_errors import PackError


def main(argv=None) -> int:
    """执行一个静态内容阶段；保留旧命令状态码，组合另报稳定PackError诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    lint = sub.add_parser('lint'); lint.add_argument('source', type=Path)
    prepare = sub.add_parser('prepare'); prepare.add_argument('source', type=Path)
    prepare.add_argument('--output', required=True, type=Path)
    target = sub.add_parser('check-target'); target.add_argument('source', type=Path)
    target.add_argument('--runtime', required=True, type=Path)
    compose = sub.add_parser('compose')
    compose.add_argument('--packs', required=True, action='append', type=Path)
    compose.add_argument('--selection', required=True, type=Path)
    compose.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(argv)
    code = 0
    try:
        if args.command == 'compose':
            try:
                selection = read_pack(args.selection)
            except ValueError as exc:
                raise PackError('InvalidPackSelectionFile', str(exc), {'source': str(args.selection)}) from exc
            result = compose_pack_set(args.packs, selection, args.output)
        else:
            pack = read_pack(args.source)
            report = validate_pack(pack, str(args.source))
            if not report.ok:
                result = report.to_dict(); code = 1
            elif args.command == 'lint':
                result = report.to_dict()
            elif args.command == 'prepare':
                result = prepare_pack(args.source, args.output)
            else:
                result = check_target(pack, read_pack(args.runtime))
                code = 0 if result['declaration_match'] else 3
    except PackError as exc:
        result = {'status': 'INVALID_PACK_SET', **exc.to_dict()}; code = 2
    except ValueError as exc:
        result = {'status':'IO_ERROR','runtime_verified':False,'message':str(exc)}; code = 2
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return code
