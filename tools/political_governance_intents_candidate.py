#!/usr/bin/env python3
"""固定956候选：原NPC安民签发/受领、完整三域保存与显式模板16，不新增业务段或命令。"""
import argparse
import difflib
import hashlib
import json
import subprocess
from pathlib import Path
from content.political_governance_intents_links import with_political_governance_intents_schema

BASE = '956d60af9390a5c9e14985f7b646a57c2b56cb71'
p = argparse.ArgumentParser(); p.add_argument('--repo', type=Path, required=True); p.add_argument('--output', type=Path, required=True); p.add_argument('--base', default=BASE); a = p.parse_args()
if a.base != BASE or a.output.exists():
    raise SystemExit('Requires fixed956 and fresh output')
F = {}; OLD = {}; D = 'native/world/src/resident/domains/'; K = 'native/world/src/resident/kernel/'


def get(path):
    """只取固定Git对象，候选永不借移动主树。"""
    if path not in F:
        F[path] = OLD[path] = subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=a.repo, text=True)
    return F[path]


def change(path, old, new, count=1):
    """逐唯一锚语义变换，已有后继必须显式三方而非静默覆叶。"""
    s = get(path)
    if s.count(old) != count:
        raise SystemExit(f'Anchor mismatch {path}: {old[:120]} ({s.count(old)} vs {count})')
    F[path] = s.replace(old, new)


def after(path, needle, extra):
    """完整唯一行之后插入，不假定巨长构造行的相邻域布局。"""
    lines = [line for line in get(path).splitlines(True) if needle in line]
    if len(lines) != 1:
        raise SystemExit(f'Line mismatch {path}: {needle}: {len(lines)}')
    change(path, lines[0], lines[0] + extra)

