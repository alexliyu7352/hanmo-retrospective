// 原控制头有独立实际提交对应，再按真实头前本人交叉Shop组；不信自存allocation选择控制前界。
#include "private_shop_command_result_player.hpp"
#include "private_shop_command_result_sources.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
#include "resident/domains/family/family_codec.hpp"
#include "resident/domains/family/disciple_control_receipts.hpp"
#include "resident/domains/family/biological_control_receipts.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
Outcome<std::vector<PrivateShopCommandFactSource>> read_private_shop_command_control_sources(const CommonStore& c,const FamilyState* family,const FamilyLegacyState* legacy){
 auto fail=[&](const StableId& code,const StableId& id=StableId{})->Outcome<std::vector<PrivateShopCommandFactSource>>{return {false,{},common_failure(code,id).error};};
 try{std::map<StableId,PrivateShopCommandFactSource> rows;std::set<Integer> allocated;
 auto add=[&](const StableId& id,const StableId& prefix,const StableId& kind,const StableId& domain,Integer tick,BoundaryValue value)->Status{auto n=organization_instance_sequence(id,prefix);const auto* head=c.state().facts.find(id);if(!n||*n>=c.state().next_instance_id||!allocated.insert(*n).second||!head||head->id!=id||head->kind!=kind||head->domain!=domain||tick<0||tick>c.tick()||!rows.emplace(id,PrivateShopCommandFactSource{id,kind,tick,std::move(value)}).second)return common_failure("PrivateShopCommandControlSourceMismatch",id);return common_success();};
 if(family){for(const auto& id:family->successions){auto step=family_succession_step(*family,id);const auto* fact=family->facts.find(id);if(!step.ok||!fact)return fail("PrivateShopCommandControlSourceMismatch",id);BoundaryValue payload;StableId kind;
  if(const auto* old=std::get_if<FamilyChoice>(fact)){kind=old->kind;if(kind!="family_succession")return fail("PrivateShopCommandControlSourceMismatch",id);payload=family_codec::encode_choice(*old,true);}
  else if(const auto* next=std::get_if<VoluntarySuccessionFact>(fact)){kind=next->kind;const auto* accepted=family->facts.find(next->consent_fact_id);const auto* consent=accepted?std::get_if<SuccessionConsentFact>(accepted):nullptr;if(kind!="family_voluntary_succession"||!consent)return fail("PrivateShopCommandControlSourceMismatch",id);payload=boundary::obj({{"control",encode_voluntary_succession_fact(*next)},{"acceptance",encode_succession_consent_fact(*consent)}});}
  else return fail("PrivateShopCommandControlSourceMismatch",id);auto valid=add(id,"family",kind,"family",step.value.tick,std::move(payload));if(!valid.ok)return {false,{},valid.error};
 }for(const auto& row:family->facts.rows()){const auto* old=std::get_if<FamilyChoice>(&row.second);if((old&&old->kind=="family_succession")||std::holds_alternative<VoluntarySuccessionFact>(row.second))if(!rows.count(row.first))return fail("PrivateShopCommandControlCoverageMismatch",row.first);}}
 if(legacy)for(const auto& row:legacy->activations.rows()){const auto* consent=legacy->consents.find(row.second.consent_fact_id);if(!consent||row.second.kind!="family_legacy_control_activated")return fail("PrivateShopCommandControlSourceMismatch",row.first);FamilyLegacyState original;original.consents.insert(row.second.consent_fact_id,*consent);original.activations.insert(row.first,row.second);auto valid=add(row.first,"family_legacy",row.second.kind,"family_legacy",row.second.tick,(consent->source.biological_origin||consent->source.born_dialogue||std::find(consent->source.field_order.begin(),consent->source.field_order.end(),"biological_origin")!=consent->source.field_order.end())?encode_family_legacy_v2(original):encode_family_legacy(original));if(!valid.ok)return {false,{},valid.error};}
 std::vector<PrivateShopCommandFactSource> out;Integer previous=0;for(const auto& head:c.state().facts.rows()){const auto& kind=head.second.kind;if(kind!="family_succession"&&kind!="family_voluntary_succession"&&kind!="family_legacy_control_activated")continue;const auto found=rows.find(head.first);const auto n=organization_instance_sequence(head.first,kind=="family_legacy_control_activated"?"family_legacy":"family");if(found==rows.end()||!n||*n<=previous)return fail("PrivateShopCommandControlCoverageMismatch",head.first);previous=*n;out.push_back(found->second);}if(out.size()!=rows.size())return fail("PrivateShopCommandControlCoverageMismatch");return {true,std::move(out),{}};
 }catch(const boundary::DecodeFailure& e){return {false,{},e.error};}
}
Status validate_private_shop_command_result_player_history(const CommonStore& c,const PrivateShopState& shops,const PrivateShopCommandResultsState* state,const FamilyState& family,const FamilyLegacyState* legacy,PrivateShopSubmissionAuthority authority,ProjectionWriter* projection){
 auto valid=validate_private_shop_command_results(c,shops,state,&family,legacy,projection);if(!valid.ok||!state)return valid;if(authority!=PrivateShopSubmissionAuthority::legacy_actor_api&&authority!=PrivateShopSubmissionAuthority::controlled_player)return common_failure("PrivateShopSubmissionAuthorityInvalid");auto sources=read_private_shop_command_fact_sources(c,shops,projection);if(!sources.ok)return {false,{},sources.error};auto controls=read_private_shop_command_control_sources(c,&family,legacy);if(!controls.ok)return {false,{},controls.error};const auto& order=c.state().facts.order();
 struct Source{const PrivateShopCommandFactSource* source;const PrivateShopCommandResultBinding* binding;Integer number;std::size_t position;};std::map<Integer,Source> all;std::map<StableId,const Source*> groups;
 auto insert=[&](const auto& values,const auto& bindings)->Status{for(const auto& s:values){auto at=s.id.rfind(':');const auto n=*organization_instance_sequence(s.id,s.id.substr(0,at));const auto pos=std::find(order.begin(),order.end(),s.id);if(!all.emplace(n,Source{&s,bindings.find(s.id),n,static_cast<std::size_t>(pos-order.begin())}).second)return common_failure("PrivateShopCommandControlSourceMismatch",s.id);}return common_success();};valid=insert(sources.value,state->bindings);if(!valid.ok)return valid;valid=insert(controls.value,state->control_bindings);if(!valid.ok)return valid;
 std::optional<std::size_t> previous;for(const auto& row:all){if(previous&&row.second.position<=*previous)return common_failure("PrivateShopCommandControlOrderMismatch",row.second.source->id);previous=row.second.position;groups.emplace(row.second.binding->result_id,&row.second);}
 for(const auto& group:groups){const auto& first=*group.second;const auto& b=*first.binding;const auto result=*organization_instance_sequence(b.result_id,"result");
  for(const auto& control:controls.value){const auto* bound=state->control_bindings.find(control.id);const auto number=*organization_instance_sequence(control.id,control.kind=="family_legacy_control_activated"?"family_legacy":"family");if((number<first.number&&control.tick>b.from_tick)||(number>result&&control.tick<b.result_tick))return common_failure("PrivateShopCommandControlOrderMismatch",control.id);if(number>=first.number&&number<result&&bound->result_id!=b.result_id)return common_failure("PrivateShopCommandControlCrossedSubmit",control.id);}
  // 本组首个原事实可能就是控制头；其前界独立证明提交者，不由begin字段选择/跳过。
  // 旧多Actor API允许合法他人驱动该半日；这类Shop组不能冒称受控player提交。
  // 控制组本身无论原入口模式仍只准真实头前本人，direct业务原actor门始终由结构全验保留。
  if(authority==PrivateShopSubmissionAuthority::legacy_actor_api&&!state->control_bindings.find(first.source->id))continue;
  auto actor=legacy?family_player_before_fact_with_legacy(c,family,*legacy,b.from_tick,first.source->id):family_player_before_fact(c,family,b.from_tick,first.source->id);if(!actor.ok)return {false,{},actor.error};if(actor.value!=b.submitter_actor_id)return common_failure("PrivateShopHistoricalSubmitterMismatch",first.source->id);
 }
 return common_success();
}
Status validate_private_shop_command_control_anchors(const PrivateShopCommandResultsState* state,const FamilyLegacyState& legacy,const DiscipleControlState* disciple,const BiologicalControlState* biological){
 if(!state)return common_success();
 auto cross=[&](const auto& commits)->Status{for(const auto& row:commits.rows())if(legacy.activations.find(row.first)){const auto* bound=state->control_bindings.find(row.first);const auto& actual=row.second;if(!bound||bound->result_id!=actual.result_id||bound->submitter_actor_id!=actual.actor_id||bound->result_kind!=actual.kind||bound->request_seq!=actual.request_seq||bound->revision!=actual.revision)return common_failure("PrivateShopCommandIndependentControlResultMismatch",row.first);}return common_success();};
 if(disciple){auto valid=cross(disciple->commits);if(!valid.ok)return valid;}if(biological){auto valid=cross(biological->commits);if(!valid.ok)return valid;}return common_success();
}

}
