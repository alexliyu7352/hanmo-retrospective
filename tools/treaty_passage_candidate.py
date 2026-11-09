"""从固定中央源码生成双签通行候选；仅写新cache目录，供中央逐叶串合。"""
from pathlib import Path
import argparse, subprocess, json, hashlib, difflib
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--base',default='b517970c105155590213d78c3219c219eda7c0d4');a=p.parse_args()
R=a.repo;B=a.base;O=a.output;F={};OLD={};K='native/world/src/resident/kernel/';D='native/world/src/resident/domains/'
assert not O.exists();O.mkdir(parents=True)
def get(p):
 if p not in F:F[p]=subprocess.check_output(['git','show',B+':'+p],cwd=R).decode();OLD[p]=F[p]
 return F[p]
def change(p,x,y,n=1):
 s=get(p);assert s.count(x)==n,(p,x[:110],s.count(x),n);F[p]=s.replace(x,y)
def add(p,s):assert p not in F;F[p]=s;OLD[p]=''
p='native/world/include/hanmo/resident/model_contract.hpp'
change(p,'struct OccupationFiscalCatalog;','struct TreatyPassageCatalog;struct TreatyPassageState;\nstruct OccupationFiscalCatalog;')
change(p,'    std::shared_ptr<const OccupationFiscalCatalog> occupation_fiscal;','    std::shared_ptr<const TreatyPassageCatalog> treaty_passage;\n    std::shared_ptr<const OccupationFiscalCatalog> occupation_fiscal;')
p='native/world/src/resident/model/common_model.hpp'
change(p,'    std::shared_ptr<const OccupationFiscalState> occupation_fiscal;','    std::shared_ptr<const TreatyPassageState> treaty_passage;\n    std::shared_ptr<const OccupationFiscalState> occupation_fiscal;')
p=K+'command_types.hpp'
change(p,'#include "political_takeover_integration.hpp"','#include "political_takeover_integration.hpp"\n#include "resident/domains/politics/treaty/treaty_passage_command.hpp"')
change(p,'FamilyBequestCommand>;','FamilyBequestCommand,TreatyPassageCommand>;')
change(p,'FamilyBequestQuote>;','FamilyBequestQuote,TreatyPassageQuote>;')
p=K+'world_bundle.hpp'
change(p,'    DiplomacyState diplomacy;bool diplomacy_seed_complete=false;','    TreatyPassageState treaty_passage;bool treaty_passage_seed_complete=false;\n    bool treaty_passage_enabled()const{return catalog->treaty_passage&&catalog->treaty_passage->enabled;}\n    DiplomacyState diplomacy;bool diplomacy_seed_complete=false;')
add(K+'treaty_passage_integration.hpp','''// 一个栈对象持有同候选外交、接管与文书读口；不跨NPC回滚缓存子对象地址。
#pragma once
#include "political_takeover_integration.hpp"
#include "resident/domains/politics/treaty/treaty_passage_access.hpp"
#include "resident/domains/politics/treaty/treaty_passage_command.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "resident/domains/tasks/envoy_task_rules.hpp"
namespace hanmo::resident {
class TreatyPassageWorldAccess final {
public:
 static Outcome<std::unique_ptr<TreatyPassageWorldAccess>> create(const WorldBundle&);
 const TreatyPassageContext& context()const{return context_;}
private:
 TreatyPassageWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverWorldAccess>);
 std::unique_ptr<PoliticalTakeoverWorldAccess> takeover_;
 InteractionAccess interaction_;
 DiplomaticPresentRead present_;
 DiplomaticMissionAccess missions_;
 std::optional<DiplomaticReportAccess> reports_;
 std::unique_ptr<EnvoyTaskRead> envoy_;
 TreatyPassageTerritoryAccess territory_;
 TreatyPassageContext context_;
};
Status validate_treaty_passage_world(const WorldBundle&);
Outcome<TreatyPassageQuote> prepare_treaty_passage_world(const WorldBundle&,const StableId&,const TreatyPassageCommand&);
Outcome<StableId> commit_treaty_passage_world(WorldBundle&,const StableId&,const TreatyPassageCommand&,const TreatyPassageQuote&);
Outcome<bool> treaty_passage_world_admission(const WorldBundle&,const StableId&,const StableId&,const StableId&);
Outcome<BoundaryValue> merge_treaty_passage_world_page(const WorldBundle&,const StableId&,BoundaryValue);
}
''')
add(K+'treaty_passage_integration.cpp','''// 双签只产生具名政治事实；旧外交、任务与钱货均由原业务独立维护。
#include "treaty_passage_integration.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
TreatyPassageWorldAccess::TreatyPassageWorldAccess(const WorldBundle& w,std::unique_ptr<PoliticalTakeoverWorldAccess> takeover):takeover_(std::move(takeover)),interaction_(w.services.interaction_access()),present_{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,interaction_},missions_(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy),territory_(w.common,w.politics,*w.catalog->politics,takeover_?&takeover_->context():nullptr),context_{present_,*w.catalog->treaty_passage,w.treaty_passage,territory_,nullptr}{
 if(w.diplomacy.version==2){reports_.emplace(present_,*w.diplomacy.v2);envoy_=std::make_unique<EnvoyTaskRead>(w.common,w.tasks,w.envoy_tasks,missions_,missions_,*reports_);}else envoy_=std::make_unique<EnvoyTaskRead>(w.common,w.tasks,w.envoy_tasks,missions_,missions_);context_.envoy_tasks=envoy_.get();
}
Outcome<std::unique_ptr<TreatyPassageWorldAccess>> TreatyPassageWorldAccess::create(const WorldBundle& w){
 if(!w.treaty_passage_enabled()||!w.treaty_passage_seed_complete||!w.politics_enabled()||!w.diplomacy_enabled()||!w.catalog->organization||!w.catalog->interaction||!w.catalog->diplomacy||(w.diplomacy.version==2&&!w.diplomacy.v2))return {false,{},common_failure("TreatyPassageProfileRequired").error};
 std::unique_ptr<PoliticalTakeoverWorldAccess> takeover;if(w.political_takeover_enabled()){auto read=PoliticalTakeoverWorldAccess::create(w,*w.catalog->political_takeover,w.political_takeover);if(!read.ok)return {false,{},read.error};takeover=std::move(read.value);}
 return {true,std::unique_ptr<TreatyPassageWorldAccess>(new TreatyPassageWorldAccess(w,std::move(takeover))),{}};
}
Status validate_treaty_passage_world(const WorldBundle& w){
 const bool enabled=w.treaty_passage_enabled();auto layout=validate_treaty_passage_layout(enabled,w.catalog->identity.module_versions,w.treaty_passage_seed_complete?&w.treaty_passage:nullptr);if(!layout.ok)return layout;auto history=validate_treaty_passage_command_history(w.common,enabled);if(!history.ok)return history;
 if(!enabled)return w.treaty_passage.deliveries.empty()&&w.treaty_passage.signatures.empty()?common_success():common_failure("UnexpectedTreatyPassageState");
 auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};auto state=validate_treaty_passage_state(access.value->context());if(!state.ok)return state;return validate_treaty_passage_receipt_sources(access.value->context(),[](const CommitResult& result){return result.details&&std::holds_alternative<Done>(result.details->data);});
}
Outcome<TreatyPassageQuote> prepare_treaty_passage_world(const WorldBundle& w,const StableId& actor,const TreatyPassageCommand& command){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return preview_treaty_passage_command(access.value->context(),actor,command);}
Outcome<StableId> commit_treaty_passage_world(WorldBundle& w,const StableId& actor,const TreatyPassageCommand& command,const TreatyPassageQuote& quote){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return apply_treaty_passage_command_in_transaction(w.common,w.treaty_passage,access.value->context(),actor,command,quote);}
Outcome<bool> treaty_passage_world_admission(const WorldBundle& w,const StableId& actor,const StableId& place,const StableId& terms){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,false,access.error};return TreatyPassageAccess(access.value->context(),w.services).treaty_personal_passage(actor,place,terms);}
Outcome<BoundaryValue> merge_treaty_passage_world_page(const WorldBundle& w,const StableId& actor,BoundaryValue page){if(!w.treaty_passage_enabled())return {true,std::move(page),{}};auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};return merge_treaty_passage_page(access.value->context(),*w.catalog->common,actor,std::move(page));}
}
''')
p=K+'world_bundle.cpp'
change(p,'#include "occupation_fiscal_integration.hpp"','#include "occupation_fiscal_integration.hpp"\n#include "treaty_passage_integration.hpp"')
change(p,'diplomacy_seed_complete(bool(seed.diplomacy)),','diplomacy_seed_complete(bool(seed.diplomacy)),treaty_passage(seed.treaty_passage?*seed.treaty_passage:TreatyPassageState{}),treaty_passage_seed_complete(bool(seed.treaty_passage)),')
change(p,'diplomacy_seed_complete(other.diplomacy_seed_complete),','diplomacy_seed_complete(other.diplomacy_seed_complete),treaty_passage(other.treaty_passage),treaty_passage_seed_complete(other.treaty_passage_seed_complete),')
change(p,'    for(const auto& row:w.occupation_fiscal.policies.rows())','    for(const auto& row:w.treaty_passage.deliveries.rows()){auto r=fact(row.first,treaty_passage_delivery_kind,"politics");if(!r.ok)return r;}\n    for(const auto& row:w.treaty_passage.signatures.rows()){auto r=fact(row.first,treaty_passage_signature_kind,"politics");if(!r.ok)return r;}\n    for(const auto& row:w.occupation_fiscal.policies.rows())')
change(p,'        valid=validate_occupation_fiscal_world(*this);','        valid=validate_treaty_passage_world(*this);if(!valid.ok)return valid;\n        valid=validate_occupation_fiscal_world(*this);')
p=K+'dispatcher.cpp'
change(p,'#include "occupation_fiscal_integration.hpp"','#include "occupation_fiscal_integration.hpp"\n#include "treaty_passage_integration.hpp"')
change(p,'    if(kind==political_occupation_fiscal_command_kind','    if(treaty_passage_command_kind(kind)&&!w.treaty_passage_enabled())return common_failure("TreatyPassageProfileRequired",{},ErrorKind::unsupported_capability);\n    if(kind==political_occupation_fiscal_command_kind')
change(p,'    if(const auto* fiscal=std::get_if<PoliticalSetOccupationTaxCommand>(&command.data))','    if(const auto* treaty=std::get_if<TreatyPassageCommand>(&command.data)){auto q=prepare_treaty_passage_world(w,id,*treaty);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n    if(const auto* fiscal=std::get_if<PoliticalSetOccupationTaxCommand>(&command.data))')
change(p,'    if(const auto* fiscal=std::get_if<PoliticalSetOccupationTaxCommand>(&op.command.data))','    if(const auto* treaty=std::get_if<TreatyPassageCommand>(&op.command.data)){if(op.command.kind!=treaty_passage_command_kind(*treaty))return failed<ReceiptDetails>(common_failure("InvalidTreatyPassageCommand"));const auto* q=std::get_if<TreatyPassageQuote>(&op.quote.data);if(!q)return failed<ReceiptDetails>(common_failure("InvalidTreatyPassageQuote"));auto r=commit_treaty_passage_world(w,id,*treaty,*q);if(!r.ok)return {false,{},r.error};return {true,{Done{}},{}};}\n    if(const auto* fiscal=std::get_if<PoliticalSetOccupationTaxCommand>(&op.command.data))')
p=K+'phase_plan.cpp'
change(p,'    auto occupation_fiscal=','    auto treaty_passage=std::holds_alternative<TreatyPassageCommand>(command.data)?std::optional<TreatyPassageState>{w.treaty_passage}:std::nullopt;\n    auto occupation_fiscal=')
change(p,'if(occupation_fiscal)w.occupation_fiscal=std::move(*occupation_fiscal);','if(occupation_fiscal)w.occupation_fiscal=std::move(*occupation_fiscal);if(treaty_passage)w.treaty_passage=std::move(*treaty_passage);')
p=K+'live_view_queries.cpp'
change(p,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "treaty_passage_integration.hpp"')
change(p,'return query_diplomacy_business(world.diplomacy_read(ia,&navigation),world.common.state().metadata.player_actor_id,boundary::get_string(args,"place_id","args"));','auto page=query_diplomacy_business(world.diplomacy_read(ia,&navigation),world.common.state().metadata.player_actor_id,boundary::get_string(args,"place_id","args"));if(!page.ok)return page;return merge_treaty_passage_world_page(world,world.common.state().metadata.player_actor_id,std::move(page.value));')
# 独立schema尾段，只在显式新模块下启用回执特征。
p=K+'world_codec.cpp'
change(p,'#include "world_codec.hpp"','#include "world_codec.hpp"\n#include "treaty_passage_integration.hpp"')
change(p,'bool occupation_fiscal_enabled(const PreparedCatalog& c)','bool treaty_passage_enabled(const PreparedCatalog& c){return c.treaty_passage&&c.treaty_passage->enabled;}\nbool occupation_fiscal_enabled(const PreparedCatalog& c)')
change(p,'if(family_bequest_enabled(catalog))domains.push_back({"family_bequest",1});return domains;','if(family_bequest_enabled(catalog))domains.push_back({"family_bequest",1});if(treaty_passage_enabled(catalog))domains.push_back({"treaty_passage",1});return domains;')
change(p,'{"political_occupation_fiscal",{"1"}},','{"treaty_passage",{"1"}}, {"political_occupation_fiscal",{"1"}},')
change(p,'        if (has("family_bequest")) domains.push_back({"family_bequest",1});','        if (has("family_bequest")) domains.push_back({"family_bequest",1});\n        if(has("treaty_passage")){if(!has("politics")||!has("diplomacy")||!has("envoy_tasks"))fail("TreatyPassageProfileMismatch","checkpoint/module_versions");domains.push_back({"treaty_passage",1});}')
change(p,'            const bool diplomatic=version("diplomacy")!=0;','            const bool treaty=version("treaty_passage")!=0;TreatyPassageState treaty_state;auto treaty_layout=validate_treaty_passage_layout(treaty,versions,treaty?&treaty_state:nullptr);if(!treaty_layout.ok||(treaty!=has("treaty_passage","1")))return {false,{},common_failure("TreatyPassageProfileMismatch").error};\n            const bool diplomatic=version("diplomacy")!=0;')
anchor='result->envoy_tasks_enabled=true;}'
change(p,anchor,anchor+'\n        if(const auto* config=dynamic->find("treaty_passage")){if(!result->diplomacy||!result->politics)fail("TreatyPassageDependenciesRequired","dynamic_world/treaty_passage");const auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");if(std::find(caps.begin(),caps.end(),"treaty_passage_v1")==caps.end())fail("TreatyPassageCapabilityRequired","dynamic_world/treaty_passage");auto decoded=decode_treaty_passage_catalog(*config);if(!decoded.ok)return {false,{},decoded.error};if(!decoded.value.enabled)fail("TreatyPassageProfileRequired","dynamic_world/treaty_passage");auto valid=validate_treaty_passage_catalog(*result->common,*result->organization,*result->politics,*result->diplomacy,decoded.value);if(!valid.ok)return {false,{},valid.error};result->treaty_passage=std::make_shared<const TreatyPassageCatalog>(std::move(decoded.value));}')
change(p,'    if(occupation_fiscal_enabled(*result)){','    if(treaty_passage_enabled(*result))id.module_versions.emplace_back("treaty_passage","1");\n    if(occupation_fiscal_enabled(*result)){')
change(p,'    if(occupation_fiscal_enabled(catalog)){auto political=','    if(treaty_passage_enabled(catalog)){auto decoded=decode_treaty_passage_state(field(source,"treaty_passage","state"));if(!decoded.ok)return {false,{},decoded.error};result.treaty_passage=std::make_shared<const TreatyPassageState>(std::move(decoded.value));}else if(source.find("treaty_passage"))fail("UnexpectedTreatyPassageState","state");\n    if(occupation_fiscal_enabled(catalog)){auto political=')
change(p,'    if(w.diplomacy_enabled())set(output,"diplomacy",encode_diplomacy_state(w.diplomacy));','    if(w.diplomacy_enabled())set(output,"diplomacy",encode_diplomacy_state(w.diplomacy));\n    if(w.treaty_passage_enabled())set(output,"treaty_passage",encode_treaty_passage_state(w.treaty_passage));')
change(p,'CodecVisitor writer(s.payload);','CodecVisitor writer(s.payload);if(w.treaty_passage_enabled())writer.schema_features|=treaty_passage_checkpoint_feature;')
change(p,'    return encode_checkpoint(envelope);','    if(w.treaty_passage_enabled()){r=section("treaty_passage",[&](auto& c){return visit_treaty_passage_checkpoint(c,w.treaty_passage);});if(!r.ok)return {false,{},r.error};}\n    return encode_checkpoint(envelope);')
change(p,'std::uint32_t political_features=0;','std::uint32_t political_features=0;if(std::find(decoded.value.identity.catalog.module_versions.begin(),decoded.value.identity.catalog.module_versions.end(),std::make_pair(StableId{"treaty_passage"},StableId{"1"}))!=decoded.value.identity.catalog.module_versions.end())political_features|=treaty_passage_checkpoint_feature;')
change(p,'for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);','for(const auto& section:decoded.value.sections){CodecVisitor reader(section.payload);if(treaty_passage_enabled(catalog))reader.schema_features|=treaty_passage_checkpoint_feature;')
change(p,'        else if(section.domain=="diplomacy")','        else if(section.domain=="treaty_passage"){auto treaty=std::make_shared<TreatyPassageState>();r=visit_treaty_passage_checkpoint(reader,*treaty);seed.treaty_passage=std::move(treaty);}\n        else if(section.domain=="diplomacy")')
change(p,'||header->domain=="politics"||','||(header->domain=="politics"&&((header->kind!=treaty_passage_delivery_kind&&header->kind!=treaty_passage_signature_kind)||(w.treaty_passage_enabled()&&((header->kind==treaty_passage_delivery_kind&&w.treaty_passage.deliveries.find(row.first))||(header->kind==treaty_passage_signature_kind&&w.treaty_passage.signatures.find(row.first))))))||')
p=K+'native_initial_state.cpp'
change(p,'    if(dynamic.find("political_occupation_fiscal"))','    if(dynamic.find("treaty_passage"))admitted.module_versions.emplace_back("treaty_passage","1");\n    if(dynamic.find("political_occupation_fiscal"))')
change(p,'    if(mutable_catalog->occupation_fiscal&&','    if(mutable_catalog->treaty_passage&&mutable_catalog->treaty_passage->enabled)seed.treaty_passage=std::make_shared<const TreatyPassageState>();\n    if(mutable_catalog->occupation_fiscal&&')
p='native/world/src/resident/storage/common_codec.cpp'
change(p,'kind=="political_occupation_fiscal_policy_changed"||','kind=="treaty_passage_document_presented"||kind=="treaty_passage_signed"||kind=="political_occupation_fiscal_policy_changed"||')
# 只追加新的条件编号；原字段和编号完全不变，terms引用复用既有offer_id槽。
p=D+'conditions/condition_model.hpp'
change(p,'any,offer_declined_before };','any,offer_declined_before,treaty_personal_passage };')
change(p,'    std::function<Outcome<bool>(const StableId&,const StableId&)> offer_declined_before;','    std::function<Outcome<bool>(const StableId&,const StableId&)> offer_declined_before;\n    std::function<Outcome<bool>(const StableId&,const StableId&,const StableId&)> treaty_personal_passage;')
p=D+'conditions/condition_rules.cpp'
change(p,'    case Condition::Kind::relation_min:','    case Condition::Kind::treaty_personal_passage:if(!a.treaty_personal_passage)return unsupported("treaty_personal_passage");return a.treaty_personal_passage(actor,q.place_id,q.offer_id);\n    case Condition::Kind::relation_min:')
p=D+'interaction/interaction_codec.cpp'
change(p,'else if(op=="relation_min"){','else if(op=="treaty_personal_passage"){if(conversation)boundary::fail("TreatyPassageRequiresAdmission",path);boundary::fields(v,{"op","place_id","terms_id"},{},path);c.kind=Condition::Kind::treaty_personal_passage;c.place_id=boundary::get_string(v,"place_id",path);c.offer_id=boundary::get_string(v,"terms_id",path);}else if(op=="relation_min"){')
change(p,'if(kind<0||kind>7)return c.reject("UnsupportedCondition");','if(kind<0||kind>8||(kind==8&&!(c.schema_features&1024u)))return c.reject("UnsupportedCondition");')
p=K+'world_services.cpp'
change(p,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "treaty_passage_integration.hpp"')
change(p,'    access.relation_trust=','    access.treaty_personal_passage=[this](const StableId& actor,const StableId& place,const StableId& terms){return treaty_passage_world_admission(world_,actor,place,terms);};\n    access.relation_trust=')
# 新回执只有Done{}。二进制须有精确1024特征；禁用profile还会拒所有新历史。
p='native/world/src/resident/checkpoint/command_boundary.cpp'
change(p,'    if(diplomacy_command_kind(kind)){if((kind=="envoy_deliver"','    if(treaty_passage_command_kind(kind)){boundary::fields(value,{},{},"receipt/details");result.data=Done{};return result;}\n    if(diplomacy_command_kind(kind)){if((kind=="envoy_deliver"')
change(p,'if(kind==political_takeover_command_kind&&tag!=0)','if(treaty_passage_command_kind(kind)&&(!(c.schema_features&treaty_passage_checkpoint_feature)||tag!=0))return c.reject("InvalidTreatyPassageReceipt");if(kind==political_takeover_command_kind&&tag!=0)')
change(p,'        if(r.kind==political_takeover_command_kind','        if(treaty_passage_command_kind(r.kind)&&(!r.details||!std::holds_alternative<Done>(r.details->data)))failed("InvalidTreatyPassageReceipt","receipt/details",ErrorKind::rule);\n        if(r.kind==political_takeover_command_kind')
change(p,'    bool has_details=static_cast<bool>(value.details);','    if(treaty_passage_command_kind(value.kind)&&!(c.schema_features&treaty_passage_checkpoint_feature))return c.reject("TreatyPassageProfileRequired");\n    bool has_details=static_cast<bool>(value.details);')
change(p,'c.field("has_details",has_details);','c.field("has_details",has_details);if(treaty_passage_command_kind(value.kind)&&!has_details)return c.reject("MissingTreatyPassageReceipt");')
change(p,'    if(diplomacy_command_kind(kind)){auto decoded=','    if(treaty_passage_command_kind(kind)){auto decoded=decode_treaty_passage_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}\n    if(diplomacy_command_kind(kind)){auto decoded=')
change(p,'                else if constexpr(std::is_same_v<T,DiplomacyQuote>)','                else if constexpr(std::is_same_v<T,TreatyPassageQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{quote.duration}},{"treaty_passage",encode_treaty_passage_quote(quote)}});\n                else if constexpr(std::is_same_v<T,DiplomacyQuote>)')
# Director独立模板，仅显式新模块允许；旧模板编号和序列字节不变。
p=D+'director/director_model.hpp'
change(p,'diplomatic_response, diplomatic_delegation };','diplomatic_response, diplomatic_delegation, treaty_ratification };')
change(p,'bool diplomatic_response=false,diplomatic_delegation=false;','bool diplomatic_response=false,diplomatic_delegation=false,treaty_passage=false;')
change(p,'bool diplomacy,Integer diplomacy_version=1);','bool diplomacy,Integer diplomacy_version=1);\nOutcome<DirectorCatalog> decode_director_catalog(const BoundaryValue&,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage);')
p=D+'director/director_codec.cpp'
change(p,'"diplomatic_response","diplomatic_delegation"};','"diplomatic_response","diplomatic_delegation","treaty_ratification"};')
change(p,'for(Integer n=0;n<9;++n)','for(Integer n=0;n<10;++n)')
change(p,'bool diplomacy,Integer diplomacy_version){try{','bool diplomacy,Integer diplomacy_version){return decode_director_catalog(value,regional_life,named_membership,politics,diplomacy,diplomacy_version,false);}\nOutcome<DirectorCatalog> decode_director_catalog(const BoundaryValue& value,bool regional_life,bool named_membership,bool politics,bool diplomacy,Integer diplomacy_version,bool treaty_passage){try{')
change(p,'politics,diplomacy,diplomacy_version);if(!parsed.ok)','politics,diplomacy,diplomacy_version,treaty_passage);if(!parsed.ok)')
change(p,'DirectorCatalog c;c.diplomatic_delegation=','DirectorCatalog c;c.treaty_passage=treaty_passage;c.diplomatic_delegation=')
change(p,'if(t==DirectorTemplate::diplomatic_delegation&&!c.diplomatic_delegation)','if(t==DirectorTemplate::treaty_ratification&&!c.treaty_passage)fail("TreatyDirectorProfileRequired",row.first);if(t==DirectorTemplate::diplomatic_delegation&&!c.diplomatic_delegation)')
p=D+'director/director_validation.cpp'
change(p,'(catalog.diplomatic_delegation?8:','(catalog.treaty_passage?9:catalog.diplomatic_delegation?8:')
change(p,'&&!(catalog.diplomatic_delegation&&p.template_id=="diplomatic_delegation")','&&!(catalog.diplomatic_delegation&&p.template_id=="diplomatic_delegation")&&!(catalog.treaty_passage&&p.template_id=="treaty_ratification")')
p=D+'politics/diplomacy/diplomacy_response_director.hpp'
change(p,'struct DirectorDiplomaticContext {','struct TreatyPassageContext;\nstruct DirectorDiplomaticContext {')
change(p,'    const ActivityRead& activity;','    const ActivityRead& activity;\n    const TreatyPassageContext* treaty=nullptr;')
p=D+'director/director_rules.cpp'
change(p,'#include "director_model.hpp"','#include "director_model.hpp"\n#include "resident/domains/politics/treaty/treaty_passage_director.hpp"')
change(p,'        if(catalog.diplomatic_response&&policy->diplomatic_response','        if(catalog.treaty_passage&&enabled(*policy,DirectorTemplate::treaty_ratification)&&!(registration&&registration->obligations_only)){if(!diplomacy||!diplomacy->treaty)return {false,{},common_failure("TreatyDirectorAccessMissing",view.actor_id,ErrorKind::internal).error};auto proposed=propose_director_treaty_passage(*diplomacy->treaty,view,*policy,p);if(!proposed.ok)return {false,{},proposed.error};if(proposed.value&&proposed.value->command)return {true,{std::move(*proposed.value)},{}};}\n        if(catalog.diplomatic_response&&policy->diplomatic_response')
p=K+'dynamic_integration.cpp'
change(p,'#include "occupation_fiscal_integration.hpp"','#include "occupation_fiscal_integration.hpp"\n#include "treaty_passage_integration.hpp"\n#include "resident/domains/politics/treaty/treaty_passage_director.hpp"')
change(p,'        DirectorDiplomaticContext context{present,*w.catalog->common,w.services};','        DirectorDiplomaticContext context{present,*w.catalog->common,w.services};\n        if(w.treaty_passage_enabled()&&std::find(policy.templates.begin(),policy.templates.end(),DirectorTemplate::treaty_ratification)!=policy.templates.end()){auto treaty=TreatyPassageWorldAccess::create(w);if(!treaty.ok)return {false,false,treaty.error};auto candidates=propose_treaty_passage_npc(treaty.value->context(),actor);if(!candidates.ok)return {false,false,candidates.error};for(const auto& command:candidates.value)if(command.authority.organization_id==policy.organization_id)return {true,true,{}};}')
change(p,'    DiplomacyState diplomacy;PoliticalTakeoverState','    TreatyPassageState treaty_passage;\n    DiplomacyState diplomacy;PoliticalTakeoverState')
change(p,'diplomacy(w.diplomacy),','diplomacy(w.diplomacy),treaty_passage(w.treaty_passage),')
change(p,'void restore(WorldBundle& w){','void restore(WorldBundle& w){w.treaty_passage=std::move(treaty_passage);')
change(p,'w.catalog->director->diplomatic_response||w.catalog->director->diplomatic_delegation','w.catalog->director->diplomatic_response||w.catalog->director->diplomatic_delegation||w.catalog->director->treaty_passage')
change(p,'        auto growth_access=w.services.growth_access();auto proposed=','        std::unique_ptr<TreatyPassageWorldAccess> treaty;if(w.treaty_passage_enabled()){auto access=TreatyPassageWorldAccess::create(w);if(!access.ok)return {false,{},access.error};treaty=std::move(access.value);if(diplomatic)diplomatic->treaty=&treaty->context();}\n        auto growth_access=w.services.growth_access();auto proposed=')
p=K+'world_codec.cpp'
change(p,'result->diplomacy?result->diplomacy->version:1);if(!director.ok)','result->diplomacy?result->diplomacy->version:1,treaty_passage_enabled(*result));if(!director.ok)')
# 新条件仅允许真实登记的同一admission场所；没有此模块不能保存未执行过的宽松条件。
change(p,'    if(treaty_passage_enabled(*result))id.module_versions.emplace_back','    {std::function<void(const Condition&,const StableId&)> verify=[&](const Condition& condition,const StableId& place){if(condition.kind==Condition::Kind::treaty_personal_passage){const auto* provision=treaty_passage_enabled(*result)?result->treaty_passage->provisions.find(condition.offer_id):nullptr;if(!provision||condition.place_id!=place||std::none_of(provision->places.begin(),provision->places.end(),[&](const auto& p){return p.place_id==place;}))fail("InvalidTreatyPassageAdmission",place);}for(const auto& child:condition.terms)verify(child,place);};for(const auto& admission:result->interaction->admissions)for(const auto& condition:admission.conditions)verify(condition,admission.id);}\n    if(treaty_passage_enabled(*result))id.module_versions.emplace_back')
p=K+'world_bundle.cpp'
change(p,'#include "treaty_passage_integration.hpp"','#include "treaty_passage_integration.hpp"\n#include "resident/domains/politics/treaty/treaty_passage_director.hpp"')
change(p,'        valid=validate_diplomatic_director_catalog','        valid=validate_treaty_passage_director(*catalog->director,director,catalog->treaty_passage.get(),treaty_passage_enabled()?&treaty_passage:nullptr,*catalog->organization);if(!valid.ok)return valid;\n        valid=validate_diplomatic_director_catalog')

p='game/ui/native_resident_backend.gd'
change(p,'func political_occupation_fiscal_enabled() -> bool:','''## 双签通行只有显式独立配置，旧外交接受不自动升级。
func treaty_passage_enabled() -> bool:
	var config: Dictionary=_catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).get("treaty_passage",{})
	return _installed and config.get("enabled",false) and config.get("version",0)==1

func political_occupation_fiscal_enabled() -> bool:''')
change(p,'\tif definition.get("dynamic_world",{}).has("political_occupation_fiscal"):','\tif definition.get("dynamic_world",{}).has("treaty_passage"):\n\t\texpected["module_versions"].append({"name":"treaty_passage","version":"1"})\n\tif definition.get("dynamic_world",{}).has("political_occupation_fiscal"):')
p='game/ui/main.gd'
change(p,'\tenabled=enabled or (kernel.has_method("political_occupation_fiscal_enabled")','\tenabled=enabled or (kernel.has_method("treaty_passage_enabled") and kernel.treaty_passage_enabled() and action.get("command","") in ["treaty_passage_present","treaty_passage_sign"])\n\tenabled=enabled or (kernel.has_method("political_occupation_fiscal_enabled")')
p='tools/content/links.py'
change(p,'    from .political_occupation_fiscal_links import check_political_occupation_fiscal','    from .political_occupation_fiscal_links import check_political_occupation_fiscal\n    from .treaty_passage_links import check_treaty_passage')
change(p,' + check_political_occupation_fiscal(pack,index)',' + check_political_occupation_fiscal(pack,index) + check_treaty_passage(pack,index)')
p='tools/content/dynamic_world_links.py'
change(p,"                allowed.add('political_set_occupation_tax')","                allowed.add('political_set_occupation_tax')\n            if dynamic.get('treaty_passage', {}).get('enabled'):\n                allowed.add('diplomatic_ratify_passage')")
p='schemas/content.schema.json';data=json.loads(get(p));data['properties']['required_capabilities']['items']['enum'].append('treaty_passage_v1')
ref={'type':'string','minLength':1};obj=lambda fields:{'type':'object','properties':fields,'required':list(fields),'additionalProperties':False}
data['$defs']['dynamic_world']['properties']['treaty_passage']=obj({'enabled':{'type':'boolean','enum':[True]},'version':{'type':'integer','enum':[1]},'provisions':{'type':'object','minProperties':1,'maxProperties':256,'additionalProperties':obj({'id':ref,'terms_id':ref,'places':{'type':'array','minItems':1,'maxItems':256,'items':obj({'place_id':{'$ref':'#/$defs/reference_places'},'jurisdiction_id':ref})},'npc_ratifier_office_ids':{'type':'array','items':ref,'uniqueItems':True,'maxItems':16}})}})
data['$defs']['condition']['oneOf'].append(obj({'op':{'const':'treaty_personal_passage'},'place_id':{'$ref':'#/$defs/reference_places'},'terms_id':ref}))
data['$defs']['dynamic_director_subject']['properties']['templates']['items']['enum'].append('treaty_ratification')
F[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
p='game/data/rule_capabilities.json';data=json.loads(get(p));data['capabilities'].append({'capability_id':'treaty_passage_v1','rule_version':'treaty_passage_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.treaty_passage.enabled','equals':True}],'supported_semantics':[],'test_ids':['test_diplomacy_player_flow'],'evidence':[]});F[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'

# 原registry的当面例外须认识新显式模板，仍守成年/登记/同半日预算。
p=D+'director/director_registry.cpp'
change(p,'return kind==DirectorTemplate::private_livelihood||','return kind==DirectorTemplate::treaty_ratification||kind==DirectorTemplate::private_livelihood||')
change(p,'(catalog.diplomatic_response||catalog.diplomatic_delegation)&&response','(catalog.diplomatic_response||catalog.diplomatic_delegation||catalog.treaty_passage)&&response')
change(p,'if(policy&&(policy->diplomatic_response||policy->diplomatic_delegation_policy_id))','if(policy&&(policy->diplomatic_response||policy->diplomatic_delegation_policy_id||std::find(policy->templates.begin(),policy->templates.end(),DirectorTemplate::treaty_ratification)!=policy->templates.end()))')
# 写精确差异，旧基准与候选摘要都可逐叶复核。
manifest={'base':B,'status':'SOURCE_ONLY_IN_PROGRESS','files':[]};patch=[]
for p,s in F.items():
 dest=O/'candidate'/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(s)
 manifest['files'].append({'path':p,'base_sha256':hashlib.sha256(OLD[p].encode()).hexdigest() if OLD[p] else None,'sha256':hashlib.sha256(s.encode()).hexdigest()})
 patch.extend(difflib.unified_diff(OLD[p].splitlines(True),s.splitlines(True),fromfile='a/'+p if OLD[p] else '/dev/null',tofile='b/'+p))
(O/'registration.patch').write_text(''.join(patch));(O/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');print(len(F),O)