p = D + 'director/director_model.hpp'
change(p, 'personal_appointment_attendance=15', 'personal_appointment_attendance=15, governance_commission=16')
change(p, '    std::optional<StableId> shop_registry_intent_id;', '    std::optional<StableId> shop_registry_intent_id;\n    std::optional<StableId> governance_intent_id;')
change(p, 'struct PoliticalShopRegistryIntentCatalog;', 'struct PoliticalShopRegistryIntentCatalog;\nstruct PoliticalGovernanceIntentCatalog;\n// 只有原政治治理/Task/公共赈济组合可选安民意图。\nstruct DirectorGovernanceIntentProfile {bool enabled=false;};')
change(p, '    std::shared_ptr<const PoliticalShopRegistryIntentCatalog> shop_registry_intents;', '    std::shared_ptr<const PoliticalGovernanceIntentCatalog> governance_intents;\n    std::shared_ptr<const PoliticalShopRegistryIntentCatalog> shop_registry_intents;')
after(p, 'DirectorShopRegistryProfile,bool personal_appointment_attendance_enabled=false);', 'Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue&,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile,DirectorSupplyRenewalProfile,std::shared_ptr<const VisitReceptionCatalog>,DirectorShopRegistryProfile,bool personal_appointment_attendance_enabled,DirectorGovernanceIntentProfile);\n')
p = D + 'director/director_codec.cpp'
change(p, '#include "director_model.hpp"', '#include "director_model.hpp"\n#include "resident/domains/politics/governance_intents/political_governance_intents.hpp"')
change(p, 'if(name=="personal_appointment_attendance")', 'if(name=="governance_commission")return DirectorTemplate::governance_commission;if(name=="personal_appointment_attendance")')
old = 'Outcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage,DirectorMilitaryAppointmentProfile military,DirectorSupplyRenewalProfile supply,std::shared_ptr<const VisitReceptionCatalog> reception,DirectorShopRegistryProfile registries,bool attendance_enabled)'
change(p, old + '{try{using namespace boundary;', old + '{return decode_director_catalog(value,regional_life,named_membership,politics,diplomacy,diplomacy_version,treaty_passage,military,supply,std::move(reception),registries,attendance_enabled,DirectorGovernanceIntentProfile{});}\n// 原完整调用保关闭，递归与两生产入口必须显式传原三域资格。\n' + old[:-1] + ',DirectorGovernanceIntentProfile governance){try{using namespace boundary;if(value.find("political_governance_intents")){auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};}')
change(p, '"political_shop_registry_intents","personal_appointment_attendance"}', '"political_shop_registry_intents","personal_appointment_attendance","political_governance_intents"}', 2)
after(p, 'if(auto* attendance=value.find("personal_appointment_attendance"))legacy', '    if(auto* governance=value.find("political_governance_intents"))legacy.emplace_back("political_governance_intents",*governance);\n')
change(p, 'military,supply,reception,registries,attendance_enabled);if(!parsed.ok)', 'military,supply,reception,registries,attendance_enabled,governance);if(!parsed.ok)')
change(p, 'DirectorCatalog c;if(auto* registry=', 'DirectorCatalog c;if(auto* selected=value.find("political_governance_intents")){if(!governance.enabled||!politics)fail("GovernanceIntentProfileRequired","director");auto parsed=decode_political_governance_intents(*selected);if(!parsed.ok)return {false,{},parsed.error};c.governance_intents=std::make_shared<const PoliticalGovernanceIntentCatalog>(std::move(parsed.value));}if(auto* registry=')
change(p, '"shop_registry_intent_id"},row.first)', '"shop_registry_intent_id","governance_intent_id"},row.first)', 3)
change(p, 'auto t=read_template(name);', 'auto t=read_template(name);if(t==DirectorTemplate::governance_commission&&!c.governance_intents)fail("GovernanceIntentProfileRequired",row.first);')
change(p, 'if(auto* registry=v.find("shop_registry_intent_id"))', 'if(auto* intent=v.find("governance_intent_id")){p.governance_intent_id=string(*intent,row.first);if(!c.governance_intents||!c.governance_intents->intents.find(*p.governance_intent_id))fail("GovernanceIntentSourceMismatch",row.first);}if(auto* registry=v.find("shop_registry_intent_id"))')
change(p, 'const bool registry_template=', 'const bool governance_template=std::find(p.templates.begin(),p.templates.end(),DirectorTemplate::governance_commission)!=p.templates.end();if(governance_template!=p.governance_intent_id.has_value())fail("GovernanceIntentDirectorPolicyMismatch",row.first);const bool registry_template=')
p = D + 'director/director_validation.cpp'
change(p, '(t==DirectorTemplate::personal_appointment_attendance?', '(t==DirectorTemplate::governance_commission?!catalog.governance_intents:t==DirectorTemplate::personal_appointment_attendance?')
change(p, '&&!(catalog.personal_appointment_attendance&&p.template_id=="personal_appointment_attendance")', '&&!(catalog.personal_appointment_attendance&&p.template_id=="personal_appointment_attendance")&&!(catalog.governance_intents&&p.template_id=="governance_commission")')
p = D + 'director/director_registry.cpp'
change(p, 'return kind==DirectorTemplate::treaty_ratification||', 'return kind==DirectorTemplate::governance_commission||kind==DirectorTemplate::treaty_ratification||')
p = 'tools/content/director_registry_links.py'
change(p, "{'private_livelihood', 'replenish_supply',", "{'governance_commission', 'private_livelihood', 'replenish_supply',")
p = D + 'politics/politics_director.hpp'
after(p, ')> shop_registry_acceptance;', '    std::function<Outcome<std::optional<DecisionProposal>>(const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&)> governance_commission;\n')
p = D + 'director/director_rules.cpp'
after(p, 'if(enabled(*policy,DirectorTemplate::shop_registry_acceptance)', '    if(enabled(*policy,DirectorTemplate::governance_commission)&&!(s.registrations.find(view.actor_id)&&s.registrations.find(view.actor_id)->obligations_only)){if(!catalog.governance_intents||!politics||!politics->governance_commission)return {false,{},common_failure("GovernanceIntentDirectorAccessMissing",view.actor_id,ErrorKind::internal).error};auto proposed=politics->governance_commission(view,*policy,p);if(!proposed.ok)return {false,{},proposed.error};if(proposed.value)return {true,{std::move(*proposed.value)},{}};}\n')
p = K + 'dynamic_integration.cpp'
change(p, '#include "dynamic_integration.hpp"', '#include "dynamic_integration.hpp"\n#include "political_governance_director_world.hpp"')
after(p, 'politics->shop_registry_acceptance=', '        if(w.catalog->director->governance_intents&&politics)politics->governance_commission=[&w](const KnownSubjectView& known,const DirectorPolicy& policy,const DecisionProposal& base){return propose_political_governance_director_world(w,known,policy,base);};\n')
# 固定956已完整保存/恢复三域；精确源门防止后继误删，不重复添加字段。
for name in ('politics_governance', 'governance_tasks', 'public_relief'):
    for fragment in (name+'(w.'+name+')', 'w.'+name+'=std::move('+name+')'):
        if get(p).count(fragment) != 1:
            raise SystemExit('Governance original write-set mismatch: '+fragment)
