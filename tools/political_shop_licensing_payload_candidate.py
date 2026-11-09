"""固定6951法律六类事实登记；不注册Action、不改模块或存档尾序。"""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import subprocess

BASE = '6951f250375e644664ec0d3630d8034ca78576df'
K = 'native/world/src/resident/kernel/'
TABLES = [('registries', 'political_shop_registry_initialized'),
          ('warrants', 'political_wanted_registered'),
          ('releases', 'political_wanted_released'),
          ('clearances', 'political_shop_clearance_recorded'),
          ('permits', 'political_shop_permit_issued'),
          ('revocations', 'political_shop_permit_revoked')]


def generate(repo, output, source=None, only='both'):
    if output.exists():
        raise ValueError('Fresh output required')
    originals, contents = {}, {}

    def read(path):
        if path not in contents:
            incoming = source / path if source else None
            originals[path] = incoming.read_text() if incoming and incoming.exists() else subprocess.check_output(
                ['git', 'show', BASE + ':' + path], cwd=repo, text=True)
            contents[path] = originals[path]
        return contents[path]

    def change(path, old, new):
        text = read(path)
        if text.count(old) != 1:
            raise ValueError('Unique anchor required: ' + path + ': ' + old[:90])
        contents[path] = text.replace(old, new)

    def include(path):
        line = '#include "resident/domains/politics/licensing/political_shop_licensing.hpp"\n'
        if line not in read(path):
            contents[path] = line + contents[path]

    if only in ('both', 'bundle'):
        path = K + 'world_bundle.cpp'
        include(path)
        anchor = '    auto fact=[&](const StableId& id,const StableId& kind,const StableId& domain)->Status{auto p=w.common.state().facts.find(id);if(!p||p->id!=id||p->kind!=kind||p->domain!=domain||!facts.insert(id).second)return common_failure(kind=="terrain_journey"?"TerrainJourneyFactMismatch":"ResidentFactRegistryMismatch",id);return common_success();};\n'
        extra = '''    // 法律仅有六种零时永久事实；每个实体必须由本域精确头验证，不凭domain豁免。
    auto legal_fact=[&](const StableId& id,const StableId& kind)->Status{
        const auto* header=w.common.state().facts.find(id);
        if(!w.political_shop_licensing_enabled()||!header||header->kind!=kind)return common_failure("PoliticalShopPayloadMismatch",id);
        auto checked=validate_political_shop_licensing_fact_header(w.common,w.political_shop_licensing,*header);if(!checked.ok)return checked;
        return fact(id,kind,political_shop_licensing_module);
    };
'''
        for table, kind in TABLES:
            extra += f'    for(const auto& row:w.political_shop_licensing.{table}.rows()){{auto checked=legal_fact(row.first,"{kind}");if(!checked.ok)return checked;}}\n'
        change(path, anchor, anchor + extra)

    if only in ('both', 'codec'):
        path = K + 'world_codec.cpp'
        include(path)
        condition = '||'.join(f'kind=="{kind}"' for _, kind in TABLES)
        count = '+'.join('state.' + table + '.size()' for table, _ in TABLES)
        cold = '''    // 独立法律段保存载荷；共同JSON只允许kind/domain，额外载荷、孤头或缺实体均拒。
    {
        CommonStore legal_common(result.common,catalog.common);std::set<StableId> legal_ids;
        for(const auto& row:object(field(source,"facts","state"),"facts")){
            const auto kind=get_string(row.second,"kind",row.first);const auto* named=row.second.find("domain");
            const bool legal_kind=CONDITION;
            if(!legal_kind&&(!named||string(*named,row.first)!=political_shop_licensing_module))continue;
            if(!catalog.political_shop_licensing||!result.political_shop_licensing)fail("PoliticalShopLicensingProfileRequired",row.first);
            fields(row.second,{"kind","domain"},{},"facts/"+row.first);
            if(object(row.second,row.first).size()!=2||!legal_ids.insert(row.first).second)fail("PoliticalShopPayloadMismatch",row.first);
            const auto* header=legal_common.state().facts.find(row.first);if(!header)fail("PoliticalShopFactMismatch",row.first);
            auto checked=validate_political_shop_licensing_fact_header(legal_common,*result.political_shop_licensing,*header);if(!checked.ok)return {false,{},checked.error};
        }
        if(result.political_shop_licensing){const auto& state=*result.political_shop_licensing;if(legal_ids.size()!=COUNT)fail("PoliticalShopPayloadMismatch","political_shop_licensing");}
    }
'''.replace('CONDITION', condition).replace('COUNT', count)
        anchor = '    // 本profile的wait没有业务params；其它未实现动作不能丢弃载荷。\n'
        change(path, anchor, cold + anchor)
        anchor = '    auto* global=output.find("facts");for(auto& row:dict(*global)){auto* payload=facts.find(row.first);'
        export = '''    std::size_t legal_exported=0;
    auto* global=output.find("facts");for(auto& row:dict(*global)){auto* payload=facts.find(row.first);
        const auto* legal_header=w.common.state().facts.find(row.first);
        if(legal_header){const auto& kind=legal_header->kind;if(legal_header->domain==political_shop_licensing_module||CONDITION){
            if(!w.political_shop_licensing_enabled())fail("PoliticalShopLicensingProfileRequired",row.first);
            auto checked=validate_political_shop_licensing_fact_header(w.common,w.political_shop_licensing,*legal_header);if(!checked.ok)return {false,{},checked.error};
            if(payload)fail("PoliticalShopPayloadMismatch",row.first);
            set(row.second,"domain",BoundaryValue{legal_header->domain});++legal_exported;continue;
        }}
'''.replace('CONDITION', condition)
        change(path, anchor, export)
        anchor = 'row.second=*payload;}\n'
        check = '    {const auto& state=w.political_shop_licensing;if(legal_exported!=' + count + ')fail("PoliticalShopPayloadMismatch","political_shop_licensing");}\n'
        change(path, anchor, anchor + check)

    rows, patches = [], []
    def oid(value):
        raw = value.encode()
        return hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    for path, value in sorted(contents.items()):
        for part, text in [('candidate', value), ('base', originals[path])]:
            target = output / part / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text)
        patch = ''.join(difflib.unified_diff(originals[path].splitlines(True), value.splitlines(True), fromfile='a/' + path, tofile='b/' + path))
        (output / (Path(path).stem + '.patch')).write_text(patch)
        patches.append(patch)
        rows.append({'path': path, 'before_oid': oid(originals[path]), 'after_oid': oid(value), 'sha256': hashlib.sha256(value.encode()).hexdigest()})
    (output / 'candidate.patch').write_text(''.join(patches))
    (output / 'manifest.json').write_text(json.dumps({'base': BASE, 'status': 'SOURCE_ONLY', 'shared': rows,
        'table_kinds': TABLES, 'scope': 'Only six legal fact headers and JSON payload boundary. No Action, module order, codec tail or cold authority order changes.',
        'dependencies': 'S5 complete law/private-shop pair holder and decode-state registration; Common exact legal-domain decode allowlist required separately.',
        'syntax': 'not_run_shared_pair_pending', 'runtime': 'not_run'}, indent=2) + '\n')
    return rows


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--base', default=BASE)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--only', choices=['both', 'bundle', 'codec'], default='both')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.base != BASE:
        raise SystemExit('Fixed 6951f25 base required')
    print(len(generate(args.repo, args.output, args.source, args.only)))
