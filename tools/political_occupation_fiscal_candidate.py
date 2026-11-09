"""从明确中央基准生成新税政配对候选；共享叶仅供中央串合，后接S3固定Task2变换。"""
from pathlib import Path
import subprocess,json,hashlib,difflib
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--base',default='35545ece9104ecef474f500d083f5e3cc8ba0fc9');args=parser.parse_args()
REPO=args.repo;OUT=args.output;BASE=args.base;CAND=OUT/'candidate'
if OUT.exists():raise ValueError('output must be a fresh candidate directory')
OUT.mkdir(parents=True)
files={}; originals={}
def get(p):
 if p not in files:
  files[p]=subprocess.check_output(['git','show',BASE+':'+p],cwd=REPO).decode();originals[p]=files[p]
 return files[p]
def change(p,a,b,n=1):
 s=get(p); assert s.count(a)==n,(p,a[:120],s.count(a),n);files[p]=s.replace(a,b)
def add(p,s):assert p not in files;files[p]=s;originals[p]=''
K='native/world/src/resident/kernel/'; D='native/world/src/resident/domains/'
p='native/world/include/hanmo/resident/model_contract.hpp'
change(p,'struct FamilyDeathPolicy; struct FamilyDeathState;','struct FamilyDeathPolicy; struct FamilyDeathState;\nstruct OccupationFiscalCatalog;struct OccupationFiscalState;struct OccupationFiscalSettlementState;')
change(p,'    bool task_takeover_execution_enabled=false;','    bool task_takeover_execution_enabled=false;\n    std::shared_ptr<const OccupationFiscalCatalog> occupation_fiscal;\n    bool occupation_fiscal_settlement_enabled=false;')
p='native/world/src/resident/model/common_model.hpp'
change(p,'    std::shared_ptr<const PoliticalTakeoverState> political_takeover;','    std::shared_ptr<const OccupationFiscalState> occupation_fiscal;\n    std::shared_ptr<const OccupationFiscalSettlementState> occupation_fiscal_settlement;\n    std::shared_ptr<const PoliticalTakeoverState> political_takeover;')
p=K+'world_bundle.hpp'
change(p,'#include "command_types.hpp"','#include "command_types.hpp"\n#include "resident/domains/local_economy/fiscal/occupation_fiscal_settlement.hpp"')
change(p,'    PoliticsState politics;PoliticalFiscalState political_fiscal;','    OccupationFiscalState occupation_fiscal;bool occupation_fiscal_seed_complete=false;\n    OccupationFiscalSettlementState occupation_fiscal_settlement;bool occupation_fiscal_settlement_seed_complete=false;\n    bool occupation_fiscal_enabled()const{return political_takeover_enabled()&&catalog->occupation_fiscal&&catalog->occupation_fiscal->enabled;}\n    PoliticsState politics;PoliticalFiscalState political_fiscal;')
p=K+'command_types.hpp'
change(p,'#include "political_takeover_integration.hpp"','#include "political_takeover_integration.hpp"\n#include "resident/domains/politics/takeover/politics_occupation_fiscal_command.hpp"')
change(p,'NavalBattleCommand,PoliticalTakeoverDecisionCommand>;','NavalBattleCommand,PoliticalTakeoverDecisionCommand,PoliticalSetOccupationTaxCommand>;')
change(p,'NavalBattleCommandQuote,PoliticalTakeoverQuote>;','NavalBattleCommandQuote,PoliticalTakeoverQuote,OccupationFiscalQuote>;')
add(K+'occupation_fiscal_integration.hpp','''// 本调用栈持有新税政的所有只读权源；每次候选替换后重新构造。
#pragma once
#include "political_takeover_integration.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_command.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_profile.hpp"
namespace hanmo::resident {
class OccupationFiscalWorldAccess final {
public:
    static Outcome<std::unique_ptr<OccupationFiscalWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
    const OccupationFiscalContext& context()const{return context_;}
    const OccupationFiscalAccess& policy()const{return policy_;}
    OccupationFiscalWorldAccess(const OccupationFiscalWorldAccess&)=delete;
    OccupationFiscalWorldAccess& operator=(const OccupationFiscalWorldAccess&)=delete;
private:
    OccupationFiscalWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverWorldAccess>);
    std::unique_ptr<PoliticalTakeoverWorldAccess> takeover_;
    OccupationFiscalContext context_;
    OccupationFiscalAccess policy_;
};
Status validate_occupation_fiscal_world(const WorldBundle&);
Status require_legacy_fiscal_world(const WorldBundle&,const StableId& jurisdiction);
Outcome<OccupationFiscalQuote> prepare_occupation_fiscal_world(const WorldBundle&,const StableId& actor,const PoliticalSetOccupationTaxCommand&);
Outcome<PoliticalFiscalEffects> commit_occupation_fiscal_world(WorldBundle&,const StableId& actor,const PoliticalSetOccupationTaxCommand&,const OccupationFiscalQuote&);
Outcome<BoundaryValue> query_occupation_fiscal_world(const WorldBundle&,const StableId& actor,const StableId& site);
}
''')
add(K+'occupation_fiscal_integration.cpp','''// 新政策不经旧财政1产生税债；实际金额另由同候选新财政段结算。
#include "occupation_fiscal_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
OccupationFiscalWorldAccess::OccupationFiscalWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverWorldAccess> takeover):takeover_(std::move(takeover)),context_{takeover_->context(),w.politics,*w.catalog->occupation_fiscal,w.occupation_fiscal,w.political_fiscal},policy_(context_){}
Outcome<std::unique_ptr<OccupationFiscalWorldAccess>> OccupationFiscalWorldAccess::create(const WorldBundle& w,const DynamicStepInput* phase){
    if(!w.occupation_fiscal_enabled()||!w.fiscal_settlement_enabled()||!w.catalog->occupation_fiscal_settlement_enabled||!w.occupation_fiscal_seed_complete||!w.occupation_fiscal_settlement_seed_complete)return {false,{},common_failure("OccupationFiscalProfileRequired").error};
    auto takeover=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover,phase);if(!takeover.ok)return {false,{},takeover.error};
    return {true,std::unique_ptr<OccupationFiscalWorldAccess>(new OccupationFiscalWorldAccess(w,std::move(takeover.value))),{}};
}
Status validate_occupation_fiscal_world(const WorldBundle& w){
    const bool enabled=w.occupation_fiscal_enabled();
    auto layout=validate_occupation_fiscal_layout(enabled,w.catalog->identity.module_versions,w.occupation_fiscal_seed_complete?&w.occupation_fiscal:nullptr,w.occupation_fiscal_settlement_seed_complete?std::optional<Integer>{w.occupation_fiscal_settlement.version}:std::nullopt);if(!layout.ok)return layout;
    if(w.catalog->occupation_fiscal_settlement_enabled!=enabled||(w.catalog->occupation_fiscal&&w.catalog->occupation_fiscal->enabled&&!enabled))return common_failure("OccupationFiscalProfileMismatch");
    auto history=validate_occupation_fiscal_command_history(w.common,enabled);if(!history.ok)return history;
    if(!enabled){if(!w.occupation_fiscal.policies.empty()||!w.occupation_fiscal_settlement.periods.empty()||!w.occupation_fiscal_settlement.assessments.empty()||!w.occupation_fiscal_settlement.payments.empty()||!w.occupation_fiscal_settlement.buckets.empty()||!w.occupation_fiscal_settlement.legacy_carry_claims.empty()||!w.occupation_fiscal_settlement.suspended_periods.empty())return common_failure("UnexpectedOccupationFiscalState");return common_success();}
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    return validate_occupation_fiscal_receipts(access.value->context(),[](const CommitResult& result){return result.kind==political_occupation_fiscal_command_kind&&result.details?std::get_if<PoliticalFiscalEffects>(&result.details->data):nullptr;});
}
Status require_legacy_fiscal_world(const WorldBundle& w,const StableId& jurisdiction){
    if(!w.occupation_fiscal_enabled())return common_success();
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    return require_legacy_fiscal_before_occupation(access.value->context().takeover,jurisdiction);
}
Outcome<OccupationFiscalQuote> prepare_occupation_fiscal_world(const WorldBundle& w,const StableId& actor,const PoliticalSetOccupationTaxCommand& command){
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return prepare_occupation_fiscal_command(access.value->context(),w.services,actor,command);
}
Outcome<PoliticalFiscalEffects> commit_occupation_fiscal_world(WorldBundle& w,const StableId& actor,const PoliticalSetOccupationTaxCommand& command,const OccupationFiscalQuote& quote){
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return commit_occupation_fiscal_command_in_transaction(w.common,w.occupation_fiscal,access.value->context(),w.services,actor,command,quote);
}
Outcome<BoundaryValue> query_occupation_fiscal_world(const WorldBundle& w,const StableId& actor,const StableId& site){
    auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return query_occupation_fiscal_entries(access.value->context(),w.services,actor,site);
}
}
''')
p=K+'world_bundle.cpp'
change(p,'#include "political_takeover_execution_world.hpp"','#include "political_takeover_execution_world.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'political_fiscal_seed_complete(bool(seed.political_fiscal)),','political_fiscal_seed_complete(bool(seed.political_fiscal)),occupation_fiscal(seed.occupation_fiscal?*seed.occupation_fiscal:OccupationFiscalState{}),occupation_fiscal_seed_complete(bool(seed.occupation_fiscal)),occupation_fiscal_settlement(seed.occupation_fiscal_settlement?*seed.occupation_fiscal_settlement:OccupationFiscalSettlementState{}),occupation_fiscal_settlement_seed_complete(bool(seed.occupation_fiscal_settlement)),')
change(p,'political_fiscal_seed_complete(other.political_fiscal_seed_complete),','political_fiscal_seed_complete(other.political_fiscal_seed_complete),occupation_fiscal(other.occupation_fiscal),occupation_fiscal_seed_complete(other.occupation_fiscal_seed_complete),occupation_fiscal_settlement(other.occupation_fiscal_settlement),occupation_fiscal_settlement_seed_complete(other.occupation_fiscal_settlement_seed_complete),')
change(p,'    for(const auto& row:w.political_fiscal.policies.rows())','    for(const auto& row:w.occupation_fiscal.policies.rows()){auto r=fact(row.first,political_occupation_fiscal_fact_kind,"politics");if(!r.ok)return r;}\n    for(const auto& row:w.occupation_fiscal_settlement.periods.rows()){auto r=fact(row.first,occupation_fiscal_period_kind,"local_economy");if(!r.ok)return r;}\n    for(const auto& row:w.occupation_fiscal_settlement.assessments.rows()){auto r=fact(row.first,occupation_fiscal_assessment_kind,"local_economy");if(!r.ok)return r;}\n    for(const auto& row:w.occupation_fiscal_settlement.payments.rows()){auto r=fact(row.first,occupation_fiscal_payment_kind,"local_economy");if(!r.ok)return r;}\n    for(const auto& row:w.political_fiscal.policies.rows())')
change(p,'return r.details?std::get_if<PoliticalFiscalEffects>(&r.details->data):nullptr;','return r.kind=="political_set_tax"&&r.details?std::get_if<PoliticalFiscalEffects>(&r.details->data):nullptr;')
change(p,'        valid=validate_economic_fiscal_world(*this);','        valid=validate_occupation_fiscal_world(*this);if(!valid.ok)return valid;\n        valid=validate_economic_fiscal_world(*this);')
change(p,'return political_takeover_enabled()&&political_takeover_grant_declares_resource(*catalog->common,*catalog->organization,*catalog->politics,*catalog->military,*catalog->political_takeover,grant.id,resource);','return political_takeover_enabled()&&(political_takeover_grant_declares_resource(*catalog->common,*catalog->organization,*catalog->politics,*catalog->military,*catalog->political_takeover,grant.id,resource)||(occupation_fiscal_enabled()&&occupation_fiscal_grant_declares_resource(*catalog->common,*catalog->organization,*catalog->politics,*catalog->military,*catalog->political_takeover,*catalog->occupation_fiscal,grant.id,resource)));')
p=K+'dispatcher.cpp'
change(p,'#include "political_takeover_execution_world.hpp"','#include "political_takeover_execution_world.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'    if(kind=="political_set_tax"&&!w.political_fiscal_enabled())','    if(kind==political_occupation_fiscal_command_kind&&!w.occupation_fiscal_enabled())return common_failure("OccupationFiscalProfileRequired",{},ErrorKind::unsupported_capability);\n    if(kind=="political_set_tax"&&!w.political_fiscal_enabled())')
change(p,'    if(const auto* fiscal=std::get_if<PoliticalSetTaxCommand>(&command.data)){auto q=','    if(const auto* fiscal=std::get_if<PoliticalSetOccupationTaxCommand>(&command.data)){auto q=prepare_occupation_fiscal_world(w,id,*fiscal);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n    if(const auto* fiscal=std::get_if<PoliticalSetTaxCommand>(&command.data)){auto legacy=require_legacy_fiscal_world(w,fiscal->jurisdiction_id);if(!legacy.ok)return {false,{},legacy.error};auto q=')
change(p,'    if(const auto* fiscal=std::get_if<PoliticalSetTaxCommand>(&op.command.data)){','    if(const auto* fiscal=std::get_if<PoliticalSetOccupationTaxCommand>(&op.command.data)){const auto* q=std::get_if<OccupationFiscalQuote>(&op.quote.data);if(!q)return failed<ReceiptDetails>(common_failure("InvalidOccupationFiscalQuote"));auto result=commit_occupation_fiscal_world(w,id,*fiscal,*q);if(!result.ok)return {false,{},result.error};return {true,{std::move(result.value)},{}};}\n    if(const auto* fiscal=std::get_if<PoliticalSetTaxCommand>(&op.command.data)){auto legacy=require_legacy_fiscal_world(w,fiscal->jurisdiction_id);if(!legacy.ok)return {false,{},legacy.error};')
p=K+'phase_plan.cpp'
change(p,'    auto fiscal=std::holds_alternative<PoliticalSetTaxCommand>(command.data)?std::optional<PoliticalFiscalState>{w.political_fiscal}:std::nullopt;','    auto fiscal=std::holds_alternative<PoliticalSetTaxCommand>(command.data)?std::optional<PoliticalFiscalState>{w.political_fiscal}:std::nullopt;\n    auto occupation_fiscal=std::holds_alternative<PoliticalSetOccupationTaxCommand>(command.data)?std::optional<OccupationFiscalState>{w.occupation_fiscal}:std::nullopt;')
change(p,'if(fiscal)w.political_fiscal=std::move(*fiscal);','if(fiscal)w.political_fiscal=std::move(*fiscal);if(occupation_fiscal)w.occupation_fiscal=std::move(*occupation_fiscal);')
p='native/world/src/resident/checkpoint/command_boundary.cpp'
change(p,'    if(kind==political_takeover_command_kind){auto decoded=','    if(kind==political_occupation_fiscal_command_kind){auto decoded=decode_political_set_occupation_tax(params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}\n    if(kind==political_takeover_command_kind){auto decoded=')
change(p,'else if constexpr(std::is_same_v<T,PoliticalTakeoverQuote>)','else if constexpr(std::is_same_v<T,OccupationFiscalQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"occupation_fiscal",encode_occupation_fiscal_quote(quote)}});\n                else if constexpr(std::is_same_v<T,PoliticalTakeoverQuote>)')
p='native/world/src/resident/checkpoint/command_boundary.cpp'
change(p,'    if(kind=="political_set_tax"){auto effect=','    if(kind=="political_set_tax"||kind==political_occupation_fiscal_command_kind){auto effect=')
change(p,'(kind=="political_set_tax"&&tag!=13)||(tag==13&&(kind!="political_set_tax"||!(c.schema_features&common_checkpoint::political_fiscal_receipts)))','((kind=="political_set_tax"||kind==political_occupation_fiscal_command_kind)&&tag!=13)||(tag==13&&((kind!="political_set_tax"&&kind!=political_occupation_fiscal_command_kind)||!(c.schema_features&common_checkpoint::political_fiscal_receipts)||(kind==political_occupation_fiscal_command_kind&&!(c.schema_features&occupation_fiscal_checkpoint_feature))))')
change(p,'if(r.kind=="political_set_tax"&&','if((r.kind=="political_set_tax"||r.kind==political_occupation_fiscal_command_kind)&&')
change(p,'value.kind=="political_set_tax")&&!has_details','value.kind=="political_set_tax"||value.kind==political_occupation_fiscal_command_kind)&&!has_details')
change(p,'    bool has_details=static_cast<bool>(value.details);','    if(value.kind==political_occupation_fiscal_command_kind&&!(c.schema_features&occupation_fiscal_checkpoint_feature))return c.reject("UnexpectedOccupationFiscalReceipt");\n    bool has_details=static_cast<bool>(value.details);')
change(p,'#include "resident/kernel/command_types.hpp"','#include "resident/kernel/command_types.hpp"\n#include "resident/domains/politics/takeover/politics_occupation_fiscal_profile.hpp"')
p=K+'world_codec.cpp'
change(p,'#include "world_codec.hpp"','#include "world_codec.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'bool takeover_enabled(const PreparedCatalog& c)','bool occupation_fiscal_enabled(const PreparedCatalog& c){return c.occupation_fiscal&&c.occupation_fiscal->enabled;}\nstd::vector<CheckpointDomain> occupation_fiscal_domains(std::vector<CheckpointDomain> domains){domains.push_back({"political_occupation_fiscal",1});domains.push_back({"occupation_fiscal_settlement",1});return domains;}\nbool takeover_enabled(const PreparedCatalog& c)')
change(p,'if(family_life_enabled(catalog))domains.push_back({"family_life",1});return domains;','if(family_life_enabled(catalog))domains.push_back({"family_life",1});if(occupation_fiscal_enabled(catalog))domains=occupation_fiscal_domains(std::move(domains));return domains;')
change(p,'{"political_takeover",{"1"}},','{"political_occupation_fiscal",{"1"}}, {"occupation_fiscal_settlement",{"1"}}, {"political_takeover",{"1"}},')
change(p,'        if (has("family_life")) domains.push_back({"family_life",1});','        if (has("family_life")) domains.push_back({"family_life",1});\n        if(has("political_occupation_fiscal")!=has("occupation_fiscal_settlement")||(has("political_occupation_fiscal")&&(!has("political_takeover")||!has("fiscal_settlement")||!has("political_fiscal"))))fail("OccupationFiscalProfileMismatch","checkpoint/module_versions");\n        if(has("political_occupation_fiscal"))domains=occupation_fiscal_domains(std::move(domains));')
change(p,'            const auto naval_markers=','            const bool occupation_fiscal=version("political_occupation_fiscal")==1;for(const auto* name:{"political_occupation_fiscal","occupation_fiscal_settlement"}){const auto count=std::count_if(versions.begin(),versions.end(),[&](const auto& v){return v.first==name;});if(count!=(occupation_fiscal?1:0)||version(name)!=(occupation_fiscal?1:0)||(occupation_fiscal&&!has(name,"1")))return {false,{},common_failure("OccupationFiscalProfileMismatch").error};}if(occupation_fiscal&&(!takeover||version("fiscal_settlement")!=1||version("political_fiscal")!=1))return {false,{},common_failure("OccupationFiscalProfileMismatch").error};\n            const auto naval_markers=')
# 配置在原接管块之后解析，配对启用，不由包能力并集隐式升级。
anchor='result->task_takeover_execution_enabled=true;}'
change(p,anchor,anchor+'\n        if(const auto* config=dynamic->find("political_occupation_fiscal")){if(!takeover_enabled(*result)||!political_fiscal_enabled(*result)||!settlement_enabled(*result))fail("OccupationFiscalDependenciesRequired","dynamic_world/political_occupation_fiscal");const auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"occupation_fiscal_v1")==caps.end())fail("OccupationFiscalCapabilityRequired","dynamic_world/political_occupation_fiscal");auto parsed=decode_occupation_fiscal_catalog(*config);if(!parsed.ok)return {false,{},parsed.error};if(!parsed.value.enabled)fail("OccupationFiscalProfileRequired","dynamic_world/political_occupation_fiscal");result->occupation_fiscal=std::make_shared<const OccupationFiscalCatalog>(std::move(parsed.value));result->occupation_fiscal_settlement_enabled=true;}')
change(p,'    if(political_fiscal_enabled(*result))id.module_versions.emplace_back("political_fiscal","1");','    if(political_fiscal_enabled(*result))id.module_versions.emplace_back("political_fiscal","1");\n    if(occupation_fiscal_enabled(*result)){id.module_versions.emplace_back("political_occupation_fiscal","1");id.module_versions.emplace_back("occupation_fiscal_settlement","1");}')
change(p,'    if(political_fiscal_enabled(catalog)){auto fiscal=','    if(occupation_fiscal_enabled(catalog)){auto political=decode_occupation_fiscal_state(field(source,"political_occupation_fiscal","state"));if(!political.ok)return {false,{},political.error};result.occupation_fiscal=std::make_shared<const OccupationFiscalState>(std::move(political.value));auto economic=decode_occupation_fiscal_settlement(field(source,"occupation_fiscal_settlement","state"));if(!economic.ok)return {false,{},economic.error};result.occupation_fiscal_settlement=std::make_shared<const OccupationFiscalSettlementState>(std::move(economic.value));}else if(source.find("political_occupation_fiscal")||source.find("occupation_fiscal_settlement"))fail("UnexpectedOccupationFiscalState","state");\n    if(political_fiscal_enabled(catalog)){auto fiscal=')
change(p,'    if(w.political_fiscal_enabled())set(output,"political_fiscal",encode_political_fiscal_state(w.political_fiscal));','    if(w.political_fiscal_enabled())set(output,"political_fiscal",encode_political_fiscal_state(w.political_fiscal));\n    if(w.occupation_fiscal_enabled()){set(output,"political_occupation_fiscal",encode_occupation_fiscal_state(w.occupation_fiscal));set(output,"occupation_fiscal_settlement",encode_occupation_fiscal_settlement(w.occupation_fiscal_settlement));}')
change(p,'CodecVisitor writer(s.payload);','CodecVisitor writer(s.payload);if(w.occupation_fiscal_enabled())writer.schema_features|=occupation_fiscal_checkpoint_feature;')
change(p,'    return encode_checkpoint(envelope);','    if(w.occupation_fiscal_enabled()){r=section("political_occupation_fiscal",[&](auto& c){return visit_occupation_fiscal_checkpoint(c,w.occupation_fiscal);});if(!r.ok)return {false,{},r.error};r=section("occupation_fiscal_settlement",[&](auto& c){return visit_occupation_fiscal_settlement_checkpoint(c,w.occupation_fiscal_settlement);});if(!r.ok)return {false,{},r.error};}\n    return encode_checkpoint(envelope);')
change(p,'std::uint32_t political_features=0;','std::uint32_t political_features=0;if(std::find(decoded.value.identity.catalog.module_versions.begin(),decoded.value.identity.catalog.module_versions.end(),std::make_pair(StableId{"political_occupation_fiscal"},StableId{"1"}))!=decoded.value.identity.catalog.module_versions.end())political_features|=occupation_fiscal_checkpoint_feature;')
change(p,'for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);','for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);if(occupation_fiscal_enabled(catalog))reader.schema_features|=occupation_fiscal_checkpoint_feature;')
change(p,'        else if(section.domain=="political_fiscal")','        else if(section.domain=="political_occupation_fiscal"){auto state=std::make_shared<OccupationFiscalState>();r=visit_occupation_fiscal_checkpoint(reader,*state);seed.occupation_fiscal=std::move(state);}\n        else if(section.domain=="occupation_fiscal_settlement"){auto state=std::make_shared<OccupationFiscalSettlementState>();r=visit_occupation_fiscal_settlement_checkpoint(reader,*state);seed.occupation_fiscal_settlement=std::move(state);}\n        else if(section.domain=="political_fiscal")')
p=K+'native_initial_state.cpp'
change(p,'    if(dynamic.find("political_takeover"))','    if(dynamic.find("political_occupation_fiscal")){admitted.module_versions.emplace_back("political_occupation_fiscal","1");admitted.module_versions.emplace_back("occupation_fiscal_settlement","1");}\n    if(dynamic.find("political_takeover"))')
change(p,'    if(mutable_catalog->political_takeover&&mutable_catalog->political_takeover->enabled)','    if(mutable_catalog->occupation_fiscal&&mutable_catalog->occupation_fiscal->enabled){seed.occupation_fiscal=std::make_shared<const OccupationFiscalState>();seed.occupation_fiscal_settlement=std::make_shared<const OccupationFiscalSettlementState>();}\n    if(mutable_catalog->political_takeover&&mutable_catalog->political_takeover->enabled)')
p='native/world/src/resident/storage/common_codec.cpp'
change(p,'kind=="political_fiscal_policy_changed"||','kind=="political_fiscal_policy_changed"||kind=="political_occupation_fiscal_policy_changed"||')

p=K+'occupation_fiscal_integration.hpp'
change(p,'#include "political_takeover_integration.hpp"','#include "political_takeover_integration.hpp"\n#include "political_takeover_execution_world.hpp"\n#include "resident/domains/local_economy/fiscal/occupation_fiscal_settlement.hpp"\n#include "resident/domains/politics/takeover/politics_occupation_fiscal_civil.hpp"')
change(p,'Status validate_occupation_fiscal_world(const WorldBundle&);','''// 金额结算与民心读取复用同一World身份；所有借口由这个栈对象持有。
class OccupationFiscalSettlementWorldAccess final:public OccupationFiscalCivilRead {
public:
    static Outcome<std::unique_ptr<OccupationFiscalSettlementWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
    const OccupationFiscalSettlementContext& context()const{return context_;}
    Outcome<std::optional<OccupationFiscalCivilAssessment>> occupation_fiscal_civil_period(const StableId&)const override;
private:
    OccupationFiscalSettlementWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverExecutionWorldAccess>,std::unique_ptr<OccupationFiscalWorldAccess>,const DynamicStepInput*);
    const WorldBundle& world_;
    std::unique_ptr<PoliticalTakeoverExecutionWorldAccess> takeover_;
    std::unique_ptr<OccupationFiscalWorldAccess> policy_;
    OrganizationAccess organizations_;
    std::unique_ptr<FiscalPolicyAccess> legacy_;
    OccupationFiscalSettlementContext context_;
};
Status initialize_occupation_fiscal_world(WorldBundle&);
Status validate_occupation_fiscal_world(const WorldBundle&);''')
p=K+'occupation_fiscal_integration.cpp'
change(p,'Status validate_occupation_fiscal_world(const WorldBundle& w){','''OccupationFiscalSettlementWorldAccess::OccupationFiscalSettlementWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverExecutionWorldAccess> takeover,std::unique_ptr<OccupationFiscalWorldAccess> policy,const DynamicStepInput* phase):world_(w),takeover_(std::move(takeover)),policy_(std::move(policy)),organizations_(w.services.organization_access()),legacy_(phase?std::make_unique<FiscalPolicyAccess>(w.common,*w.catalog->common,w.politics,w.political_fiscal,*w.catalog->political_fiscal,*phase,takeover_->fiscal()):std::make_unique<FiscalPolicyAccess>(w.common,*w.catalog->common,w.politics,w.political_fiscal,*w.catalog->political_fiscal,takeover_->fiscal())),context_{w.common,w.local_economy,*w.catalog->local_economy,organizations_,*legacy_,*w.catalog->fiscal_settlement,w.fiscal_settlement,policy_->policy(),takeover_->economy(),phase,takeover_->taxpayer()}{}
Outcome<std::unique_ptr<OccupationFiscalSettlementWorldAccess>> OccupationFiscalSettlementWorldAccess::create(const WorldBundle& w,const DynamicStepInput* phase){
    auto policy=OccupationFiscalWorldAccess::create(w,phase);if(!policy.ok)return {false,{},policy.error};
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(w,phase);if(!takeover.ok)return {false,{},takeover.error};
    return {true,std::unique_ptr<OccupationFiscalSettlementWorldAccess>(new OccupationFiscalSettlementWorldAccess(w,std::move(takeover.value),std::move(policy.value),phase)),{}};
}
Outcome<std::optional<OccupationFiscalCivilAssessment>> OccupationFiscalSettlementWorldAccess::occupation_fiscal_civil_period(const StableId& id)const{
    if(context_.phase)return {false,{},common_failure("OccupationFiscalCivilCommittedClockRequired").error};
    auto read=read_occupation_fiscal_period(context_,world_.occupation_fiscal_settlement,id);if(!read.ok)return {false,{},read.error};if(!read.value)return {true,std::nullopt,{}};
    const auto* period=world_.occupation_fiscal_settlement.periods.find(id);if(!period)return {false,{},common_failure("MissingOccupationFiscalPeriod",id).error};
    return {true,OccupationFiscalCivilAssessment{world_.common.state().identity,id,read.value->due_tick,period->policy},{}};
}
Status initialize_occupation_fiscal_world(WorldBundle& w){
    if(!w.occupation_fiscal_enabled())return common_success();auto access=OccupationFiscalSettlementWorldAccess::create(w);if(!access.ok)return {false,{},access.error};
    if(!w.occupation_fiscal_settlement.periods.empty()||!w.occupation_fiscal_settlement.assessments.empty()||!w.occupation_fiscal_settlement.payments.empty()||!w.occupation_fiscal_settlement.buckets.empty()||!w.occupation_fiscal_settlement.legacy_carry_claims.empty()||!w.occupation_fiscal_settlement.suspended_periods.empty())return common_failure("OccupationFiscalAlreadyInitialized");
    auto initial=make_initial_occupation_fiscal_settlement(access.value->context());if(!initial.ok)return {false,{},initial.error};w.occupation_fiscal_settlement=std::move(initial.value);return common_success();
}
Status validate_occupation_fiscal_world(const WorldBundle& w){''')
p=K+'economic_world_integration.cpp'
change(p,'#include "political_takeover_execution_world.hpp"','#include "political_takeover_execution_world.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'    return validate_fiscal_settlement(world.common,world.local_economy,*world.catalog->local_economy,organizations,policy,*world.catalog->fiscal_settlement,world.fiscal_settlement,takeover.value->economy());','    auto legacy=validate_fiscal_settlement(world.common,world.local_economy,*world.catalog->local_economy,organizations,policy,*world.catalog->fiscal_settlement,world.fiscal_settlement,takeover.value->economy());if(!legacy.ok)return legacy;\n    if(!world.occupation_fiscal_enabled())return common_success();auto current=OccupationFiscalSettlementWorldAccess::create(world);if(!current.ok)return {false,{},current.error};return validate_occupation_fiscal_settlement(current.value->context(),world.occupation_fiscal_settlement);')
change(p,'    return step_fiscal_settlement(world.common,world.local_economy,*world.catalog->local_economy,organizations,policy,*world.catalog->fiscal_settlement,world.fiscal_settlement,input,takeover.value->economy());','    auto legacy=step_fiscal_settlement(world.common,world.local_economy,*world.catalog->local_economy,organizations,policy,*world.catalog->fiscal_settlement,world.fiscal_settlement,input,takeover.value->economy());if(!legacy.ok||!world.occupation_fiscal_enabled())return legacy;\n    auto current=OccupationFiscalSettlementWorldAccess::create(world,&input);if(!current.ok)return {false,{},current.error};auto step=step_occupation_fiscal_settlement(world.common,world.occupation_fiscal_settlement,current.value->context(),input);if(!step.ok)return step;legacy.value.fact_ids.insert(legacy.value.fact_ids.end(),step.value.fact_ids.begin(),step.value.fact_ids.end());legacy.value.receipt_ids.insert(legacy.value.receipt_ids.end(),step.value.receipt_ids.begin(),step.value.receipt_ids.end());return legacy;')
p=K+'dynamic_integration.cpp'
change(p,'#include "political_takeover_execution_world.hpp"','#include "political_takeover_execution_world.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'    PoliticsState politics;PoliticalFiscalState political_fiscal;','    OccupationFiscalState occupation_fiscal;OccupationFiscalSettlementState occupation_fiscal_settlement;\n    PoliticsState politics;PoliticalFiscalState political_fiscal;')
change(p,'political_fiscal(w.political_fiscal),','political_fiscal(w.political_fiscal),occupation_fiscal(w.occupation_fiscal),occupation_fiscal_settlement(w.occupation_fiscal_settlement),')
change(p,'w.political_fiscal=std::move(political_fiscal);','w.political_fiscal=std::move(political_fiscal);w.occupation_fiscal=std::move(occupation_fiscal);w.occupation_fiscal_settlement=std::move(occupation_fiscal_settlement);')
change(p,'return common_failure("DynamicWorldAlreadyInitialized");','return common_failure("DynamicWorldAlreadyInitialized");\n    auto initialized_fiscal=initialize_occupation_fiscal_world(w);if(!initialized_fiscal.ok)return initialized_fiscal;')
p=K+'political_fiscal_civil_integration.cpp'
change(p,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'    return settle_political_fiscal_civil_in_transaction(world.common,world.politics,world.politics_governance,*world.catalog->politics_governance,world.political_fiscal,world.fiscal_settlement,phase,effects);','''    auto legacy=settle_political_fiscal_civil_in_transaction(world.common,world.politics,world.politics_governance,*world.catalog->politics_governance,world.political_fiscal,world.fiscal_settlement,phase,effects);if(!legacy.ok||!world.occupation_fiscal_enabled())return legacy;
    auto current=OccupationFiscalSettlementWorldAccess::create(world);if(!current.ok)return {false,{},current.error};
    for(const auto& id:effects.fact_ids){const auto* header=world.common.state().facts.find(id);if(!header||header->kind!=occupation_fiscal_period_kind)continue;auto result=apply_occupation_fiscal_civil_in_transaction(world.common,world.politics,world.politics_governance,*world.catalog->politics_governance,&world.fiscal_settlement,*current.value,id);if(!result.ok)return {false,{},result.error};}
    return common_success();''')
p=K+'world_bundle.cpp'
change(p,'if(politics_governance_enabled()){GovernanceTaskRead governance_read','if(politics_governance_enabled()){std::unique_ptr<OccupationFiscalSettlementWorldAccess> occupation_civil;if(occupation_fiscal_enabled()){auto access=OccupationFiscalSettlementWorldAccess::create(*this);if(!access.ok)return {false,{},access.error};occupation_civil=std::move(access.value);}GovernanceTaskRead governance_read')
change(p,'fiscal_settlement_enabled()?&fiscal_settlement:nullptr);if(!valid.ok)return valid;}','fiscal_settlement_enabled()?&fiscal_settlement:nullptr,occupation_civil.get());if(!valid.ok)return valid;}')
p=K+'live_view_queries.cpp'
change(p,'#include "political_takeover_execution_world.hpp"','#include "political_takeover_execution_world.hpp"\n#include "occupation_fiscal_integration.hpp"')
change(p,'    if(query=="political_takeover")','    if(query=="political_occupation_fiscal"){boundary::fields(args,{"place_id"},{},"args");return query_occupation_fiscal_world(world,world.common.state().metadata.player_actor_id,boundary::get_string(args,"place_id","args"));}\n    if(query=="political_takeover")')

p='game/ui/native_resident_backend.gd'
change(p,'func political_takeover_enabled() -> bool:','''## 新税政和实际税账同一显式配置，不由已有财政页隐式启用。
func political_occupation_fiscal_enabled() -> bool:
	var config: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("political_occupation_fiscal",{})
	return _installed and config.get("enabled",false) and config.get("version",0)==1

func political_occupation_fiscal_entries(place_id: String) -> Dictionary:
	return demand_view("political_occupation_fiscal",{"place_id":place_id})

func political_takeover_enabled() -> bool:''')
change(p,'\tif definition.get("dynamic_world",{}).has("political_takeover"):','\tif definition.get("dynamic_world",{}).has("political_occupation_fiscal"):\n\t\tfor module in ["political_occupation_fiscal","occupation_fiscal_settlement"]: expected["module_versions"].append({"name":module,"version":"1"})\n\tif definition.get("dynamic_world",{}).has("political_takeover"):')
p='game/ui/resident_page_requests.gd'
change(p,'\tvar pages: Array=[host.kernel.politics_entries(place_id)]','\tvar pages: Array=[host.kernel.politics_entries(place_id)]\n\tif host.kernel.has_method("political_occupation_fiscal_enabled") and host.kernel.political_occupation_fiscal_enabled():\n\t\tpages.append(host.kernel.political_occupation_fiscal_entries(place_id))')
p='game/ui/main.gd'
change(p,'\tenabled=enabled or (kernel.has_method("political_takeover_enabled")','\tenabled=enabled or (kernel.has_method("political_occupation_fiscal_enabled") and kernel.political_occupation_fiscal_enabled() and action.get("command","")=="political_set_occupation_tax")\n\tenabled=enabled or (kernel.has_method("political_takeover_enabled")')
p='tools/content/links.py'
change(p,'    from .political_takeover_links import check_political_takeover','    from .political_takeover_links import check_political_takeover\n    from .political_occupation_fiscal_links import check_political_occupation_fiscal')
change(p,' + check_political_takeover(pack,index)',' + check_political_takeover(pack,index) + check_political_occupation_fiscal(pack,index)')
p='tools/content/dynamic_world_links.py'
change(p,'from .political_takeover_links import political_takeover_grant_declares_assets','from .political_takeover_links import political_takeover_grant_declares_assets\nfrom .political_occupation_fiscal_links import occupation_fiscal_grant_declares_assets')
change(p,'not political_takeover_grant_declares_assets(scenario, index, grant) and','not political_takeover_grant_declares_assets(scenario, index, grant) and not occupation_fiscal_grant_declares_assets(scenario,index,grant) and')
change(p,"                allowed.add('political_assume_public_responsibilities')","                allowed.add('political_assume_public_responsibilities')\n            if dynamic.get('political_occupation_fiscal', {}).get('enabled'):\n                allowed.add('political_set_occupation_tax')")
p='schemas/content.schema.json';data=json.loads(get(p));data['properties']['required_capabilities']['items']['enum'].append('occupation_fiscal_v1');data['$defs']['dynamic_world']['properties']['political_occupation_fiscal']={'type':'object','properties':{'enabled':{'type':'boolean','enum':[True]},'version':{'type':'integer','enum':[1]}},'required':['enabled','version'],'additionalProperties':False};files[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
p='game/data/rule_capabilities.json';data=json.loads(get(p));data['capabilities'].append({'capability_id':'occupation_fiscal_v1','rule_version':'occupation_tax_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_occupation_fiscal.enabled','equals':True}],'supported_semantics':[],'test_ids':['test_political_career_player_flow','test_fiscal_settlement_panel'],'evidence':[]});files[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'


# 新债与纳税人资产读均借同一World候选，临时对象的生命期覆盖所有消费者。
p=K+'political_takeover_integration.cpp'
change(p,'w.envoy_tasks_enabled()?&w.envoy_tasks:nullptr,phase};','w.envoy_tasks_enabled()?&w.envoy_tasks:nullptr,phase,w.occupation_fiscal_enabled()?&w.occupation_fiscal_settlement:nullptr};')
p=K+'political_takeover_execution_world.hpp'
change(p,'#include "political_takeover_integration.hpp"','#include "political_takeover_integration.hpp"\n#include "resident/domains/politics/takeover/politics_occupation_taxpayer.hpp"')
change(p,'    const FiscalPolicyOccupationRead* fiscal()const','    const OccupationFiscalTaxpayerRead* taxpayer()const{return taxpayer_?&*taxpayer_:nullptr;}\n    const FiscalPolicyOccupationRead* fiscal()const')
change(p,'    std::optional<TaskPersonalAdvanceAccess> advances_;','    std::optional<PoliticalOccupationFiscalTaxpayerAccess> taxpayer_;\n    std::optional<TaskPersonalAdvanceAccess> advances_;')
p=K+'political_takeover_execution_world.cpp'
change(p,'    if(w.tasks.personal_advances)','    if(w.occupation_fiscal_enabled())taxpayer_.emplace(politics_->context());\n    if(w.tasks.personal_advances)')
change(p,'w.catalog->organization.get(),phase});','w.catalog->organization.get(),phase,w.occupation_fiscal_enabled()?&w.occupation_fiscal_settlement:nullptr,taxpayer()});')
p=K+'occupation_fiscal_integration.cpp'
change(p,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "resident/domains/local_economy/fiscal/occupation_fiscal_view.hpp"\n#include "resident/domains/local_economy/views/local_economy_view.hpp"')
change(p,'return prepare_occupation_fiscal_command(access.value->context(),w.services,actor,command);', 'auto quote=prepare_occupation_fiscal_command(access.value->context(),w.services,actor,command);if(!quote.ok)return quote;auto money=OccupationFiscalSettlementWorldAccess::create(w);if(!money.ok)return {false,{},money.error};const auto* district=w.catalog->politics->jurisdictions.find(command.jurisdiction_id);if(!district)return {false,{},common_failure("UnknownPoliticalJurisdiction").error};auto source=validate_occupation_fiscal_new_sources(money.value->context(),district->seat_place_id);if(!source.ok)return {false,{},source.error};return quote;')
change(p,'auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return commit_occupation_fiscal_command_in_transaction', 'auto prepared=prepare_occupation_fiscal_world(w,actor,command);if(!prepared.ok)return {false,{},prepared.error};auto access=OccupationFiscalWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return commit_occupation_fiscal_command_in_transaction')
change(p,'Outcome<BoundaryValue> query_occupation_fiscal_world(','''Outcome<BoundaryValue> query_occupation_fiscal_settlement_world(const WorldBundle& w,const BoundaryValue& args){
    auto query=decode_local_economy_query(args,w.common.state().metadata.player_actor_id);if(!query.ok)return {false,{},query.error};auto access=OccupationFiscalSettlementWorldAccess::create(w);if(!access.ok)return {false,{},access.error};auto index=build_occupation_fiscal_settlement_index(access.value->context(),w.occupation_fiscal_settlement,w.organization);if(!index.ok)return {false,{},index.error};auto page=query_occupation_fiscal_settlement(access.value->context(),w.occupation_fiscal_settlement,index.value,query.value);if(!page.ok)return {false,{},page.error};return {true,encode_occupation_fiscal_settlement_page(page.value),{}};
}
Outcome<BoundaryValue> query_occupation_fiscal_world(''')
p=K+'occupation_fiscal_integration.hpp'
change(p,'Outcome<BoundaryValue> query_occupation_fiscal_world(', 'Outcome<BoundaryValue> query_occupation_fiscal_settlement_world(const WorldBundle&,const BoundaryValue& args);\nOutcome<BoundaryValue> query_occupation_fiscal_world(')
p=K+'live_view_queries.cpp'
change(p,'    if(query=="fiscal_settlement")','    if(query=="occupation_fiscal_settlement")return query_occupation_fiscal_settlement_world(world,args);\n    if(query=="fiscal_settlement")')
p=K+'world_bundle.cpp'
change(p,'kind==occupation_fiscal_assessment_kind){count','kind==occupation_fiscal_assessment_kind){count') if False else None
# 具名暂停是独立真实期次，不伪装为已评税或已完成民心事件。
s=get(p)
a='if(f.kind==occupation_fiscal_period_kind)'
if a in s:
 change(p,a,'if(f.kind==occupation_fiscal_suspension_kind){if(occupation_fiscal_enabled()&&occupation_fiscal_settlement.suspended_periods.find(f.id))++owners;}else '+a)
else:
 a='if(fact.kind==occupation_fiscal_period_kind)'
 if a in s:change(p,a,'if(fact.kind==occupation_fiscal_suspension_kind){if(occupation_fiscal_enabled()&&occupation_fiscal_settlement.suspended_periods.find(fact.id))++owners;}else '+a)
p='game/ui/native_resident_backend.gd'
change(p,'func political_occupation_fiscal_entries(place_id: String) -> Dictionary:','''## 接任后的真实税账使用自己的游标，不混入原财政1历史。
func occupation_fiscal_settlement(place_id: String, cursor: Variant=null, authority: Variant=null) -> Dictionary:
	var args: Dictionary={"place_id":place_id}
	if cursor is Dictionary and not cursor.is_empty(): args["cursor"]=cursor.duplicate(true)
	if authority is Dictionary and not authority.is_empty(): args["authority"]=authority.duplicate(true)
	return demand_view("occupation_fiscal_settlement",args)

func political_occupation_fiscal_entries(place_id: String) -> Dictionary:''')


p=K+'world_bundle.cpp'
a='    for(const auto& row:w.occupation_fiscal_settlement.periods.rows())'
change(p,a,'    for(const auto& row:w.occupation_fiscal_settlement.suspended_periods.rows()){auto r=fact(row.first,occupation_fiscal_suspension_kind,"local_economy");if(!r.ok)return r;}\n'+a)
p=K+'economic_world_integration.cpp'
a='encoded.as<BoundaryValue::Dict>()->emplace_back("fiscal_available",BoundaryValue{world.fiscal_settlement_enabled()});'
change(p,a,a+'if(world.occupation_fiscal_enabled())encoded.as<BoundaryValue::Dict>()->emplace_back("occupation_fiscal_available",BoundaryValue{true});')
p=K+'dynamic_access.cpp'
a='    if(world_.fiscal_settlement_enabled())if(auto result=fiscal_settlement_dynamic_fact(world_.fiscal_settlement,*world_.catalog->fiscal_settlement,id))return result;'
change(p,a,a+'\n    if(world_.occupation_fiscal_enabled())if(auto result=occupation_fiscal_dynamic_fact(world_.occupation_fiscal_settlement,*world_.catalog->fiscal_settlement,id))return result;')

for p,s in files.items():
 q=CAND/p;q.parent.mkdir(parents=True,exist_ok=True);q.write_text(s)
(OUT/'registration-base.json').write_text(json.dumps({'base':BASE,'files':[{'path':p,'before_sha256':hashlib.sha256(originals[p].encode()).hexdigest(),'after_sha256':hashlib.sha256(s.encode()).hexdigest()} for p,s in files.items()]},indent=2))
(OUT/'registration.patch').write_text(''.join(''.join(difflib.unified_diff(originals[p].splitlines(True),s.splitlines(True),fromfile='a/'+p,tofile='b/'+p)) for p,s in files.items()))
print(len(files),'shared candidates')
