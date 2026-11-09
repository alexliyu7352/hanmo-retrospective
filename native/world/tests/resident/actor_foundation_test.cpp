// 首片只证明同一Common的真实作者初态、用途读取和来源闭包；不冒完整人生验收。
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "resident/domains/actor_foundation/rest_condition.hpp"
#include "resident/domains/actor_foundation/actor_foundation_codec.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// 每个断言对应真实值或拒绝；失败即停止并保留计数。
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
// 输入来自受检锁定初态定义；当前人物以后变化不会改变此源。
ActorFoundationCatalog catalog(){
 ActorFoundationCatalog c;c.identity.content_digest="locked";c.scenario_id="scenario";
 c.construction_profile={"selected_existing_actor_context_v2",2,"locked"};
 c.provenance_policy={"character_origin_read_v2",2,"locked"};
 c.rest_rule={"original_service_rest",1,"locked"};
 c.rest_purpose={"rest",1,"locked"};c.provider_purpose={"provide_rest",1,"locked"};
 for(const auto& id:{StableId{"guest"},StableId{"host"}}){AuthoredActorFoundationDefinition d;
  d.actor_id=id;d.identity_definition_ref={id,1,"locked"};d.appearance_source_ref={id,1,"locked"};
  d.scenario_origin_ref={"locked","scenario",id,0};d.age_basis.years=27;d.age_basis.source=d.scenario_origin_ref;
  d.condition.seed_key=id+":initial-condition";d.condition.actor_id=id;d.condition.source=d.scenario_origin_ref;
  d.condition.hp=id=="guest"?57:100;d.condition.fatigue=id=="guest"?42:0;d.condition.injury=id=="guest"?1:0;d.condition.activity_limit_profile=c.rest_rule;
  d.placement.place_ref="inn";d.attributes={50,51,52,53,54};c.actors.insert(id,d);
 }
 return c;
}
// 只为冷decoder生成完整作者输入；没有当前Common参数或可回灌live状态。
std::pair<BoundaryValue,BoundaryValue> authored_inputs(const ActorFoundationCatalog& c){
 using V=BoundaryValue;using boundary::obj;
 auto ref=[](const ActorFoundationRuleRef& r){return boundary::obj({{"id",V{r.id}},{"version",V{r.version}},{"catalog_digest",V{r.catalog_digest}}});};
 auto src=[](const ScenarioOriginRef& r){return boundary::obj({{"catalog_digest",V{r.catalog_digest}},{"scenario_id",V{r.scenario_id}},{"row_key",V{r.row_key}},{"initial_tick",V{r.initial_tick}}});};
 V::Array seeds,actors,original;
 for(const auto& row:c.actors.rows()){const auto& d=row.second;
  seeds.push_back(obj({{"seed_key",V{d.condition.seed_key}},{"actor_id",V{d.actor_id}},{"source",src(d.scenario_origin_ref)},{"hp",V{d.condition.hp}},{"fatigue",V{d.condition.fatigue}},{"injury",V{d.condition.injury}},{"episodes",V{V::Array{}}},{"activity_limit_profile",ref(c.rest_rule)}}));
  auto age=obj({{"kind",V{StableId{"DeclaredAgeAtStart"}}},{"years",V{d.age_basis.years}},{"as_of_tick",V{Integer{0}}},{"calendar_year_ordinal",V{Integer{0}}},{"source",src(d.scenario_origin_ref)}});
  actors.push_back(obj({{"actor_id",V{d.actor_id}},{"identity_definition_ref",ref(d.identity_definition_ref)},{"alive",V{d.alive}},{"hp",V{d.condition.hp}},{"fatigue",V{d.condition.fatigue}},{"injury",V{d.condition.injury}},{"age_basis",age},{"condition_seed_refs",V{V::Array{obj({{"seed_key",V{d.condition.seed_key}}})}}},{"placement",obj({{"kind",V{StableId{"AtPlace"}}},{"place_ref",V{d.placement.place_ref}}})},{"service_seed_refs",V{V::Array{}}},{"office_seed_refs",V{V::Array{}}},{"property_business_seed_refs",V{V::Array{}}},{"initial_knowledge_refs",V{V::Array{}}},{"appearance_source_ref",ref(d.appearance_source_ref)}}));
  V::Dict attrs;for(auto item:{std::pair<const char*,Integer>{"administration",d.attributes.administration},{"charisma",d.attributes.charisma},{"intellect",d.attributes.intellect},{"leadership",d.attributes.leadership},{"martial",d.attributes.martial}})attrs.emplace_back(item.first,V{item.second});
  V::Dict skills;for(const auto* key:{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport"})skills.emplace_back(key,V{Integer{0}});
  original.push_back(obj({{"actor_id",V{d.actor_id}},{"available",V{d.available}},{"place_id",V{d.placement.place_ref}},{"attributes",V{attrs}},{"skills",V{skills}}}));
 }
 return {obj({{"construction_profile_ref",ref(c.construction_profile)},{"provenance_policy_ref",ref(c.provenance_policy)},{"rest_rule_ref",ref(c.rest_rule)},{"rest_purpose_ref",ref(c.rest_purpose)},{"provider_purpose_ref",ref(c.provider_purpose)},{"initial_condition_seeds",V{seeds}},{"actor_states",V{actors}}}),obj({{"actors",V{original}}})};
}
// 只服务本局部测试实际调用的原完成边界；不是生产time provider或由旧tick补精度。
struct ObservedOriginalCompletion final:AuthoredRestTimeRead {
 StableId action;WorldMoment actual_at;
 Outcome<WorldMoment> completed_rest_at(const StableId& id)const override {return id==action?Outcome<WorldMoment>{true,actual_at,{}}:Outcome<WorldMoment>{false,{},common_failure("MissingObservedCompletion").error};}
};
}
int main(){try{
 auto c=catalog();CommonState common;common.metadata.scenario_id="scenario";common.metadata.content_digest="locked";
 for(const auto& row:c.actors.rows()){ActorRecord a;a.id=row.first;a.alive=true;a.available=true;a.location=SettlementLocation{"town","inn"};
  a.attributes=row.second.attributes;a.skills=row.second.skills;auto s=initialize_authored_actor_foundation_actor(a,c,row.first);check(s.ok,"cold actor fields installed before Common insertion");common.actors.insert(a.id,a);}
 auto seeded=make_authored_actor_foundation_seed(common,c);check(seeded.ok,"real authored origin seed accepted");auto state=seeded.value;
 check(!common.actors.find("guest")->birth_tick&&common.actors.find("guest")->hp==57,"declared age does not invent birthday or heal");
 auto origin=read_authored_character_origin_v2(common,state,c,"guest");check(origin.ok&&origin.value.initial_condition_seed_ref.seed_key=="guest:initial-condition","origin resolves actual immutable seed");
 auto f=capture_authored_rest_capability(common,state,c,"guest",c.rest_purpose,WorldMoment{});check(f.ok&&f.value.body_scalars.hp==57&&f.value.result.allowed&&f.value.active_condition_inputs.empty(),"complete genuine rest capability capture");
 auto wrong=c.rest_purpose;wrong.id="unknown";check(!capture_authored_rest_capability(common,state,c,"guest",wrong,WorldMoment{}).ok,"unknown purpose is unsupported");
 common.actors.find("guest")->hp=99;check(!capture_authored_rest_capability(common,state,c,"guest",c.rest_purpose,WorldMoment{}).ok,"body without a condition source is rejected");
 origin=read_authored_character_origin_v2(common,state,c,"guest");check(origin.ok,"identity reader does not rewrite or infer current body");common.actors.find("guest")->hp=57;
 auto bytes=encode_actor_foundation_checkpoint(state);check(bytes.ok,"all three roots encode");auto loaded=decode_actor_foundation_checkpoint(bytes.value);check(loaded.ok,"all three roots decode");
 check(validate_authored_actor_foundation(common,loaded.value,c).ok,"cold round trip validates actual source closure");
 auto bad=bytes.value;bad.push_back(0);check(!decode_actor_foundation_checkpoint(bad).ok,"trailing bytes refused");
 state.actor_start_context.initial_condition_seeds.find("guest:initial-condition")->hp=99;check(!validate_authored_actor_foundation(common,state,c).ok,"tampered seed cannot become source");state=seeded.value;
 auto unsupported=state;unsupported.character_condition_extensions.episodes.insert("unsupported",{});check(!encode_actor_foundation_checkpoint(unsupported).ok,"nonempty unimplemented episode rejected, never silently dropped");
 unsupported=state;unsupported.character_origin_records.origins.find("guest")->origin_kind=CharacterOriginRecordV2::Kind::created;check(!encode_actor_foundation_checkpoint(unsupported).ok,"unsupported origin cannot masquerade as authored");
 auto start_json=encode_actor_start_context_v2(state.actor_start_context);auto origin_json=encode_character_origin_records_v1(state.character_origin_records);auto condition_json=encode_character_condition_extensions_v1(state.character_condition_extensions);check(start_json.ok&&origin_json.ok&&condition_json.ok,"complete JSON roots encode");
 auto start_read=decode_actor_start_context_v2(start_json.value);auto origin_read=decode_character_origin_records_v1(origin_json.value);auto condition_read=decode_character_condition_extensions_v1(condition_json.value);check(start_read.ok&&origin_read.ok&&condition_read.ok,"complete JSON roots decode");
 AuthoredActorFoundationState json_state{start_read.value,origin_read.value,condition_read.value};check(encode_actor_foundation_checkpoint(json_state).value==bytes.value,"JSON and binary exact typed state agree");
 auto extra=start_json.value;extra.as<BoundaryValue::Dict>()->emplace_back("unknown",BoundaryValue{Integer{1}});check(!decode_actor_start_context_v2(extra).ok,"JSON unknown root rejected");extra=start_json.value;extra.as<BoundaryValue::Dict>()->push_back(extra.as<BoundaryValue::Dict>()->front());check(!decode_actor_start_context_v2(extra).ok,"JSON duplicate root rejected");extra=start_json.value;extra.as<BoundaryValue::Dict>()->pop_back();check(!decode_actor_start_context_v2(extra).ok,"JSON missing root rejected");
 auto capability_json=encode_frozen_rest_capability_inputs(f.value);auto capability_read=decode_frozen_rest_capability_inputs(capability_json.value);check(capability_json.ok&&capability_read.ok&&capability_read.value.field_order==f.value.field_order&&capability_read.value.body_scalars.hp==57,"all nine capability slots round trip");
 auto packed=state.character_origin_records;packed.origins.find("guest")->initial_capability_basis.catalog_identity.pack_set_digest="pack";auto packed_json=encode_character_origin_records_v1(packed);auto packed_read=decode_character_origin_records_v1(packed_json.value);check(packed_read.ok&&*packed_read.value.origins.find("guest")->initial_capability_basis.catalog_identity.pack_set_digest=="pack","optional catalog digest round trip");
 auto authored=authored_inputs(c);auto authored_read=decode_authored_actor_foundation_catalog(authored.first,authored.second,c.identity,c.scenario_id,{});check(authored_read.ok&&authored_read.value.actors.find("guest")->condition.hp==57,"real locked authored definition decoder");
 auto authored_bad=authored.first;authored_bad.as<BoundaryValue::Dict>()->emplace_back("unknown",BoundaryValue{false});check(!decode_authored_actor_foundation_catalog(authored_bad,authored.second,c.identity,c.scenario_id,{}).ok,"unknown authored fields rejected");
 authored_bad=authored.first;authored_bad.as<BoundaryValue::Dict>()->push_back(authored_bad.as<BoundaryValue::Dict>()->front());check(!decode_authored_actor_foundation_catalog(authored_bad,authored.second,c.identity,c.scenario_id,{}).ok,"duplicate authored field rejected");
 auto wrong_catalog=c;wrong_catalog.actors.find("guest")->identity_definition_ref.id="host";check(!make_authored_actor_foundation_seed(common,wrong_catalog).ok,"identity must reference same original actor definition");wrong_catalog=c;wrong_catalog.actors.find("guest")->appearance_source_ref.id="guest:invented";check(!make_authored_actor_foundation_seed(common,wrong_catalog).ok,"appearance ref must resolve original actor definition");
 wrong_catalog=c;wrong_catalog.actors.find("guest")->attributes.martial=101;check(!make_authored_actor_foundation_seed(common,wrong_catalog).ok,"typed catalog cannot bypass original attribute range");
 auto inactive=state;inactive.actor_start_context.actor_age_bases.find("guest")->age_basis.birth_tick=123;check(!encode_actor_foundation_checkpoint(inactive).ok&&!validate_authored_actor_foundation(common,inactive,c).ok,"inactive birth tick cannot vanish from source equality");
 inactive=state;inactive.actor_start_context.actor_age_bases.find("guest")->age_basis.birth_source_ref="forged";check(!encode_actor_start_context_v2(inactive.actor_start_context).ok&&!validate_authored_actor_foundation(common,inactive,c).ok,"inactive birth source cannot vanish from JSON or source equality");
 inactive=state;inactive.character_origin_records.origins.find("guest")->created_at.legacy_tick=3;check(!encode_actor_foundation_checkpoint(inactive).ok&&!encode_character_origin_records_v1(inactive.character_origin_records).ok&&!validate_authored_actor_foundation(common,inactive,c).ok,"inactive legacy moment cannot vanish from exact source equality");
 check(origin_json.value.find("registration_sources")->as<BoundaryValue::Dict>()&&origin_json.value.find("allocation_sources")->as<BoundaryValue::Dict>(),"unselected origin tables keep OrderedTable JSON object shape");
 bool condition_maps=true;for(const auto* key:{"episodes","effect_contributions","recovery_resolutions","capacity_transitions","combat_return_bindings","instant_limits"})condition_maps=condition_maps&&condition_json.value.find(key)->as<BoundaryValue::Dict>();check(condition_maps,"unselected condition tables keep OrderedTable object shape");
 auto wrong_table_shape=origin_json.value;for(auto& row:*wrong_table_shape.as<BoundaryValue::Dict>())if(row.first=="registration_sources")row.second=BoundaryValue{BoundaryValue::Array{}};check(!decode_character_origin_records_v1(wrong_table_shape).ok,"empty array cannot substitute original ordered table");
 auto nonempty_table=state.character_origin_records;nonempty_table.registration_sources.insert("unsupported",{});check(!encode_character_origin_records_v1(nonempty_table).ok,"nonempty unimplemented original table rejects");
 check(start_json.value.find("initial_condition_seeds")->find("guest:initial-condition")->find("episodes")->as<BoundaryValue::Array>()&&capability_json.value.find("active_condition_inputs")->as<BoundaryValue::Array>(),"real seed episode and active-input arrays retain array shape");
 check(origin_read.value.origins.find("guest")->age_basis_ref.kind==CharacterAgeBasisRef::Kind::actor_start&&origin_read.value.origins.find("guest")->age_basis_ref.actor_start.actor_id=="guest"&&loaded.value.character_origin_records.origins.find("guest")->age_basis_ref.kind==CharacterAgeBasisRef::Kind::actor_start,"Origin age reference union tag and payload round trip both codecs");
 for(Integer unsupported_tag:{Integer{1},Integer{99}}){auto wrong_age_ref=origin_json.value;*wrong_age_ref.find("origins")->find("guest")->find("age_basis_ref")->find("tag")=BoundaryValue{unsupported_tag};check(!decode_character_origin_records_v1(wrong_age_ref).ok,"known unimplemented or unknown age-ref tag rejected in JSON");auto wrong_age_state=state;wrong_age_state.character_origin_records.origins.find("guest")->age_basis_ref.kind=static_cast<CharacterAgeBasisRef::Kind>(unsupported_tag);check(!encode_actor_foundation_checkpoint(wrong_age_state).ok,"known unimplemented or unknown age-ref tag rejected in binary writer");}
 auto untagged_age=origin_json.value;auto* age_fields=untagged_age.find("origins")->find("guest")->find("age_basis_ref")->as<BoundaryValue::Dict>();age_fields->erase(age_fields->begin());check(!decode_character_origin_records_v1(untagged_age).ok,"untagged WIP age reference is not a compatibility profile");
 auto shared=std::make_shared<CommonCatalog>();shared->actor_ids={"guest","host"};shared->places.insert("inn",{"inn","town",{}});shared->roles.insert("innkeeper",{"innkeeper","host","inn",{},{},{"rest"},6});
 for(auto& row:common.actors.rows()){row.second.wallet_id=row.first+":wallet";WalletView w;w.id=row.second.wallet_id;w.owner={Owner::Kind::actor,row.first,{},{}};w.balance=row.first=="guest"?100:0;common.wallets.insert(w.id,w);}
 CommonStore store(common,shared);GrowthCatalog growth_catalog;growth_catalog.regional_life=true;ServiceTerms terms;terms.duration=1;terms.cost=5;terms.hp_gain=12;terms.fatigue_delta=-20;terms.wallet_id="host:wallet";GrowthRole role;role.activities.insert("rest",terms);growth_catalog.roles.insert("innkeeper",role);c.original_growth=std::make_shared<GrowthCatalog>(growth_catalog);GrowthState growth;SocialState social;InteractionState interaction;
 auto action_id=store.next_id("action");check(action_id.ok,"real action identity allocated");ActionCoreView action;action.id=action_id.value;action.kind="service";action.actor_id="guest";action.participants={"guest","host"};action.remaining_ticks=1;
 ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;check(store.apply_action(insert).ok,"real Common action inserted");
 GrowthQuote quote;quote.command.kind="service";quote.command.activity="rest";quote.context=InteractionContext{"inn","host","innkeeper"};quote.data=terms;
 check(growth::start_service_trade(store,growth,growth_catalog,action,quote).ok,"original Growth start pays actual cost and creates source");check(store.wallet("guest:wallet")->balance==95&&store.wallet("host:wallet")->balance==5,"one actual payment");
 ActionMutation due;due.action.id=action.id;due.remaining_ticks=0;check(store.apply_action(due).ok,"original action now due");action.remaining_ticks=0;
 check(store.begin_actor_condition_capture().ok,"original Common capture opened");WorldMoment finish_at;finish_at.half_day_tick=1;
 auto begun=begin_authored_rest_condition_write(store,state,c,growth,action,finish_at);if(!begun.ok)throw std::runtime_error("begin: "+begun.error.original.code);check(begun.ok,"condition boundary captures genuine before");
 check(growth::finish_service_trade(store,growth,growth_catalog,social,interaction,action).ok,"original Growth finish runs once");
 auto bound=bind_authored_rest_condition_write(store,state,c,growth,action,finish_at,begun.value);if(!bound.ok)throw std::runtime_error("bind: "+bound.error.original.code);check(bound.ok,"actual write bound to completed original source");
 check(store.actor("guest")->hp==69&&store.actor("guest")->fatigue==22&&store.state().actors.find("guest")->injury==0,"original actual body result retained");check(store.actor("host")->hp==100&&store.actor("host")->fatigue==0,"provider gets no duplicated restoration");
 check(!bind_authored_rest_condition_write(store,state,c,growth,action,finish_at,begun.value).ok,"same scope cannot apply another receipt");
 check(validate_authored_rest_capture(store,state,{bound.value}).ok,"every actual Common body write covered exactly once");check(!validate_authored_rest_capture(store,state,{}).ok,"missing bound write rejects publish");
 ObservedOriginalCompletion observed;observed.action=action.id;observed.actual_at=finish_at;
 check(validate_authored_actor_foundation(store.state(),state,c,&growth,&observed).ok,"actual old body plus real service source validates");check(!validate_authored_actor_foundation(store.state(),state,c,&growth).ok,"cold source cannot infer exact time from legacy tick");
 auto saved=encode_actor_foundation_checkpoint(state);check(saved.ok,"completed condition saved");auto restored=decode_actor_foundation_checkpoint(saved.value);check(restored.ok&&validate_authored_actor_foundation(store.state(),restored.value,c,&growth,&observed).ok,"completed origin/body source cold continued without another application");
 auto half=normalize_fraction("1","2");check(half.ok,"exact fractional test moment created");observed.actual_at.sub_half_day=half.value;check(!validate_authored_actor_foundation(store.state(),state,c,&growth,&observed).ok,"same floor different exact completion refused");observed.actual_at=finish_at;
 auto historic=read_authored_rest_capability_at_head(store.state(),state,c,f.value.condition_head_snapshot,c.rest_purpose,WorldMoment{},&growth,&observed);check(historic.ok&&historic.value.body_scalars.hp==57&&historic.value.body_scalars.fatigue==42&&historic.value.body_scalars.injury==1,"historical frame reads actual old source not current body");
 auto after_frame=capture_authored_rest_capability(store.state(),state,c,"guest",c.rest_purpose,finish_at,&growth,&observed);check(after_frame.ok&&after_frame.value.body_scalars.hp==69&&after_frame.value.condition_head_snapshot.condition_revision==1,"current frame uses same reducer and real new head");
 check(!read_authored_rest_capability_at_head(store.state(),state,c,after_frame.value.condition_head_snapshot,c.rest_purpose,WorldMoment{},&growth,&observed).ok,"future receipt cannot enter historical frame");
 WorldMoment later;later.half_day_tick=2;check(!read_authored_rest_capability_at_head(store.state(),state,c,f.value.condition_head_snapshot,c.rest_purpose,later,&growth,&observed).ok,"omitted earlier receipt cannot present stale head");
 auto wrong_head=after_frame.value.condition_head_snapshot;wrong_head.latest_receipt_ref->result_id="condition:missing";check(!read_authored_rest_capability_at_head(store.state(),state,c,wrong_head,c.rest_purpose,finish_at,&growth,&observed).ok,"historical head requires actual latest receipt identity");
 auto complete_json=encode_character_condition_extensions_v1(state.character_condition_extensions);auto complete_read=decode_character_condition_extensions_v1(complete_json.value);auto complete=state;complete.character_condition_extensions=complete_read.value;check(complete_json.ok&&complete_read.ok&&validate_authored_actor_foundation(store.state(),complete,c,&growth,&observed).ok,"actual rest receipt complete JSON cold chain");
 const auto& actual_draft=store.actor_condition_execution_drafts().front();check(actual_draft.injury_written&&!actual_draft.alive_written&&actual_draft.before->injury==1&&actual_draft.after.injury==0&&actual_draft.before->alive&&actual_draft.after.alive,"original rest four-scalar actual presence retained");
 for(bool alive_write:{false,true}){auto point=store.begin_savepoint();check(point.ok,"extra scalar test savepoint");ActorMutation extra;extra.actor_id="guest";if(alive_write)extra.alive=true;else extra.injury=0;check(store.apply_actor(extra).ok,"actual same-value injury or alive write");check(!validate_authored_rest_capture(store,state,{bound.value}).ok,"unbound same-value injury or alive cannot hide");check(store.rollback_savepoint(point.value).ok&&validate_authored_rest_capture(store,state,{bound.value}).ok,"rolled-back extra body write does not leak draft");}
 ActorMutation illicit;illicit.actor_id="guest";illicit.hp=69;check(store.apply_actor(illicit).ok,"test executes real extra same-value body write");check(!validate_authored_rest_capture(store,state,{bound.value}).ok,"unbound same-value extra write cannot hide behind final equality");
 check(store.finish_actor_condition_capture().ok,"original capture ends after test boundary");
 // 饱和净零仍是实际writer：冷档删receipt并回退head不能以末态等于初态冒完整来源。
 auto saturated_catalog=c;auto* saturated_definition=saturated_catalog.actors.find("guest");saturated_definition->condition.hp=100;saturated_definition->condition.fatigue=0;saturated_definition->condition.injury=0;
 auto saturated_common=common;saturated_common.actors.find("guest")->hp=100;saturated_common.actors.find("guest")->fatigue=0;saturated_common.actors.find("guest")->injury=0;auto saturated_seed=make_authored_actor_foundation_seed(saturated_common,saturated_catalog);check(saturated_seed.ok,"saturated real authored initial source");
 CommonStore saturated_store(saturated_common,shared);GrowthState saturated_growth;auto saturated_id=saturated_store.next_id("action");ActionCoreView saturated_action=action;saturated_action.id=saturated_id.value;saturated_action.remaining_ticks=1;ActionMutation saturated_insert;saturated_insert.kind=ActionMutation::Kind::insert;saturated_insert.action=saturated_action;check(saturated_id.ok&&saturated_store.apply_action(saturated_insert).ok&&growth::start_service_trade(saturated_store,saturated_growth,growth_catalog,saturated_action,quote).ok,"real saturated original rest started and paid");
 ActionMutation saturated_due;saturated_due.action.id=saturated_action.id;saturated_due.remaining_ticks=0;saturated_action.remaining_ticks=0;check(saturated_store.apply_action(saturated_due).ok&&saturated_store.begin_actor_condition_capture().ok,"saturated actual completion boundary");
 auto saturated_begin=begin_authored_rest_condition_write(saturated_store,saturated_seed.value,saturated_catalog,saturated_growth,saturated_action,finish_at);check(saturated_begin.ok&&growth::finish_service_trade(saturated_store,saturated_growth,growth_catalog,social,interaction,saturated_action).ok,"real original rest writer even at saturation");
 auto saturated_bound=bind_authored_rest_condition_write(saturated_store,saturated_seed.value,saturated_catalog,saturated_growth,saturated_action,finish_at,saturated_begin.value);check(saturated_bound.ok&&validate_authored_rest_capture(saturated_store,saturated_seed.value,{saturated_bound.value}).ok&&saturated_store.finish_actor_condition_capture().ok,"saturated actual body write bound once");
 ObservedOriginalCompletion saturated_time;saturated_time.action=saturated_action.id;saturated_time.actual_at=finish_at;check(validate_authored_actor_foundation(saturated_store.state(),saturated_seed.value,saturated_catalog,&saturated_growth,&saturated_time).ok,"saturated complete real source accepted");
 saturated_seed.value.character_condition_extensions.applied_receipts={};auto* saturated_head=saturated_seed.value.character_condition_extensions.actor_condition_heads.find("guest");saturated_head->condition_revision=0;saturated_head->latest_receipt_ref.reset();check(!validate_authored_actor_foundation(saturated_store.state(),saturated_seed.value,saturated_catalog,&saturated_growth,&saturated_time).ok,"deleted zero-net rest receipt and rolled-back head rejected");
 std::cout<<"ACTOR_FOUNDATION "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"ACTOR_FOUNDATION "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
