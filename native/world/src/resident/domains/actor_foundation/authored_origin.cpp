#include "resident/kernel/catalog_section_observation.hpp"
#include "injury_capability_map.hpp"
// 作者来源一次构造；同版身体从真实seed与已完成rest链核验，绝不按今日值补历史。
#include "authored_origin.hpp"
#include "validated_catalog.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "character_capability.hpp"
#include "rest_condition.hpp"
#include "travel_condition.hpp"
#include "actor_foundation_codec.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include <algorithm>
#include <limits>
#include <mutex>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace actor_foundation_catalog_reuse {
namespace {
thread_local bool counting_enabled = false;
thread_local Counts observed_counts;
}
// 只切换独立观察开关，不复位既有累计，调用者以原窗口前后快照求差。
bool set_counting_enabled(bool enabled) noexcept {
  const bool previous = counting_enabled;
  counting_enabled = enabled;
  return previous;
}
Counts counts() noexcept { return observed_counts; }
// 饱和后明确失去精确计数，不影响验证结果或首错。
void count(std::uint64_t Counts::*field) noexcept {
  if (!counting_enabled) return;
  auto &value = observed_counts.*field;
  if (value == std::numeric_limits<std::uint64_t>::max())
    observed_counts.overflow = true;
  else ++value;
}
} // namespace actor_foundation_catalog_reuse
namespace {
// 只有这四项是原 capability 目录校验对共享 GrowthCatalog 的真实依赖。
struct GrowthSelection {
  bool present = false, regional_life = false, economy = false,
       complete_study_sources = false;
  bool operator==(const GrowthSelection &other) const noexcept {
    return present == other.present && regional_life == other.regional_life &&
           economy == other.economy &&
           complete_study_sources == other.complete_study_sources;
  }
};
GrowthSelection growth_selection(const ActorFoundationCatalog &catalog) noexcept {
  const auto &growth = catalog.original_growth;
  if (!growth) return {};
  return {true, growth->regional_life, growth->economy,
          growth->complete_study_sources};
}
struct SealedActorFoundationCatalog;
// 活 owner 链不持有目录；摘链发生在其 const 内容销毁之前，地址不能越代复用。
std::mutex sealed_catalog_mutex;
SealedActorFoundationCatalog *sealed_catalog_head = nullptr;
struct SealedActorFoundationCatalog {
  const ActorFoundationCatalog catalog;
  SealedActorFoundationCatalog *next = nullptr;
  bool validated = false;
  GrowthSelection validated_growth;
  // 深值复制隔离封口前可能借出的元素地址；唯一共享 Growth 依赖逐命中实核。
  explicit SealedActorFoundationCatalog(ActorFoundationCatalog &&value)
      : catalog(value) {
    std::lock_guard<std::mutex> lock(sealed_catalog_mutex);
    next = sealed_catalog_head;
    sealed_catalog_head = this;
  }
  ~SealedActorFoundationCatalog() {
    std::lock_guard<std::mutex> lock(sealed_catalog_mutex);
    auto **at = &sealed_catalog_head;
    while (*at && *at != this) at = &(*at)->next;
    if (*at) *at = next;
  }
};
// 普通值/拷贝没有封口凭据；不按 digest、shared_ptr<const> 或裸地址猜不可变。
bool sealed_catalog_validated(const ActorFoundationCatalog &catalog) {
  std::lock_guard<std::mutex> lock(sealed_catalog_mutex);
  for (auto *owner = sealed_catalog_head; owner; owner = owner->next)
    if (&owner->catalog == &catalog)
      return owner->validated &&
             owner->validated_growth == growth_selection(catalog);
  return false;
}
// 只记录原全验证成功。外部 Growth 选择改变后仍须走原验证，不缓存失败。
void remember_sealed_catalog_success(const ActorFoundationCatalog &catalog) {
  std::lock_guard<std::mutex> lock(sealed_catalog_mutex);
  for (auto *owner = sealed_catalog_head; owner; owner = owner->next)
    if (&owner->catalog == &catalog) {
      owner->validated_growth = growth_selection(catalog);
      owner->validated = true;
      return;
    }
}
} // namespace
std::shared_ptr<const ActorFoundationCatalog>
seal_actor_foundation_catalog(ActorFoundationCatalog &&catalog) {
  // 对象及控制块保原同 job allocator；别名只发布真实 const 成员，无可写别名。
  auto owner = checkpoint_make_shared<SealedActorFoundationCatalog>(std::move(catalog));
  const auto *value = &owner->catalog;
  actor_foundation_catalog_reuse::count(
      &actor_foundation_catalog_reuse::Counts::sealed_creations);
  return std::shared_ptr<const ActorFoundationCatalog>(std::move(owner), value);
}
bool actor_foundation_rule_equal(const ActorFoundationRuleRef& a,const ActorFoundationRuleRef& b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
bool actor_foundation_origin_equal(const ScenarioOriginRef& a,const ScenarioOriginRef& b){return a.catalog_digest==b.catalog_digest&&a.scenario_id==b.scenario_id&&a.row_key==b.row_key&&a.initial_tick==b.initial_tick;}
namespace {
// 受检定义只允许此片的已实现规则族；锁内引用不授自由脚本或未知用途。
bool valid_ref(const ActorFoundationRuleRef& r,const StableId& digest){return !r.id.empty()&&r.id.size()<=256&&r.version>0&&r.catalog_digest==digest;}
bool attributes_equal(const ActorAttributes& a,const ActorAttributes& b){return a.administration==b.administration&&a.charisma==b.charisma&&a.intellect==b.intellect&&a.leadership==b.leadership&&a.martial==b.martial;}
bool skills_equal(const ActorSkills& a,const ActorSkills& b){
 if(a.archery!=b.archery||a.cavalry!=b.cavalry||a.commerce!=b.commerce||a.debate!=b.debate||a.infantry!=b.infantry||a.martial!=b.martial||a.negotiation!=b.negotiation||a.rites!=b.rites||a.tactics!=b.tactics||a.transport!=b.transport||a.extended.has_value()!=b.extended.has_value())return false;
 if(!a.extended)return true;const auto& x=*a.extended;const auto& y=*b.extended;return x.naval==y.naval&&x.espionage==y.espionage&&x.fortification==y.fortification&&x.agriculture==y.agriculture&&x.medicine==y.medicine&&x.forging==y.forging&&x.field_order==y.field_order;
}
Status validate_catalog(const ActorFoundationCatalog& c){
 using actor_foundation_catalog_reuse::Counts;
 actor_foundation_catalog_reuse::count(&Counts::requests);
 if(sealed_catalog_validated(c)) {
  actor_foundation_catalog_reuse::count(&Counts::reused);
  return common_success();
 }
 actor_foundation_catalog_reuse::count(&Counts::full_validation_begins);
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 1, "ActorFoundationCatalog", "validate_catalog", &c);

 if(c.identity.content_digest.empty()||c.scenario_id.empty()||c.actors.empty())return common_failure("AuthoredActorFoundationCatalogRequired");
 for(const auto* r:{&c.construction_profile,&c.provenance_policy,&c.rest_rule,&c.rest_purpose,&c.provider_purpose})if(!valid_ref(*r,c.identity.content_digest))return common_failure("AuthoredActorFoundationRuleInvalid");
 if(c.construction_profile.id!="selected_existing_actor_context_v2"||c.construction_profile.version!=2||c.provenance_policy.id!="character_origin_read_v2"||c.provenance_policy.version!=2||c.rest_rule.id!="original_service_rest"||c.rest_rule.version!=1||c.rest_purpose.id!="rest"||c.provider_purpose.id!="provide_rest"||c.rest_purpose.version!=1||c.provider_purpose.version!=1)return common_failure("UnsupportedActorFoundationRule",{},ErrorKind::unsupported_capability);
 if(c.study_capability_rule&&(!valid_ref(*c.study_capability_rule,c.identity.content_digest)||c.study_capability_rule->id!="study_condition_credit_v2"||c.study_capability_rule->version!=2))return common_failure("UnsupportedStudyCapabilityRule");
 if(c.work_or_study_rule&&(!valid_ref(*c.work_or_study_rule,c.identity.content_digest)||c.work_or_study_rule->id!="original_work_or_study_exertion"||c.work_or_study_rule->version!=1))return common_failure("UnsupportedWorkStudyConditionRule");
 if(c.original_long_rest_rule&&(!valid_ref(c.original_long_rest_rule->rule_ref,c.identity.content_digest)||c.original_long_rest_rule->rule_ref.id!="original_estate_long_rest"||c.original_long_rest_rule->rule_ref.version!=1))return common_failure("UnsupportedLongRestConditionRule");
 if(c.travel_rule&&(!valid_ref(*c.travel_rule,c.identity.content_digest)||c.travel_rule->id!="original_travel_exertion"||c.travel_rule->version!=1))return common_failure("UnsupportedTravelConditionRule");
 if(c.original_military_wound_rule){auto valid=validate_original_military_wound_rule(c);if(!valid.ok)return valid;}else {auto valid=validate_injury_capability_map(c);if(!valid.ok)return valid;}
 for(const auto* selected:{&c.military_death_rule,&c.natural_age_death_rule,&c.sea_death_rule})if(*selected&&!valid_ref(**selected,c.identity.content_digest))return common_failure("LifeConditionRuleInvalid");
 auto purposes=validate_character_capability_catalog(c);if(!purposes.ok)return purposes;
 const auto repeated_seed=[&](const auto& current){for(const auto& prior:c.actors.rows()){if(&prior==&current)break;if(prior.second.condition.seed_key==current.second.condition.seed_key)return true;}return false;};
 for(const auto& row:c.actors.rows()){const auto& d=row.second;const auto& o=d.scenario_origin_ref;const auto& s=d.condition;
  if(row.first.empty()||row.first.size()>256||row.first!=d.actor_id||o.row_key!=d.actor_id||o.catalog_digest!=c.identity.content_digest||o.scenario_id!=c.scenario_id||o.initial_tick!=0||!valid_ref(d.identity_definition_ref,c.identity.content_digest)||!valid_ref(d.appearance_source_ref,c.identity.content_digest)||d.identity_definition_ref.id!=d.actor_id||d.identity_definition_ref.version!=1||d.appearance_source_ref.id!=d.actor_id||d.appearance_source_ref.version!=1||s.actor_id!=d.actor_id||s.seed_key.empty()||s.seed_key.size()>256||repeated_seed(row)||!actor_foundation_origin_equal(s.source,o)||!actor_foundation_rule_equal(s.activity_limit_profile,c.rest_rule))return common_failure("AuthoredActorFoundationDefinitionInvalid",row.first);
  // 此首片的两个ref都指锁内原actors[id]定义；不接受任造appearance别名。
  for(auto value:{d.attributes.administration,d.attributes.charisma,d.attributes.intellect,d.attributes.leadership,d.attributes.martial})if(value<1||value>100)return common_failure("InvalidAuthoredBaseAttribute",row.first);
  for(auto value:{d.skills.archery,d.skills.cavalry,d.skills.commerce,d.skills.debate,d.skills.infantry,d.skills.martial,d.skills.negotiation,d.skills.rites,d.skills.tactics,d.skills.transport})if(value<0||value>1200)return common_failure("InvalidAuthoredSkill",row.first);
  if(d.skills.extended){const auto& x=*d.skills.extended;for(auto value:{x.naval,x.espionage,x.fortification,x.agriculture,x.medicine,x.forging})if(value<0||value>1200)return common_failure("InvalidAuthoredSkill",row.first);}
  if(d.age_basis.kind!=AuthoredActorAgeBasis::Kind::declared_age_at_start||d.placement.kind!=AuthoredStartPlacement::Kind::at_place||!s.episodes.empty())return common_failure("UnsupportedAuthoredActorFoundationKind",row.first,ErrorKind::unsupported_capability);
  if(d.age_basis.years<0||d.age_basis.years>120||d.age_basis.as_of_tick!=0||d.age_basis.calendar_year_ordinal!=0||!actor_foundation_origin_equal(d.age_basis.source,o)||!d.age_basis.birth_source_ref.empty()||d.age_basis.birth_tick!=0||d.placement.place_ref.empty()||!d.alive||s.hp<0||s.hp>100||s.fatigue<0||s.fatigue>100||s.injury<0||s.injury>3)return common_failure("InvalidAuthoredActorFoundationInitialValue",row.first);
 }
 remember_sealed_catalog_success(c);
 actor_foundation_catalog_reuse::count(&Counts::full_validation_successes);
 return common_success();
}
// 只组装锁定冷源；调用者另证原人物确已按本定义在同一冷候选创建。
AuthoredActorFoundationState source_state(const ActorFoundationCatalog& c){
 AuthoredActorFoundationState out;
 for(const auto& row:c.actors.rows()){const auto& d=row.second;const auto& id=d.actor_id;const auto& ref=d.scenario_origin_ref;
  out.actor_start_context.scenario_origins.push_back({ref,c.construction_profile});
  out.actor_start_context.actor_age_bases.insert(id,{id,ref,d.age_basis});
  out.actor_start_context.initial_condition_seeds.insert(d.condition.seed_key,d.condition);
  ActorDomainBinding binding;binding.actor_id=id;binding.scenario_origin_ref=ref;binding.identity_definition_ref=d.identity_definition_ref;binding.age_basis_ref={id};binding.condition_seed_refs={{d.condition.seed_key}};binding.placement_ref=d.placement;binding.appearance_source_ref=d.appearance_source_ref;
  out.actor_start_context.actor_domain_bindings.insert(id,std::move(binding));
  out.actor_start_context.construction_receipts.insert(id,{ref,c.construction_profile,{id},{{d.condition.seed_key}},id});
  CharacterOriginRecordV2 origin;origin.actor_id=id;origin.created_at.exact_moment={};origin.age_basis_ref.actor_start={id};origin.actor_start_context_ref=ref;origin.source=AuthoredCharacterSource{ref,id,d.identity_definition_ref};origin.initial_capability_basis=AuthoredInitialCapabilityBasis{c.identity,d.identity_definition_ref};origin.initial_condition_seed_ref={d.condition.seed_key};origin.provenance_policy_ref=c.provenance_policy;
  out.character_origin_records.origins.insert(id,std::move(origin));
  out.character_condition_extensions.actor_condition_heads.insert(id,{id,0,{d.condition.seed_key},{}});
 }
 return out;
}
// 比较整个冷源结构及完整CatalogIdentity，而非只核一条seed或来源字符串。
Status validate_sources(const CommonState& common,const AuthoredActorFoundationState& s,const ActorFoundationCatalog& c){
 auto valid=validate_catalog(c);if(!valid.ok)return valid;
 if(common.metadata.scenario_id!=c.scenario_id||common.metadata.content_digest!=c.identity.content_digest||common.actors.size()!=c.actors.size())return common_failure("AuthoredActorFoundationWorldMismatch");
 auto actual=s;actual.character_condition_extensions.applied_receipts={};
 for(auto& row:actual.character_condition_extensions.actor_condition_heads.rows()){row.second.condition_revision=0;row.second.latest_receipt_ref.reset();}
 auto a=encode_actor_foundation_checkpoint(actual);if(!a.ok)return {false,{},a.error};auto expected=encode_actor_foundation_checkpoint(source_state(c));if(!expected.ok)return {false,{},expected.error};
 if(a.value!=expected.value)return common_failure("AuthoredActorFoundationSourceMismatch");
 for(const auto& row:c.actors.rows()){const auto* actor=common.actors.find(row.first);const auto& d=row.second;
  if(!actor||actor->id!=row.first||actor->prototype_id||!actor->created_tick||*actor->created_tick!=0||actor->birth_tick||!attributes_equal(actor->attributes,d.attributes)||!skills_equal(actor->skills,d.skills))return common_failure("AuthoredActorFoundationIdentityMismatch",row.first);
 }
 return common_success();
}
}
Status initialize_authored_actor_foundation_actor(ActorRecord& actor,const ActorFoundationCatalog& c,const StableId& id){
 auto valid=validate_catalog(c);if(!valid.ok)return valid;const auto* d=c.actors.find(id);
 if(!d||actor.id!=id||actor.created_tick||actor.birth_tick||!actor.action_id.empty())return common_failure("AuthoredActorFoundationColdActorRequired",id);
 actor.alive=d->alive;actor.available=d->available;actor.hp=d->condition.hp;actor.fatigue=d->condition.fatigue;actor.injury=d->condition.injury;actor.created_tick=0;actor.birth_tick.reset();return common_success();
}
Outcome<AuthoredActorFoundationState> make_authored_actor_foundation_seed(const CommonState& common,const ActorFoundationCatalog& c){
 auto valid=validate_catalog(c);if(!valid.ok)return {false,{},valid.error};
 if(common.identity.tick!=0||common.identity.revision!=0||!common.actions.empty())return {false,{},common_failure("AuthoredActorFoundationColdWorldRequired").error};
 auto state=source_state(c);valid=validate_authored_actor_foundation(common,state,c);if(!valid.ok)return {false,{},valid.error};
 for(const auto& row:c.actors.rows()){const auto& a=*common.actors.find(row.first);const auto* at=std::get_if<SettlementLocation>(&a.location);if(!at||at->place_id!=row.second.placement.place_ref||a.available!=row.second.available)return {false,{},common_failure("AuthoredActorFoundationPlacementMismatch",row.first).error};}
 return {true,std::move(state),{}};
}
Outcome<CharacterOriginRecordV2> read_authored_character_origin_v2(const CommonState& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& c,const StableId& id){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::authored_read, 2, "ActorFoundationCatalog", "read_authored_character_origin_v2", &c);

 auto valid=validate_catalog(c);if(!valid.ok)return {false,{},valid.error};const auto* origin=state.character_origin_records.origins.find(id);const auto* definition=c.actors.find(id);const auto* actor=common.actors.find(id);const auto* source=origin?std::get_if<AuthoredCharacterSource>(&origin->source):nullptr;const auto* basis=origin?std::get_if<AuthoredInitialCapabilityBasis>(&origin->initial_capability_basis):nullptr;
 if(!origin||!definition||!actor||!source||!basis||common.metadata.scenario_id!=c.scenario_id||common.metadata.content_digest!=c.identity.content_digest||origin->actor_id!=id||origin->origin_kind!=CharacterOriginRecordV2::Kind::authored||origin->origin_fact_ref||actor->id!=id||actor->prototype_id||!actor->created_tick||*actor->created_tick!=0||actor->birth_tick||origin->created_at.kind!=ActorFoundationAt::Kind::exact_moment||origin->created_at.exact_moment.half_day_tick!=0||origin->age_basis_ref.kind!=CharacterAgeBasisRef::Kind::actor_start||origin->age_basis_ref.actor_start.actor_id!=id||!origin->actor_start_context_ref||!actor_foundation_origin_equal(*origin->actor_start_context_ref,definition->scenario_origin_ref)||!actor_foundation_origin_equal(source->actor_start_context_ref,definition->scenario_origin_ref)||source->authored_actor_id!=id||!actor_foundation_rule_equal(source->prototype_ref,definition->identity_definition_ref)||!actor_foundation_rule_equal(basis->prototype_ref,definition->identity_definition_ref)||!actor_foundation_rule_equal(origin->provenance_policy_ref,c.provenance_policy)||origin->initial_condition_seed_ref.seed_key!=definition->condition.seed_key)return {false,{},common_failure("AuthoredCharacterOriginSourceMismatch",id).error};
 auto zero=compare_world_moments(origin->created_at.exact_moment,WorldMoment{});if(!zero.ok)return {false,{},zero.error};if(zero.value)return {false,{},common_failure("AuthoredCharacterOriginTimeMismatch",id).error};const auto& a=basis->catalog_identity;const auto& b=c.identity;if(a.content_id!=b.content_id||a.content_version!=b.content_version||a.content_digest!=b.content_digest||a.ruleset_id!=b.ruleset_id||a.schema_sha256!=b.schema_sha256||a.pack_set_digest!=b.pack_set_digest||a.module_versions!=b.module_versions||a.artifact_sha256!=b.artifact_sha256)return {false,{},common_failure("AuthoredCharacterOriginCatalogMismatch",id).error};
 const auto* seed=state.actor_start_context.initial_condition_seeds.find(origin->initial_condition_seed_ref.seed_key);if(!seed||seed->actor_id!=id||seed->hp!=definition->condition.hp||seed->fatigue!=definition->condition.fatigue||seed->injury!=definition->condition.injury||!seed->episodes.empty()||!actor_foundation_origin_equal(seed->source,definition->condition.source)||!actor_foundation_rule_equal(seed->activity_limit_profile,definition->condition.activity_limit_profile))return {false,{},common_failure("AuthoredCharacterInitialConditionMismatch",id).error};return {true,*origin,{}};
}
Status validate_authored_start_context_sources(const CommonState& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog){
 auto valid=validate_catalog(catalog);if(!valid.ok)return valid;const auto& context=state.actor_start_context;const auto count=catalog.actors.size();
 if(common.metadata.scenario_id!=catalog.scenario_id||common.metadata.content_digest!=catalog.identity.content_digest||context.scenario_origins.size()!=count||context.actor_age_bases.size()!=count||context.actor_domain_bindings.size()!=count||context.construction_receipts.size()!=count)return common_failure("AuthoredStartContextCoverageMismatch",{},ErrorKind::internal);
 for(const auto& row:catalog.actors.rows()){
  const auto& definition=row.second;const auto& ref=definition.scenario_origin_ref;auto origin=read_authored_character_origin_v2(common,state,catalog,row.first);if(!origin.ok)return {false,{},origin.error};
  const auto* actor=common.actors.find(row.first);if(!actor||!attributes_equal(actor->attributes,definition.attributes)||actor->skills.extended.has_value()!=definition.skills.extended.has_value())return common_failure("AuthoredInitialCapabilitySourceMismatch",row.first,ErrorKind::internal);for(const auto& skill:growth_skill_ids(definition.skills.extended.has_value()))if(growth_skill_xp(actor->skills,skill)<growth_skill_xp(definition.skills,skill))return common_failure("AuthoredInitialSkillSourceMismatch",row.first,ErrorKind::internal);
  std::size_t source_count=0;for(const auto& source:context.scenario_origins)if(actor_foundation_origin_equal(source.ref,ref)){++source_count;if(!actor_foundation_rule_equal(source.construction_profile_ref,catalog.construction_profile))return common_failure("AuthoredConstructionRuleMismatch",row.first,ErrorKind::internal);}if(source_count!=1)return common_failure("AuthoredScenarioOriginCoverageMismatch",row.first,ErrorKind::internal);
  const auto* age=context.actor_age_bases.find(row.first);const auto* binding=context.actor_domain_bindings.find(row.first);const auto* construction=context.construction_receipts.find(row.first);
  if(!age||age->actor_id!=row.first||!actor_foundation_origin_equal(age->scenario_origin_ref,ref))return common_failure("AuthoredAgeSourceMismatch",row.first,ErrorKind::internal);
  const auto& a=age->age_basis;const auto& b=definition.age_basis;if(a.kind!=b.kind||a.birth_tick!=b.birth_tick||a.birth_source_ref!=b.birth_source_ref||a.years!=b.years||a.as_of_tick!=b.as_of_tick||a.calendar_year_ordinal!=b.calendar_year_ordinal||!actor_foundation_origin_equal(a.source,b.source))return common_failure("AuthoredAgeSourceMismatch",row.first,ErrorKind::internal);
  if(!binding||binding->actor_id!=row.first||!actor_foundation_origin_equal(binding->scenario_origin_ref,ref)||!actor_foundation_rule_equal(binding->identity_definition_ref,definition.identity_definition_ref)||binding->age_basis_ref.actor_id!=row.first||binding->condition_seed_refs.size()!=1||binding->condition_seed_refs.front().seed_key!=definition.condition.seed_key||binding->placement_ref.kind!=definition.placement.kind||binding->placement_ref.place_ref!=definition.placement.place_ref||binding->activity_ref||binding->custody_ref||!binding->service_refs.empty()||!binding->office_refs.empty()||!binding->property_business_refs.empty()||!binding->initial_knowledge_refs.empty()||!actor_foundation_rule_equal(binding->appearance_source_ref,definition.appearance_source_ref))return common_failure("AuthoredDomainBindingMismatch",row.first,ErrorKind::internal);
  if(!construction||!actor_foundation_origin_equal(construction->scenario_origin_ref,ref)||!actor_foundation_rule_equal(construction->construction_profile_ref,catalog.construction_profile)||construction->constructed_actor_ids.size()!=1||construction->constructed_actor_ids.front()!=row.first||construction->initial_source_refs.size()!=1||construction->initial_source_refs.front().seed_key!=definition.condition.seed_key||construction->initialization_result_ref!=row.first)return common_failure("AuthoredConstructionReceiptMismatch",row.first,ErrorKind::internal);
 }
 return common_success();
}
Status validate_authored_actor_foundation(const CommonState& common,const AuthoredActorFoundationState& s,const ActorFoundationCatalog& c,const GrowthState* growth,const AuthoredRestTimeRead* times,const MovementExactState* movement){
 auto valid=validate_sources(common,s,c);if(!valid.ok)return valid;
 OrderedTable<AuthoredConditionCurrent> current;for(const auto& row:c.actors.rows()){const auto& d=row.second;current.insert(row.first,{d.condition.hp,d.condition.fatigue,d.condition.injury,d.alive});}
 std::set<std::tuple<Integer,StableId,StableId>> business;
 for(const auto& row:s.character_condition_extensions.applied_receipts.rows()){const auto& r=row.second;auto* a=current.find(r.actor_id);
  if((!std::holds_alternative<OriginalRestProvidedCause>(r.cause)&&!std::holds_alternative<TravelExertionConditionCause>(r.cause))||(!std::holds_alternative<RestConditionBusinessKey>(r.business_key)&&!std::holds_alternative<TravelConditionBusinessKey>(r.business_key)))return common_failure("CompleteCharacterConditionReaderRequired",row.first,ErrorKind::unsupported_capability);
  if(!a||row.first!=r.result_id||!business.emplace(Integer(r.cause.index()),std::holds_alternative<RestConditionBusinessKey>(r.business_key)?std::get<RestConditionBusinessKey>(r.business_key).action_id:std::get<TravelConditionBusinessKey>(r.business_key).exertion_source_id,r.actor_id).second||a->revision==std::numeric_limits<Integer>::max()||r.condition_revision_before!=a->revision||r.condition_revision_after!=a->revision+1)return common_failure("ActorConditionChainInvalid",row.first);
  if(std::holds_alternative<OriginalRestProvidedCause>(r.cause)){if(!growth||!times)return common_failure("RestConditionSourceRequired",row.first);valid=validate_authored_rest_receipt(common,r,c,*growth,*times);}
  else {if(!movement)return common_failure("TravelConditionSourceRequired",row.first);valid=validate_travel_exertion_condition_receipt(common,r,c,*movement);}
  if(!valid.ok)return valid;
  auto order=compare_world_moments(a->last_at,r.effective_at.exact_moment);if(!order.ok)return {false,{},order.error};if(order.value>0)return common_failure("AuthoredRestConditionTimeReversed",row.first);
  for(const auto& change:r.scalar_changes){Integer* field=nullptr;switch(change.field){case ConditionScalarChange::Field::hp:field=&a->hp;break;case ConditionScalarChange::Field::fatigue:field=&a->fatigue;break;case ConditionScalarChange::Field::injury:field=&a->injury;break;case ConditionScalarChange::Field::alive:return common_failure("UnsupportedAuthoredRestLifeTransition",row.first,ErrorKind::unsupported_capability);}
   if(!field||*field!=change.before)return common_failure("AuthoredRestConditionBeforeMismatch",row.first);*field=change.after;
  }
  a->revision=r.condition_revision_after;a->latest=ConditionReceiptRefV2{r.result_id};a->last_at=r.effective_at.exact_moment;
 }
 // 原已完成rest即使身体饱和净零，也必须反向找到唯一真实收据；不能删史回退head。
 if(growth)for(const auto& row:growth->life_activities.rows()){const auto& v=row.second.view;if(v.kind==LifeActivityKind::service&&v.activity=="rest"&&v.phase==LifeActivityPhase::completed){if(row.first!=v.action_id||v.fact_id!=v.action_id||!business.count({1,v.action_id,v.actor_id}))return common_failure("AuthoredRestReceiptCoverageMismatch",row.first);}}
 if(movement){valid=validate_travel_exertion_condition_coverage(common,s,c,*movement);if(!valid.ok)return valid;}
 for(const auto& row:current.rows()){const auto& a=row.second;const auto& actor=*common.actors.find(row.first);const auto* h=s.character_condition_extensions.actor_condition_heads.find(row.first);
  if(!h||h->condition_revision!=a.revision||h->latest_receipt_ref.has_value()!=a.latest.has_value()||(a.latest&&h->latest_receipt_ref->result_id!=a.latest->result_id)||actor.hp!=a.hp||actor.fatigue!=a.fatigue||actor.injury!=a.injury||actor.alive!=a.alive)return common_failure("AuthoredActorConditionCurrentMismatch",row.first);
 }
 return common_success();
}
namespace {
// 所有当前/历史用途共用同一纯reducer；输入只是已验证源链的一次局部值。
Outcome<FrozenFacilityCapabilityInputs> rest_capability(const ActorFoundationCatalog& c,const ActorConditionHead& head,const FrozenBodyScalars& body,const ActorFoundationRuleRef& purpose,const WorldMoment& at){
 if(!actor_foundation_rule_equal(purpose,c.rest_purpose)&&!actor_foundation_rule_equal(purpose,c.provider_purpose))return {false,{},common_failure("UnsupportedActorCapabilityPurpose",purpose.id,ErrorKind::unsupported_capability).error};
 auto moment=encode_world_moment(at);if(!moment.ok)return {false,{},moment.error};if(at.half_day_tick<0)return {false,{},common_failure("ActorCapabilityTimeInvalid").error};
 FrozenFacilityCapabilityInputs out;out.actor_id=head.actor_id;out.purpose_ref=purpose;out.captured_at=at;out.condition_head_snapshot=head;out.body_scalars=body;out.selected_rule_refs={purpose,c.rest_rule};
 // 原服务身体门只核活着，HP不是新增资格门；available/在场/占用由原Common/接触门独立判断。
 out.result.allowed=body.alive;out.result.efficiency_bp=out.result.allowed?10000:0;out.result.rule_ref=purpose;out.result.observed_at=at;if(!out.result.allowed)out.result.limiting_condition_refs={head.initial_condition_seed_ref};
 out.field_order={"actor_id","purpose_ref","captured_at","condition_head_snapshot","body_scalars","active_condition_inputs","selected_rule_refs","result","field_order"};return {true,std::move(out),{}};
}
}
Outcome<FrozenFacilityCapabilityInputs> read_authored_rest_capability_at_head(const CommonState& common,const AuthoredActorFoundationState& s,const ActorFoundationCatalog& c,const ActorConditionHead& expected,const ActorFoundationRuleRef& purpose,const WorldMoment& at,const GrowthState* growth,const AuthoredRestTimeRead* times,const MovementExactState* movement){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::authored_read, 3, "ActorFoundationCatalog", "read_authored_rest_capability_at_head", &c);

 auto valid=validate_authored_actor_foundation(common,s,c,growth,times,movement);if(!valid.ok)return {false,{},valid.error};
 const auto* d=c.actors.find(expected.actor_id);if(!d)return {false,{},common_failure("UnknownActor",expected.actor_id).error};
 ActorConditionHead head{expected.actor_id,0,{d->condition.seed_key},{}};FrozenBodyScalars body{d->alive,d->condition.hp,d->condition.fatigue,d->condition.injury,{"alive","hp","fatigue","injury"}};
 if(expected.condition_revision<0)return {false,{},common_failure("InvalidHistoricalConditionHead").error};
 for(const auto& row:s.character_condition_extensions.applied_receipts.rows()){const auto& r=row.second;if(r.actor_id!=expected.actor_id)continue;
  auto order=compare_world_moments(r.effective_at.exact_moment,at);if(!order.ok)return {false,{},order.error};
  if(r.condition_revision_after>expected.condition_revision){if(order.value<0)return {false,{},common_failure("StaleHistoricalConditionHead",r.result_id).error};continue;}
  if(order.value>0)return {false,{},common_failure("FutureHistoricalConditionReceipt",r.result_id).error};
  for(const auto& change:r.scalar_changes){switch(change.field){case ConditionScalarChange::Field::hp:body.hp=change.after;break;case ConditionScalarChange::Field::fatigue:body.fatigue=change.after;break;case ConditionScalarChange::Field::injury:body.injury=change.after;break;case ConditionScalarChange::Field::alive:return {false,{},common_failure("UnsupportedAuthoredRestLifeTransition",r.result_id,ErrorKind::unsupported_capability).error};}}
  head.condition_revision=r.condition_revision_after;head.latest_receipt_ref=ConditionReceiptRefV2{r.result_id};
 }
 if(head.condition_revision!=expected.condition_revision||head.initial_condition_seed_ref.seed_key!=expected.initial_condition_seed_ref.seed_key||head.latest_receipt_ref.has_value()!=expected.latest_receipt_ref.has_value()||(head.latest_receipt_ref&&head.latest_receipt_ref->result_id!=expected.latest_receipt_ref->result_id))return {false,{},common_failure("HistoricalConditionHeadMismatch",expected.actor_id).error};
 return rest_capability(c,head,body,purpose,at);
}
Outcome<FrozenFacilityCapabilityInputs> capture_authored_rest_capability(const CommonState& common,const AuthoredActorFoundationState& s,const ActorFoundationCatalog& c,const StableId& id,const ActorFoundationRuleRef& purpose,const WorldMoment& at,const GrowthState* growth,const AuthoredRestTimeRead* times,const MovementExactState* movement){
 const auto* head=s.character_condition_extensions.actor_condition_heads.find(id);if(!head)return {false,{},common_failure("UnknownActor",id).error};
 return read_authored_rest_capability_at_head(common,s,c,*head,purpose,at,growth,times,movement);
}
} // 新业务或外部身体原因未接入时严格拒绝，不默回legacy来源。

namespace hanmo::resident {
bool actor_foundation_origin_equal(const InitialConditionSeedSource& a,const ScenarioOriginRef& b){const auto* value=std::get_if<ScenarioOriginRef>(&a);return value&&actor_foundation_origin_equal(*value,b);}
bool actor_foundation_origin_equal(const InitialConditionSeedSource& a,const InitialConditionSeedSource& b){const auto* value=std::get_if<ScenarioOriginRef>(&b);return value&&actor_foundation_origin_equal(a,*value);}
}
