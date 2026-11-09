"""用同一真实SO/源码检查回执来源：旧只读摘要必须拒绝，纠正的resident摘要可通过。"""
import argparse
import copy
import json
from pathlib import Path
import tempfile
import sys


def main() -> int:
    """只调用原prepare身份口，不创建stage/世界或改写输入回执。"""
    parser=argparse.ArgumentParser()
    for name in ('root','source','library','original','corrected','profile'):
        parser.add_argument('--'+name,required=True)
    args=parser.parse_args();root=Path(args.root).resolve();sys.path.insert(0,str(root))
    from tools import native_resident_profile as profile
    config=profile._read_config(Path(args.profile));corrected=json.loads(Path(args.corrected).read_text());checks=0
    profile._native_identity(root,args.source,Path(args.library),Path(args.corrected),config);checks+=1
    with tempfile.TemporaryDirectory(prefix='hanmo-receipt-identity-') as tmp:
        cases=[('original-readonly-header',json.loads(Path(args.original).read_text()))]
        for field in ('source_sha256','resident_core_source_sha256','source_commit'):
            bad=copy.deepcopy(corrected);bad['bridge_identity'][field]='0'*len(bad['bridge_identity'][field]);cases.append((field,bad))
        for label,value in cases:
            path=Path(tmp)/(label+'.json');path.write_text(json.dumps(value))
            try:profile._native_identity(root,args.source,Path(args.library),path,config)
            except RuntimeError as error:
                if 'NativeResidentIdentityMismatch' not in str(error):raise
            else:raise AssertionError('prepare accepted wrong resident identity: '+label)
            checks+=1
    print(json.dumps({'checks':checks,'failures':0,'source':args.source,'scope':'actual prepare identity entry, unchanged source/library, no stage or world'}));return 0


if __name__=='__main__':
    raise SystemExit(main())
