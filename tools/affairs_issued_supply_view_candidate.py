"""新供给当前许可沿有限显示/Director尾参传入；只生成精确基准共享候选。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
BASE = '9effd89a84873a7cec0e30bb3a5761011c62151e'
ROOT = 'native/world/src/resident/'
PATHS = [ROOT + p for p in [
    'views/dynamic_world_view.hpp', 'views/dynamic_world_view.cpp',
    'views/membership_view.hpp', 'views/membership_view.cpp',
    'views/regional_life_view.hpp', 'views/regional_life_view.cpp',
    'domains/director/director_model.hpp', 'domains/director/director_rules.cpp']]
INCLUDE = '#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"\n'


def replace(source, before, after, count=1):
    """锚点失配即失败，不用旧共享整叶掩盖其它owner的后继。"""
    if source.count(before) != count:
        raise ValueError((before, source.count(before), count))
    return source.replace(before, after)


def transform(path, source):
    """原已知列表先过滤许可，错误原样上送，空许可不伪造新知识。"""
    if path.endswith('.hpp'):
        source = replace(source, 'namespace hanmo::resident {', 'namespace hanmo::resident {\nclass EconomicSupplyPolicyRead;')
        if path.endswith(('dynamic_world_view.hpp', 'membership_view.hpp')):
            return replace(source, 'can_accept_organization={});', 'can_accept_organization={},const EconomicSupplyPolicyRead* issued_supply=nullptr);')
        if path.endswith('regional_life_view.hpp'):
            return replace(source, 'const StableId& observer,const StableId& place);', 'const StableId& observer,const StableId& place,const EconomicSupplyPolicyRead* issued_supply=nullptr);')
        return replace(source, 'const DirectorDiplomaticContext* diplomacy);', 'const DirectorDiplomaticContext* diplomacy,const EconomicSupplyPolicyRead* issued_supply=nullptr);')
    source = INCLUDE + source
    if path.endswith('dynamic_world_view.cpp'):
        source = replace(source, 'can_accept_organization){', 'can_accept_organization,const EconomicSupplyPolicyRead* issued_supply){')
        source = replace(source, '    std::set<StableId> visible_needs;', '    auto opportunities=query_opportunities_with_issued_supply(c,*world.affairs,world.tasks,view,issued_supply);if(!opportunities.ok)return {false,{},opportunities.error};\n    std::set<StableId> visible_needs;')
        return replace(source, 'query_opportunities(*world.affairs,world.tasks,view)', 'opportunities.value', 2)
    if path.endswith('membership_view.cpp'):
        source = replace(source, 'can_accept_organization) {', 'can_accept_organization,const EconomicSupplyPolicyRead* issued_supply) {')
        return replace(source, 'observer,place,0,can_accept_organization);', 'observer,place,0,can_accept_organization,issued_supply);')
    if path.endswith('regional_life_view.cpp'):
        source = replace(source, 'const RegionalSharePreview& share,const StableId& observer,const StableId& place){', 'const RegionalSharePreview& share,const StableId& observer,const StableId& place,const EconomicSupplyPolicyRead* issued_supply){')
        source = replace(source, 'const auto opportunities=query_opportunities(*world.affairs,world.tasks,observed);', 'auto opportunities=query_opportunities_with_issued_supply(*common,*world.affairs,world.tasks,observed,issued_supply);if(!opportunities.ok)return {false,{},opportunities.error};')
        return replace(source, 'const auto& opportunity:opportunities)', 'const auto& opportunity:opportunities.value)')
    source = replace(source, 'const DirectorDiplomaticContext* diplomacy){', 'const DirectorDiplomaticContext* diplomacy,const EconomicSupplyPolicyRead* issued_supply){')
    return replace(source, 'auto opportunities=query_opportunities(affairs,tasks,view);Integer considered=0;for(const auto& offer:opportunities){', 'auto opportunities=query_opportunities_with_issued_supply(c,affairs,tasks,view,issued_supply);if(!opportunities.ok)return {false,{},opportunities.error};Integer considered=0;for(const auto& offer:opportunities.value){')


def main():
    """记录同源每叶OID；kernel借用必须由共享组合层绑定同一候选。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--base', default=BASE)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    oid = lambda data: hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    files = []
    for path in PATHS:
        before = subprocess.check_output(['git', '-C', str(args.repo), 'show', args.base+':'+path])
        after = transform(path, before.decode()).encode()
        target = args.output/'candidate'/path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(after)
        files.append({'path': path, 'base_oid': oid(before), 'result_oid': oid(after)})
    (args.output/'manifest.json').write_text(json.dumps({
        'base': args.base, 'requires': 'S3 6b6df864 issued-registration05', 'files': files,
        'kernel_followups': ['query_dynamic_place + query_named_membership_entries',
                             'resolve_regional_life_candidates current-candidate callback',
                             'query_runtime_task_offer_conversation final issued_supply argument',
                             'propose_director full overload final issued_supply argument'],
        'boundary': 'All new pointers borrow the same current candidate. Known list unchanged except new issued permission filtering; filter runs before Director candidate budget. No new persistent state or command tag.'}, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    main()
