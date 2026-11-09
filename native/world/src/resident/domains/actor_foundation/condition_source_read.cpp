#include "condition_source_read.hpp"
#include "created_origin.hpp"
namespace hanmo::resident {
Outcome<Integer> read_initial_condition_seed_schema(const ActorFoundationCatalog& catalog){Integer schema=0;for(const auto& module:catalog.identity.module_versions)if(module.first=="actor_start_context"){if(schema)return {false,0,common_failure("DuplicateActorStartContextSelection").error};if(module.second=="2")schema=1;else if(module.second=="3")schema=2;else return {false,0,common_failure("UnsupportedActorStartContextSelection").error};}return schema?Outcome<Integer>{true,schema,{}}:Outcome<Integer>{false,0,common_failure("ActorStartContextSelectionRequired").error};}
SourceRef initial_condition_seed_source(const InitialConditionSeedRef& ref){return {"actor_foundation","initial_condition_seed",ref.seed_key};}
SourceRef character_condition_receipt_source(const ConditionReceiptRefV2& ref){return {"actor_foundation","condition_receipt",ref.result_id};}
Outcome<CharacterConditionSourceView> read_character_condition_source(const CharacterConditionSources& body,const SourceRef& ref,Integer schema){
 if(ref.domain!="actor_foundation"||ref.id.empty())return {false,{},common_failure("CharacterConditionSourceIdentityInvalid",ref.id).error};
 CharacterConditionSourceView out;out.source=ref;out.source_schema=schema;
 if(schema==2&&ref.kind=="condition_receipt"){
  auto receipt=read_verified_character_condition_change(body,{ref.id});if(!receipt.ok)return {false,{},receipt.error};
  out.actor_id=receipt.value->actor_id;out.occurred_at=receipt.value->effective_at.exact_moment;out.condition_revision=receipt.value->condition_revision_after;out.receipt=receipt.value;
 }else if(ref.kind=="initial_condition_seed"){
  auto selected=read_initial_condition_seed_schema(body.catalog);if(!selected.ok)return {false,{},selected.error};if(schema!=selected.value)return {false,{},common_failure("InitialConditionSeedSchemaMismatch",ref.id).error};
  auto valid=validate_complete_character_conditions(body);if(!valid.ok)return {false,{},valid.error};
  const auto* seed=body.state.actor_start_context.initial_condition_seeds.find(ref.id);
  if(!seed||seed->seed_key!=ref.id)return {false,{},common_failure("CharacterInitialConditionSourceRequired",ref.id).error};
  auto origin=read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,seed->actor_id,body.actual_now,body.born_sources);if(!origin.ok)return {false,{},origin.error};
  const auto* stored=body.state.character_origin_records.origins.find(seed->actor_id);
  if(!stored||stored->initial_condition_seed_ref.seed_key!=ref.id||stored->created_at.kind!=ActorFoundationAt::Kind::exact_moment)return {false,{},common_failure("CharacterInitialConditionSourceMismatch",ref.id).error};
  out.actor_id=seed->actor_id;out.occurred_at=stored->created_at.exact_moment;out.initial_seed=seed;
 }else return {false,{},common_failure("UnregisteredCharacterConditionSource",ref.id).error};
 auto order=compare_world_moments(out.occurred_at,body.actual_now);if(!order.ok)return {false,{},order.error};if(order.value>0)return {false,{},common_failure("CharacterConditionSourceInFuture",ref.id).error};
 return {true,std::move(out),{}};
}
Outcome<CharacterConditionSourceView> read_character_condition_head_source(const CharacterConditionSources& body,const ActorConditionHead& head,const WorldMoment& at){
 auto actual=read_character_condition_at_head(body,head,at);if(!actual.ok)return {false,{},actual.error};
 auto seed_schema=read_initial_condition_seed_schema(body.catalog);if(!seed_schema.ok)return {false,{},seed_schema.error};
 auto source=head.latest_receipt_ref?read_character_condition_source(body,character_condition_receipt_source(*head.latest_receipt_ref),2):read_character_condition_source(body,initial_condition_seed_source(head.initial_condition_seed_ref),seed_schema.value);
 if(!source.ok)return source;
 if(source.value.actor_id!=head.actor_id||source.value.condition_revision!=head.condition_revision)return {false,{},common_failure("CharacterConditionHeadSourceMismatch",head.actor_id).error};
 return source;
}
}
namespace hanmo::resident {
Outcome<CharacterConditionAtHead> read_character_condition_at_source(const CharacterConditionSources& body,const SourceRef& source,Integer schema,const WorldMoment& at){
 auto actual=read_character_condition_source(body,source,schema);if(!actual.ok)return {false,{},actual.error};
 const auto* origin=body.state.character_origin_records.origins.find(actual.value.actor_id);if(!origin)return {false,{},common_failure("CharacterConditionSourceOriginRequired",source.id).error};
 ActorConditionHead head{actual.value.actor_id,actual.value.condition_revision,origin->initial_condition_seed_ref,{}};
 if(actual.value.receipt)head.latest_receipt_ref=ConditionReceiptRefV2{actual.value.receipt->result_id};
 else if(!actual.value.initial_seed||actual.value.condition_revision!=0||origin->initial_condition_seed_ref.seed_key!=actual.value.initial_seed->seed_key)return {false,{},common_failure("CharacterConditionInitialHeadMismatch",source.id).error};
 return read_character_condition_at_head(body,head,at);
}
}