p = K + 'world_bundle.cpp'
change(p, '#include "world_bundle.hpp"', '#include "world_bundle.hpp"\n#include "political_governance_director_world.hpp"')
after(p, 'valid=validate_political_governance_continuation_world(*this);', '    valid=validate_political_governance_director_world(*this);if(!valid.ok)return valid;\n')
p = K + 'native_initial_state.cpp'
change(p, ',attendance.value);if(!response_director.ok)', ',attendance.value,DirectorGovernanceIntentProfile{read_politics_governance_profile(dynamic)});if(!response_director.ok)')
after(p, 'admitted.module_versions.emplace_back("political_governance_continuations","1");', '    if(response_director.value.governance_intents)admitted.module_versions.emplace_back("political_governance_intents","1");\n')
p = K + 'world_codec.cpp'
change(p, '#include "world_codec.hpp"', '#include "world_codec.hpp"\n#include "resident/domains/politics/governance_intents/political_governance_intent_validation.hpp"')
change(p, '{"political_governance_continuations",{"1"}}', '{"political_governance_continuations",{"1"}}, {"political_governance_intents",{"1"}}')
after(p, 'validate_political_governance_continuation_modules(identity.catalog.module_versions,', '        auto governance_intents_layout=validate_political_governance_intent_modules(has("political_governance_intents"),identity.catalog.module_versions);if(!governance_intents_layout.ok)return {false,{},governance_intents_layout.error};\n')
after(p, 'auto registry_intents_layout=validate_political_shop_registry_intent_modules(has("political_shop_registry_intents","1"),', '            auto governance_intents_layout=validate_political_governance_intent_modules(has("political_governance_intents","1"),versions);if(!governance_intents_layout.ok)return {false,{},governance_intents_layout.error};\n')
change(p, ',attendance.value);if(!director.ok)', ',attendance.value,DirectorGovernanceIntentProfile{result->politics_governance&&result->politics_governance->enabled&&result->public_relief!=nullptr});if(!director.ok)')
after(p, 'result->director=std::make_shared<const DirectorCatalog>(std::move(director.value));', '        if(result->director->governance_intents){const auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");if(!result->politics||!result->organization||!result->politics_governance||std::find(caps.begin(),caps.end(),"political_governance_intents_v1")==caps.end())fail("GovernanceIntentProfileRequired","director");auto checked=validate_political_governance_intents(*result->director->governance_intents,*result->common,*result->politics,*result->politics_governance,*result->organization);if(!checked.ok)return {false,{},checked.error};}\n')
after(p, 'id.module_versions.emplace_back("political_governance_continuations","1");', '    if(result->director&&result->director->governance_intents)id.module_versions.emplace_back("political_governance_intents","1");\n')
p = 'game/ui/native_resident_backend.gd'
after(p, 'expected["module_versions"].append({"name":"political_governance_continuations"', '\tif definition.get("dynamic_world",{}).get("director",{}).has("political_governance_intents"): expected["module_versions"].append({"name":"political_governance_intents","version":"1"})\n')
p = 'schemas/content.schema.json'; F[p] = json.dumps(with_political_governance_intents_schema(json.loads(get(p))), ensure_ascii=False, indent=2) + '\n'
p = 'tools/content/links.py'
after(p, 'from .political_governance_continuation_links import ', '    from .political_governance_intents_links import check_political_governance_intents\n')
change(p, ' + check_political_governance_continuations(pack,index)', ' + check_political_governance_continuations(pack,index) + check_political_governance_intents(pack,index)')
p = 'game/data/rule_capabilities.json'; data = json.loads(get(p)); data['capabilities'].append({'capability_id':'political_governance_intents_v1','rule_version':'political_governance_intents_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.director.political_governance_intents.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_governance_continuation_panel'],'evidence':[]}); F[p] = json.dumps(data, ensure_ascii=False, indent=2) + '\n'
a.output.mkdir(parents=True); files=[]; patch=[]


def oid(text):
    """记录候选原字节Git身份；Git物化由中央唯一共享写者完成。"""
    b = text.encode(); return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()

for path, text in sorted(F.items()):
    if text == OLD[path]: continue
    target = a.output/'candidate'/path; target.parent.mkdir(parents=True, exist_ok=True); target.write_text(text)
    files.append({'path':path,'before_oid':oid(OLD[path]),'after_oid':oid(text),'sha256':hashlib.sha256(text.encode()).hexdigest()})
    patch.extend(difflib.unified_diff(OLD[path].splitlines(True),text.splitlines(True),'a/'+path,'b/'+path))
(a.output/'candidate.patch').write_text(''.join(patch)); (a.output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_ONLY_SYNTAX_PENDING','shared':files,'central_registration_only':'native/world/resident.cmake','marker':'political_governance_intents/1; Director template16; no new section or feature','new_cpp':['political_governance_intents.cpp','political_governance_director.cpp','political_governance_intent_validation.cpp','political_governance_director_world.cpp'],'native_tests':['political_governance_intents_test.cpp'],'author_tests':['tools/test_political_governance_intents_authoring.py'],'runtime':False},indent=2)+'\n'); print(len(files))
