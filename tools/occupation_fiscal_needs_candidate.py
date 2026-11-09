"""基于固定中央288生成显式新Need准入候选；不写中央，不改旧profile载荷与事实分流。"""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--base', default='288f8e5')
args = parser.parse_args()
base = subprocess.check_output(['git', 'rev-parse', args.base], cwd=args.repo, text=True).strip()
if args.output.exists():
    raise ValueError('fresh output required')
files = {}; original = {}

def get(path):
    """只从指定Git对象读取共享原件，不混用在写中的中央工作树。"""
    if path not in files:
        original[path] = subprocess.check_output(['git', 'show', base + ':' + path], cwd=args.repo).decode()
        files[path] = original[path]
    return files[path]

def change(path, old, new, count=1):
    """原锚必须精确命中；变化不明时停下交中央语义合并。"""
    value = get(path)
    assert value.count(old) == count, (path, old[:100], value.count(old), count)
    files[path] = value.replace(old, new)

K='native/world/src/resident/kernel/'
profile='resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp'
p='native/world/include/hanmo/resident/model_contract.hpp'
change(p,'    bool occupation_fiscal_settlement_enabled=false;','    bool occupation_fiscal_settlement_enabled=false;\n    bool occupation_fiscal_needs_enabled=false;')
p=K+'world_bundle.hpp'
anchor='    bool occupation_fiscal_enabled()const{return political_takeover_enabled()&&catalog->occupation_fiscal&&catalog->occupation_fiscal->enabled;}'
change(p,anchor,anchor+'\n    bool occupation_fiscal_needs_enabled()const{return occupation_fiscal_enabled()&&catalog->occupation_fiscal_needs_enabled;}')
p=K+'world_bundle.cpp'
change(p,'#include "occupation_fiscal_integration.hpp"','#include "occupation_fiscal_integration.hpp"\n#include "'+profile+'"')
anchor='valid=validate_occupation_fiscal_world(*this);if(!valid.ok)return valid;'
change(p,anchor,anchor+'\n        valid=validate_occupation_fiscal_need_layout(catalog->occupation_fiscal_needs_enabled,catalog->identity.module_versions);if(!valid.ok)return valid;')
anchor='fiscal_need_read.emplace(local_economy,fiscal_settlement,*catalog->fiscal_settlement,*catalog->affairs->fiscal_need_policies);'
change(p,anchor,'if(occupation_fiscal_needs_enabled())fiscal_need_read.emplace(local_economy,fiscal_settlement,*catalog->fiscal_settlement,*catalog->affairs->fiscal_need_policies,occupation_fiscal_settlement);else '+anchor)
p=K+'phase_plan.cpp'
change(p,'        if(w.occupation_fiscal_enabled()){\n            if(!w.catalog->occupation_fiscal_settlement_enabled)', '        if(w.occupation_fiscal_enabled()&&!w.occupation_fiscal_needs_enabled()){\n            if(!w.catalog->occupation_fiscal_settlement_enabled)')
anchor='r=reconcile_fiscal_needs_in_candidate(w.common,w.affairs,w.local_economy,w.fiscal_settlement,*w.catalog->fiscal_settlement,*w.catalog->affairs->fiscal_need_policies,legacy_sources);'
change(p,anchor,'if(w.occupation_fiscal_needs_enabled())r=reconcile_fiscal_needs_in_candidate(w.common,w.affairs,w.local_economy,w.fiscal_settlement,*w.catalog->fiscal_settlement,*w.catalog->affairs->fiscal_need_policies,legacy_sources,w.occupation_fiscal_settlement);else '+anchor)
p=K+'occupation_fiscal_integration.cpp'
change(p,'#include "resident/domains/local_economy/fiscal/occupation_fiscal_view.hpp"','#include "resident/domains/local_economy/fiscal/occupation_fiscal_view.hpp"\n#include "resident/domains/local_economy/fiscal/fiscal_need_view.hpp"')
change(p,'if(!index.ok)return {false,{},index.error};auto page=query_occupation_fiscal_settlement','if(!index.ok)return {false,{},index.error};if(w.occupation_fiscal_needs_enabled())bind_fiscal_need_references(index.value,w.affairs);auto page=query_occupation_fiscal_settlement')
p=K+'world_codec.cpp'
change(p,'#include "world_codec.hpp"','#include "world_codec.hpp"\n#include "'+profile+'"')
change(p,'{"fiscal_need_policies",{"1"}},','{"fiscal_need_policies",{"1"}}, {"occupation_fiscal_needs",{"1"}},')
anchor='        if(has("political_occupation_fiscal"))domains=occupation_fiscal_domains(std::move(domains));'
change(p,anchor,anchor+'\n        auto fiscal_needs_layout=validate_occupation_fiscal_need_layout(has("occupation_fiscal_needs"),identity.catalog.module_versions);if(!fiscal_needs_layout.ok)return {false,{},fiscal_needs_layout.error};')
anchor='    const auto& identity=decoded.value.identity.catalog;const auto& versions=identity.module_versions;'
change(p,anchor,anchor+'auto fiscal_needs_layout=validate_occupation_fiscal_need_layout(std::any_of(versions.begin(),versions.end(),[](const auto& v){return v.first==occupation_fiscal_need_module;}),versions);if(!fiscal_needs_layout.ok)return {false,{},fiscal_needs_layout.error};')
anchor='result->affairs=std::make_shared<const AffairsCatalog>(std::move(affairs.value));'
change(p,anchor,anchor+'''\n        if(const auto* needs=dynamic->find("occupation_fiscal_needs")){auto parsed=decode_occupation_fiscal_need_profile(*needs);if(!parsed.ok)return {false,{},parsed.error};if(!occupation_fiscal_enabled(*result)||!fiscal_needs_enabled(*result))fail("OccupationFiscalNeedDependenciesRequired","dynamic_world/occupation_fiscal_needs");const auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"occupation_fiscal_needs_v1")==caps.end())fail("OccupationFiscalNeedCapabilityRequired","dynamic_world/occupation_fiscal_needs");result->occupation_fiscal_needs_enabled=true;}''')
anchor='    if(fiscal_needs_enabled(*result))id.module_versions.emplace_back("fiscal_need_policies","1");'
change(p,anchor,anchor+'\n    if(result->occupation_fiscal_needs_enabled)id.module_versions.emplace_back(occupation_fiscal_need_module,"1");')
p=K+'native_initial_state.cpp'
change(p,'#include "native_initial_state.hpp"','#include "native_initial_state.hpp"\n#include "'+profile+'"')
anchor='    if(fiscal_needs)admitted.module_versions.emplace_back("fiscal_need_policies","1");'
change(p,anchor,anchor+'''\n    if(const auto* needs=dynamic.find("occupation_fiscal_needs")){auto parsed=decode_occupation_fiscal_need_profile(*needs);if(!parsed.ok)return {false,{},parsed.error};if(!fiscal_needs||!dynamic.find("political_occupation_fiscal"))fail("OccupationFiscalNeedDependenciesRequired","dynamic_world/occupation_fiscal_needs");admitted.module_versions.emplace_back(occupation_fiscal_need_module,"1");}''')
p='game/ui/native_resident_backend.gd'
anchor='\t\texpected["module_versions"].append({"name":"fiscal_need_policies","version":"1"})'
change(p,anchor,anchor+'\n\tif definition.get("dynamic_world",{}).has("occupation_fiscal_needs"): expected["module_versions"].append({"name":"occupation_fiscal_needs","version":"1"})')
p='tools/content/links.py'
change(p,'    from .political_occupation_fiscal_links import check_political_occupation_fiscal','    from .political_occupation_fiscal_links import check_political_occupation_fiscal\n    from .occupation_fiscal_need_links import check_occupation_fiscal_needs')
change(p,' + check_political_occupation_fiscal(pack,index)',' + check_political_occupation_fiscal(pack,index) + check_occupation_fiscal_needs(pack,index)')
p='schemas/content.schema.json';value=json.loads(get(p));value['properties']['required_capabilities']['items']['enum'].append('occupation_fiscal_needs_v1');value['$defs']['dynamic_world']['properties']['occupation_fiscal_needs']={'type':'object','properties':{'enabled':{'type':'boolean','enum':[True]},'version':{'type':'integer','enum':[1]}},'required':['enabled','version'],'additionalProperties':False};files[p]=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
p='game/data/rule_capabilities.json';value=json.loads(get(p));value['capabilities'].append({'capability_id':'occupation_fiscal_needs_v1','rule_version':'occupation_fiscal_needs_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.occupation_fiscal_needs.enabled','equals':True}],'supported_semantics':[],'test_ids':['test_occupation_fiscal_panel'],'evidence':[]});files[p]=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
p='native/world/resident.cmake'
anchor='    src/resident/domains/local_economy/fiscal/fiscal_need_adapter.cpp'
change(p,anchor,anchor+'\n    src/resident/domains/local_economy/fiscal/occupation_fiscal_debt_source.cpp',2)
anchor='    src/resident/domains/local_economy/fiscal/fiscal_need_view.cpp'
# 这里只向生产身份列表追加实际新头；不另建可花数据段或新金额算法。
value=get(p);at=value.rfind(anchor);assert at>=0;files[p]=value[:at]+value[at:].replace(anchor,anchor+'\n    src/resident/domains/local_economy/fiscal/fiscal_need_adapter.hpp\n    src/resident/domains/local_economy/fiscal/occupation_fiscal_debt_source.hpp\n    src/resident/domains/local_economy/fiscal/occupation_fiscal_need_profile.hpp',1)
files[p] += '\nadd_executable(test_resident_occupation_fiscal_need_profile tests/resident/economic_world/occupation_fiscal_need_profile_test.cpp)\ntarget_link_libraries(test_resident_occupation_fiscal_need_profile PRIVATE hanmo_resident)\ntarget_compile_options(test_resident_occupation_fiscal_need_profile PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\n'
p='tools/verify.py';change(p,'TESTS = [',"TESTS = ['test_occupation_fiscal_panel', ")
args.output.mkdir(parents=True);entries=[];patch=[]
for path,value in files.items():
    destination=args.output/'shared'/path;destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text(value)
    raw=value.encode();old=original[path].encode();oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    entries.append({'path':path,'base_oid':oid(old),'oid':oid(raw),'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)})
    patch.extend(difflib.unified_diff(original[path].splitlines(True),value.splitlines(True),'a/'+path,'b/'+path))
(args.output/'registration.patch').write_text(''.join(patch))
(args.output/'manifest.json').write_text(json.dumps({'base':base,'shared_only':True,'files':entries,'persistence':'explicit module marker selects original Affairs payload semantics; no new monetary ledger, no restore-time backfill','old_profile':'pair1 + fiscal_need_policies1 without marker retains exact 288 strict routing and source reader','runtime':'DEFERRED'},indent=2)+'\n')
print(args.output/'manifest.json')
