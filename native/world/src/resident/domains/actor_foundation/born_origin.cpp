#include "born_origin.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id,ErrorKind::internal).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void selected(const CommonStore& c,const ActorFoundationCatalog& d,const BornCharacterSourceRead& source){
 if(!source.bound_to(c))fail("BornOriginCandidateMismatch");bool start=false,birth=false;for(const auto& row:d.identity.module_versions){if(row.first=="actor_start_context")start=row.second=="3";if(row.first=="family_birth")birth=row.second=="4";}
 if(!start||!birth||d.identity.content_digest!=c.state().metadata.content_digest||d.scenario_id!=c.state().metadata.scenario_id)throw Failure{common_failure("BornOriginSelectedProfileRequired",{},ErrorKind::unsupported_capability).error};
}
bool attributes(const ActorAttributes&a,const ActorAttributes&b){return a.administration==b.administration&&a.charisma==b.charisma&&a.intellect==b.intellect&&a.leadership==b.leadership&&a.martial==b.martial;}
ActorFoundationRuleRef rule(const ActorFoundationCatalog& d){return {"original_birth_capability_allocation",1,d.identity.content_digest};}
ActorFoundationRuleRef prototype(const ActorFoundationCatalog& d,const StableId& id){return {id,1,d.identity.content_digest};}
bool same_time(const ActorFoundationAt& at,const WorldMoment& m){return at.kind==ActorFoundationAt::Kind::exact_moment&&at.legacy_tick==0&&take(compare_world_moments(at.exact_moment,m))==0;}
}
Outcome<BornOriginView> read_born_character_origin_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const BornCharacterSourceRead& sources,const StableId& actor,const WorldMoment& now){try{
 selected(common,catalog,sources);const auto* origin=state.character_origin_records.origins.find(actor);const auto* person=common.state().actors.find(actor);
 if(!origin||!person||origin->actor_id!=actor||origin->origin_kind!=CharacterOriginRecordV2::Kind::born||origin->origin_fact_ref!=std::optional<StableId>{actor}||origin->actor_start_context_ref||origin->age_basis_ref.kind!=CharacterAgeBasisRef::Kind::birth||!actor_foundation_rule_equal(origin->provenance_policy_ref,catalog.provenance_policy))fail("BornOriginIdentityRequired",actor);
 const auto* source=std::get_if<BornCharacterSource>(&origin->source);const auto* basis=std::get_if<BirthInitialCapabilityBasis>(&origin->initial_capability_basis);if(!source||!basis||basis->birth_record_ref.birth_fact_id!=source->birth_record_ref.birth_fact_id||origin->age_basis_ref.birth.birth_fact_id!=source->birth_record_ref.birth_fact_id)fail("BornOriginSourceMismatch",actor);
 auto original=take(sources.read_original_birth(source->birth_record_ref));const auto& birth=*original.birth;if(birth.actor_id!=actor||source->biological_parent_refs!=birth.biological_parent_ids||source->innate_result_ref!=std::optional<StableId>{source->birth_record_ref.birth_fact_id}||basis->allocation_source_ref!=birth.allocation_source_ref||!same_time(origin->created_at,original.occurred_at)||take(compare_world_moments(original.occurred_at,now))>0)fail("BornOriginSourceMismatch",actor);
 const auto* allocation=state.character_origin_records.allocation_sources.find(basis->allocation_source_ref);const auto* original_ref=allocation?std::get_if<BirthFactRef>(&allocation->original_source_ref):nullptr;
 if(!allocation||allocation->allocation_result_id!=basis->allocation_source_ref||allocation->actor_id!=actor||allocation->allocation_kind!=InitialCapabilityAllocation::Kind::birth||!original_ref||original_ref->birth_fact_id!=source->birth_record_ref.birth_fact_id||!actor_foundation_rule_equal(allocation->allocation_rule_ref,rule(catalog))||!same_time(allocation->applied_at,original.occurred_at)||!attributes(allocation->base_attributes,original.prototype->attributes)||!attributes(person->attributes,allocation->base_attributes)||allocation->initial_skill_xp.extended.has_value()!=original.prototype->skills.extended.has_value()||person->skills.extended.has_value()!=allocation->initial_skill_xp.extended.has_value())fail("BornInitialAllocationMismatch",actor);
 for(const auto& skill:growth_skill_ids(allocation->initial_skill_xp.extended.has_value()))if(growth_skill_xp(allocation->initial_skill_xp,skill)!=growth_skill_xp(original.prototype->skills,skill)||growth_skill_xp(person->skills,skill)<growth_skill_xp(allocation->initial_skill_xp,skill))fail("BornInitialSkillSourceMismatch",actor);
 if(allocation->selected_input_refs.size()!=1)fail("BornAllocationOriginalInputMismatch",actor);const auto* input=std::get_if<AllocationPrototypeInput>(&allocation->selected_input_refs.front());if(!input||!actor_foundation_rule_equal(input->prototype_ref,prototype(catalog,birth.prototype_id))||allocation->actual_selection_result_refs.size()!=birth.innate.size())fail("BornAllocationOriginalInputMismatch",actor);
 for(std::size_t n=0;n<birth.innate.size();++n){const auto* ref=std::get_if<BirthInnateSelectionRef>(&allocation->actual_selection_result_refs[n]);if(!ref||ref->birth_ref.birth_fact_id!=source->birth_record_ref.birth_fact_id||ref->family_id!=birth.innate.rows()[n].first)fail("BornAllocationOriginalSelectionMismatch",actor);}
 const auto* seed=state.actor_start_context.initial_condition_seeds.find(origin->initial_condition_seed_ref.seed_key);const auto* cause=seed?std::get_if<BirthConditionSeedSource>(&seed->source):nullptr;
 if(!seed||!cause||seed->seed_key!=origin->initial_condition_seed_ref.seed_key||seed->actor_id!=actor||cause->birth_record_ref.birth_fact_id!=source->birth_record_ref.birth_fact_id||cause->allocation_source_ref!=basis->allocation_source_ref||seed->hp!=100||seed->fatigue!=0||seed->injury!=0||!seed->episodes.empty()||!actor_foundation_rule_equal(seed->activity_limit_profile,catalog.rest_rule))fail("BornInitialConditionMismatch",actor);
 if(state.actor_start_context.actor_age_bases.find(actor)||state.actor_start_context.actor_domain_bindings.find(actor))fail("DynamicBornActorInAuthoredContext",actor);
 return {true,{origin,allocation,seed,original},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
// 在同一候选内记录出生来源；真实人物初态不符时保留原失败门，由调用方丢弃候选。
Status record_born_character_origin_in_candidate(
    CommonStore &common, AuthoredActorFoundationState &state,
    const ActorFoundationCatalog &catalog,
    const BornCharacterSourceRead &sources, const BirthFactRef &ref,
    const CurrentMomentRead &clock) {
  try {
    selected(common, catalog, sources);
    auto now = take(clock.read_current_moment());
    auto actual = take(sources.read_original_birth(ref));
    const auto &birth = *actual.birth;
    const auto &actor = birth.actor_id;
    if (take(compare_world_moments(now, actual.occurred_at)))
      fail("BornOriginActualCandidateRequired", actor);
    auto &records = state.character_origin_records;
    auto &heads = state.character_condition_extensions.actor_condition_heads;
    if (records.origins.find(actor) ||
        records.allocation_sources.find(birth.allocation_source_ref) ||
        heads.find(actor))
      fail("BornOriginAlreadyApplied", actor);
    const auto *person = common.state().actors.find(actor);
    if (!person || !person->alive || person->available || person->hp != 100 ||
        person->fatigue != 0 || person->injury != 0)
      fail("BornOriginalInitialBodyRequired", actor);
    const auto seed_id = take(common.next_id("initial_condition_seed"));
    InitialCapabilityAllocation allocation;
    allocation.allocation_result_id = birth.allocation_source_ref;
    allocation.actor_id = actor;
    allocation.original_source_ref = ref;
    allocation.allocation_rule_ref = rule(catalog);
    allocation.base_attributes = actual.prototype->attributes;
    allocation.initial_skill_xp = actual.prototype->skills;
    allocation.selected_input_refs.push_back(
        AllocationPrototypeInput{prototype(catalog, birth.prototype_id)});
    for (const auto &row : birth.innate.rows())
      allocation.actual_selection_result_refs.push_back(
          BirthInnateSelectionRef{ref, row.first});
    allocation.applied_at.exact_moment = now;
    InitialConditionSeed seed;
    seed.seed_key = seed_id;
    seed.actor_id = actor;
    seed.source = BirthConditionSeedSource{ref, birth.allocation_source_ref};
    seed.hp = 100;
    seed.activity_limit_profile = catalog.rest_rule;
    CharacterOriginRecordV2 origin;
    origin.actor_id = actor;
    origin.origin_kind = CharacterOriginRecordV2::Kind::born;
    origin.origin_fact_ref = actor;
    origin.created_at.exact_moment = now;
    origin.age_basis_ref.kind = CharacterAgeBasisRef::Kind::birth;
    origin.age_basis_ref.birth = ref;
    origin.source = BornCharacterSource{ref, birth.biological_parent_ids,
                                        ref.birth_fact_id};
    origin.initial_capability_basis =
        BirthInitialCapabilityBasis{ref, birth.allocation_source_ref};
    origin.initial_condition_seed_ref = {seed_id};
    origin.provenance_policy_ref = catalog.provenance_policy;
    if (!records.allocation_sources.insert(birth.allocation_source_ref,
                                           std::move(allocation)) ||
        !state.actor_start_context.initial_condition_seeds.insert(
            seed_id, std::move(seed)) ||
        !records.origins.insert(actor, std::move(origin)) ||
        !heads.insert(actor, ActorConditionHead{actor, 0, {seed_id}, {}}))
      fail("DuplicateBornOriginSource", actor);
    take(read_born_character_origin_v2(common, state, catalog, sources, actor,
                                       now));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
}
