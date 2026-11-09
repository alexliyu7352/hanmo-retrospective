#include "created_origin.hpp"
#include "born_origin.hpp"
#include "character_availability_provenance.hpp"
#include "condition_source_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::rule){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
bool selected(const ActorFoundationCatalog& catalog){std::size_t count=0;bool complete=false;for(const auto& m:catalog.identity.module_versions)if(m.first=="actor_start_context"){++count;complete=m.second=="3";}return count==1&&complete;}
bool attributes(const ActorAttributes&a,const ActorAttributes&b){return a.administration==b.administration&&a.charisma==b.charisma&&a.intellect==b.intellect&&a.leadership==b.leadership&&a.martial==b.martial;}
bool appearance(const AppearanceProfile& a,const CharacterCreationAppearance& b){if(a.format_version!=1||a.generator_version!="appearance-v1"||a.generation_seed!="not-sampled"||a.source!="custom"||a.attire_style!=b.attire_style||!a.base_features.present||!a.parent_sources.empty())return false;const auto& x=a.base_features;const auto& y=b.features;return x.face_shape==y.face_shape&&x.eyes==y.eyes&&x.brows==y.brows&&x.nose==y.nose&&x.mouth==y.mouth&&x.hair==y.hair&&x.beard==y.beard&&x.skin_palette==y.skin_palette&&x.build==y.build;}
ActorFoundationRuleRef creation_rule(const ActorFoundationCatalog& c){return {"original_character_creation",1,c.identity.content_digest};}
}
Outcome<CreatedOriginView> read_created_character_origin_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyCatalog& family,const CommonCatalog& shared,const StableId& id,const WorldMoment& current){try{
 if(!selected(catalog)||!family.character_creation)fail("CreatedOriginSelectedSourceProfileRequired",id,ErrorKind::unsupported_capability);
 if(common.state().metadata.scenario_id!=catalog.scenario_id||common.state().metadata.content_digest!=catalog.identity.content_digest)fail("CreatedOriginCatalogIdentityMismatch",id,ErrorKind::internal);
 const auto* origin=state.character_origin_records.origins.find(id);const auto* actor=common.state().actors.find(id);const auto* header=common.state().facts.find(id);
 if(!origin||!actor||origin->actor_id!=id||origin->origin_kind!=CharacterOriginRecordV2::Kind::created||origin->origin_fact_ref!=std::optional<StableId>{id}||origin->actor_start_context_ref||!header||header->id!=id||header->domain!="family"||header->kind!="actor_creation")fail("CreatedOriginIdentityRequired",id);
 const auto* source=std::get_if<CreatedCharacterSource>(&origin->source);const auto* basis=std::get_if<CreatedInitialCapabilityBasis>(&origin->initial_capability_basis);
 if(!source||!basis||source->creation_result_ref.empty()||basis->creation_result_ref!=source->creation_result_ref||origin->age_basis_ref.kind!=CharacterAgeBasisRef::Kind::creation||origin->age_basis_ref.creation.registration_result_id!=source->creation_result_ref||!actor_foundation_rule_equal(origin->provenance_policy_ref,catalog.provenance_policy))fail("CreatedOriginSourceMismatch",id);
 const auto* registration=state.character_origin_records.registration_sources.find(source->creation_result_ref);const auto* creation=registration?std::get_if<CreatedRegistrationSource>(&registration->source):nullptr;const auto* age=registration?std::get_if<DeclaredAgeAtRegistration>(&registration->age_basis):nullptr;
 if(!registration||!creation||!age||registration->registration_kind!=RegistrationSourceV2::Kind::created||registration->registration_result_id!=source->creation_result_ref||registration->actor_id!=id||registration->initial_condition_seed_ref.seed_key!=origin->initial_condition_seed_ref.seed_key||!actor_foundation_rule_equal(creation->creation_rule_ref,creation_rule(catalog)))fail("CreatedRegistrationSourceMismatch",id);
 take(validate_character_creation_draft(creation->draft,shared.extended_skills));const auto& definition=*family.character_creation;
 if(std::find(definition.allowed_modes.begin(),definition.allowed_modes.end(),creation->draft.mode)==definition.allowed_modes.end()||actor->prototype_id!=definition.prototype_actor_id||actor->display_name!=creation->draft.display_name||actor->birth_tick||actor->created_tick!=std::optional<Integer>{0}||actor->wallet_id!="personal:"+id||actor->inventory_id!="bag:"+id||!attributes(actor->attributes,creation->draft.attributes)||actor->skills.extended.has_value()!=shared.extended_skills)fail("CreatedOriginalActorMismatch",id);
 for(const auto& skill:growth_skill_ids(shared.extended_skills))if(growth_skill_xp(actor->skills,skill)<growth_skill_xp(creation->draft.skills,skill))fail("CreatedInitialSkillSourceMismatch",id);
 if(registration->registered_at.kind!=ActorFoundationAt::Kind::exact_moment||origin->created_at.kind!=ActorFoundationAt::Kind::exact_moment||take(compare_world_moments(registration->registered_at.exact_moment,WorldMoment{}))||take(compare_world_moments(origin->created_at.exact_moment,registration->registered_at.exact_moment))||take(compare_world_moments(origin->created_at.exact_moment,current))>0)fail("CreatedActualInitialMomentRequired",id);
 const auto* age_source=std::get_if<CreationRegistrationRef>(&age->source_ref);if(!age_source||age_source->registration_result_id!=registration->registration_result_id||age->years!=creation->draft.age_years||age->as_of_tick!=0||age->calendar_year_ordinal!=0)fail("CreatedDeclaredAgeSourceMismatch",id);auto calendar=take(OriginalCalendarRead::admit(catalog.identity,age->calendar_profile_ref));take(calendar.declared_age_at(age->years,age->as_of_tick,age->calendar_year_ordinal,current));
 const auto* seed=state.actor_start_context.initial_condition_seeds.find(origin->initial_condition_seed_ref.seed_key);const auto* seed_source=seed?std::get_if<CreatedConditionSeedSource>(&seed->source):nullptr;
 if(!seed||!seed_source||seed->seed_key!=origin->initial_condition_seed_ref.seed_key||seed->actor_id!=id||seed_source->creation_result_ref!=registration->registration_result_id||seed->hp!=100||seed->fatigue!=0||seed->injury!=0||!seed->episodes.empty()||!actor_foundation_rule_equal(seed->activity_limit_profile,catalog.rest_rule))fail("CreatedInitialConditionMismatch",id);
 const CharacterCreationAppearance* selected_appearance=nullptr;if(!actor->appearance)fail("CreatedAppearanceSourceRequired",id);for(const auto& row:definition.appearance_choices.rows())if(appearance(*actor->appearance,row.second)){if(selected_appearance)fail("CreatedAppearanceSourceAmbiguous",id);selected_appearance=&row.second;}if(!selected_appearance)fail("CreatedAppearanceSourceRequired",id);
 if(state.actor_start_context.actor_age_bases.find(id)||state.actor_start_context.actor_domain_bindings.find(id))fail("DynamicCreatedActorInAuthoredContext",id);
 return {true,{origin,registration,creation,seed,selected_appearance},{}};
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
// 仅为同候选的真实初始身体登记创建来源；重复或初态不符时拒绝。
Status record_created_character_origin_in_candidate(
    CommonStore &common, CharacterCreationV2Write &write,
    const FamilyCatalog &family, const CommonCatalog &shared,
    const StableId &registration_id, const StableId &id,
    const CharacterCreationDraft &draft) {
  try {
    auto now = take(write.clock.read_current_moment());
    if (!selected(write.catalog))
      fail("CreatedOriginSelectedSourceProfileRequired", id,
           ErrorKind::unsupported_capability);
    if (take(compare_world_moments(now, WorldMoment{})) ||
        registration_id.empty())
      fail("CreatedOriginInitialCandidateRequired", id);
    auto &records = write.state.character_origin_records;
    auto &heads =
        write.state.character_condition_extensions.actor_condition_heads;
    if (records.origins.find(id) ||
        records.registration_sources.find(registration_id) || heads.find(id))
      fail("CreatedOriginAlreadyApplied", id);
    const auto *actor = common.state().actors.find(id);
    if (!actor || !actor->alive || !actor->available || actor->hp != 100 ||
        actor->fatigue != 0 || actor->injury != 0)
      fail("CreatedOriginalInitialBodyRequired", id);
    const auto seed_id = take(common.next_id("initial_condition_seed"));
    RegistrationSourceV2 registration;
    registration.registration_result_id = registration_id;
    registration.actor_id = id;
    registration.registered_at.exact_moment = now;
    DeclaredAgeAtRegistration age;
    age.years = draft.age_years;
    age.as_of_tick = now.half_day_tick;
    age.calendar_profile_ref = write.calendar.profile_ref();
    age.calendar_year_ordinal =
        take(write.calendar.year_ordinal(now.half_day_tick));
    age.source_ref = CreationRegistrationRef{registration_id};
    registration.age_basis = std::move(age);
    registration.source =
        CreatedRegistrationSource{draft, creation_rule(write.catalog)};
    registration.initial_condition_seed_ref = {seed_id};
    InitialConditionSeed seed;
    seed.seed_key = seed_id;
    seed.actor_id = id;
    seed.source = CreatedConditionSeedSource{registration_id};
    seed.hp = 100;
    seed.activity_limit_profile = write.catalog.rest_rule;
    CharacterOriginRecordV2 origin;
    origin.actor_id = id;
    origin.origin_kind = CharacterOriginRecordV2::Kind::created;
    origin.origin_fact_ref = id;
    origin.created_at.exact_moment = now;
    origin.age_basis_ref.kind = CharacterAgeBasisRef::Kind::creation;
    origin.age_basis_ref.creation = {registration_id};
    origin.source = CreatedCharacterSource{registration_id};
    origin.initial_capability_basis =
        CreatedInitialCapabilityBasis{registration_id};
    origin.initial_condition_seed_ref = {seed_id};
    origin.provenance_policy_ref = write.catalog.provenance_policy;
    if (!records.registration_sources.insert(registration_id,
                                             std::move(registration)) ||
        !write.state.actor_start_context.initial_condition_seeds.insert(
            seed_id, std::move(seed)) ||
        !records.origins.insert(id, std::move(origin)) ||
        !heads.insert(id, ActorConditionHead{id, 0, {seed_id}, {}}))
      fail("DuplicateCreatedOriginSource", id);
    take(read_created_character_origin_v2(common, write.state, write.catalog,
                                          family, shared, id, now));
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
}
namespace hanmo::resident {
Outcome<CharacterInitialValuesV2> read_character_initial_values_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyCatalog* family,const CommonCatalog* shared,const StableId& actor,const WorldMoment& current,const BornCharacterSourceRead* born_sources){
 const auto* origin=state.character_origin_records.origins.find(actor);if(!origin)return {false,{},common_failure("CharacterOriginRequired",actor,ErrorKind::internal).error};
 if(origin->origin_kind==CharacterOriginRecordV2::Kind::created){if(!family||!shared)return {false,{},common_failure("CreatedOriginalCatalogRequired",actor,ErrorKind::unsupported_capability).error};auto actual=read_created_character_origin_v2(common,state,catalog,*family,*shared,actor,current);if(!actual.ok)return {false,{},actual.error};return {true,{origin,&actual.value.creation->draft.attributes,&actual.value.creation->draft.skills,actual.value.condition,true,true},{}};}
 if(origin->origin_kind==CharacterOriginRecordV2::Kind::born){if(!born_sources)return {false,{},common_failure("BornOriginalSourceReadRequired",actor,ErrorKind::unsupported_capability).error};auto actual=read_born_character_origin_v2(common,state,catalog,*born_sources,actor,current);if(!actual.ok)return {false,{},actual.error};return {true,{origin,&actual.value.allocation->base_attributes,&actual.value.allocation->initial_skill_xp,actual.value.condition,true,false},{}};}
 if(origin->origin_kind!=CharacterOriginRecordV2::Kind::authored)return {false,{},common_failure("DynamicInitialCapabilitySourceReaderRequired",actor,ErrorKind::unsupported_capability).error};
 auto valid=read_authored_character_origin_v2(common.state(),state,catalog,actor);if(!valid.ok)return {false,{},valid.error};const auto* definition=catalog.actors.find(actor);const auto* seed=state.actor_start_context.initial_condition_seeds.find(origin->initial_condition_seed_ref.seed_key);if(!definition||!seed)return {false,{},common_failure("AuthoredInitialCapabilitySourceRequired",actor).error};return {true,{origin,&definition->attributes,&definition->skills,seed,definition->alive,definition->available},{}};
}
Outcome<CharacterInitialValuesAtFactV2> read_character_initial_values_before_fact_v2(
 const CommonStore&common,const AuthoredActorFoundationState&state,const ActorFoundationCatalog&catalog,
 const FamilyCatalog*family,const CommonCatalog*shared,const StableId&actor,
 const SourceRef&boundary,const WorldMoment&at,const WorldMoment&now,const BornCharacterSourceRead*born,CharacterAvailabilityProvenanceSink*provenance){
 auto invalid=[&](const char*code)->Outcome<CharacterInitialValuesAtFactV2>{return {false,{},common_failure(code,actor,ErrorKind::internal).error};};
 auto temporal=compare_world_moments(at,now);if(!temporal.ok)return {false,{},temporal.error};if(temporal.value>0)return invalid("CharacterPresenceFutureBoundary");
 const auto cut=read_actual_common_fact_position(common,boundary);
 if(!cut.ok)return invalid("CharacterPresenceActualBoundaryRequired");
 auto initial=read_character_initial_values_v2(common,state,catalog,family,shared,actor,now,born);if(!initial.ok)return {false,{},initial.error};
 const auto*origin=initial.value.origin;if(!origin||origin->created_at.kind!=ActorFoundationAt::Kind::exact_moment)return invalid("CharacterPresenceExactOriginRequired");
 auto occurred=compare_world_moments(origin->created_at.exact_moment,at);if(!occurred.ok)return {false,{},occurred.error};
 bool present=occurred.value<=0;const FactRecord* creation=nullptr;
 if(origin->origin_kind==CharacterOriginRecordV2::Kind::created||origin->origin_kind==CharacterOriginRecordV2::Kind::born){
  creation=common.state().facts.find(actor);
  const auto domain=origin->origin_kind==CharacterOriginRecordV2::Kind::born?"family_birth":"family";
  if(!creation||creation->id!=actor||creation->domain!=domain||creation->kind!="actor_creation")return invalid("CharacterPresenceCreationOrderRequired");
  const auto position=read_actual_common_fact_position(common,*creation);if(!position.ok)return invalid("CharacterPresenceCreationOrderRequired");
  // A true earlier/later M must agree with append order too. Same-M identity
  // uses the actual cut, not a numeric ID or today's actor existence.
  if((occurred.value<0&&position.value>=cut.value)||(occurred.value>0&&position.value<=cut.value))return invalid("CharacterPresenceCreationTimeOrderMismatch");
  present=position.value<cut.value;
 }
 if(provenance){
  // Emit only actual checked rows. Unregistered owned identities remain typed,
  // rather than deriving a source schema from their physical module/type name.
  auto valid=provenance->original_origin(*origin,present);if(!valid.ok)return {false,{},std::move(valid.error)};
  auto seed_schema=read_initial_condition_seed_schema(catalog);if(!seed_schema.ok)return {false,{},std::move(seed_schema.error)};
  valid=provenance->append(CharacterAvailabilityProvenanceSlot::initial_condition_seed,{"actor_foundation","initial_condition_seed",origin->initial_condition_seed_ref.seed_key},seed_schema.value,present);if(!valid.ok)return {false,{},std::move(valid.error)};
  if(origin->origin_kind==CharacterOriginRecordV2::Kind::created){
   const auto* source=std::get_if<CreatedCharacterSource>(&origin->source);const auto* registration=source?state.character_origin_records.registration_sources.find(source->creation_result_ref):nullptr;
   if(!creation||!registration)return invalid("CreatedRegistrationSourceMismatch");
   // The existing original created-placement reader establishes this exact /2.
   valid=provenance->append(CharacterAvailabilityProvenanceSlot::created_creation,{creation->domain,creation->kind,creation->id},2,present);if(!valid.ok)return {false,{},std::move(valid.error)};
   valid=provenance->created_registration(*registration,present);if(!valid.ok)return {false,{},std::move(valid.error)};
  }else if(origin->origin_kind==CharacterOriginRecordV2::Kind::born){
   const auto* source=std::get_if<BornCharacterSource>(&origin->source);const auto* basis=std::get_if<BirthInitialCapabilityBasis>(&origin->initial_capability_basis);const auto* allocation=basis?state.character_origin_records.allocation_sources.find(basis->allocation_source_ref):nullptr;
   if(!creation||!source||!allocation)return invalid("BornOriginSourceMismatch");
   valid=provenance->birth_allocation(*allocation,present);if(!valid.ok)return {false,{},std::move(valid.error)};
   valid=provenance->original_birth(source->birth_record_ref,present);if(!valid.ok)return {false,{},std::move(valid.error)};
   valid=provenance->born_creation(*creation,present);if(!valid.ok)return {false,{},std::move(valid.error)};
  }
 }
 return {true,{initial.value,present},{}};
}
Outcome<CharacterInitialValuesAtFactV2> read_character_initial_values_at_applied_prefix_v2(
 const CommonStore&common,const AuthoredActorFoundationState&state,const ActorFoundationCatalog&catalog,
 const FamilyCatalog*family,const CommonCatalog*shared,const StableId&actor,
 const CommonActualFactPrefix&nested,const CommonActualFactPrefix&outer,
 const WorldMoment&outer_at,const WorldMoment&now,const BornCharacterSourceRead*born){
 auto invalid=[&](const char*code)->Outcome<CharacterInitialValuesAtFactV2>{return {false,{},common_failure(code,actor,ErrorKind::internal).error};};
 auto nested_cut=read_actual_common_fact_prefix(common,nested);if(!nested_cut.ok)return {false,{},nested_cut.error};
 auto outer_cut=read_actual_common_fact_prefix(common,outer);if(!outer_cut.ok)return {false,{},outer_cut.error};
 if(nested_cut.value>outer_cut.value)return invalid("CharacterAppliedPresenceNestedPrefixAfterOuter");
 auto temporal=compare_world_moments(outer_at,now);if(!temporal.ok)return {false,{},temporal.error};if(temporal.value>0)return invalid("CharacterAppliedPresenceFutureOuter");
 auto initial=read_character_initial_values_v2(common,state,catalog,family,shared,actor,now,born);if(!initial.ok)return {false,{},initial.error};
 const auto*origin=initial.value.origin;if(!origin||origin->created_at.kind!=ActorFoundationAt::Kind::exact_moment)return invalid("CharacterPresenceExactOriginRequired");
 auto current=compare_world_moments(origin->created_at.exact_moment,now);if(!current.ok)return {false,{},current.error};if(current.value>0)return invalid("CharacterAppliedPresenceFutureOrigin");
 bool present=true;
 if(origin->origin_kind==CharacterOriginRecordV2::Kind::created||origin->origin_kind==CharacterOriginRecordV2::Kind::born){
  const auto*creation=common.state().facts.find(actor);const auto domain=origin->origin_kind==CharacterOriginRecordV2::Kind::born?"family_birth":"family";
  if(!creation||creation->id!=actor||creation->domain!=domain||creation->kind!="actor_creation")return invalid("CharacterPresenceCreationOrderRequired");
  auto position=read_actual_common_fact_position(common,*creation);if(!position.ok)return {false,{},position.error};
  auto compared=compare_world_moments(origin->created_at.exact_moment,outer_at);if(!compared.ok)return {false,{},compared.error};
  if((position.value<outer_cut.value&&compared.value>0)||(position.value>=outer_cut.value&&compared.value<0))return invalid("CharacterAppliedPresenceCreationTimeOrderMismatch");
  present=position.value<nested_cut.value;
 }else{
  if(origin->origin_kind!=CharacterOriginRecordV2::Kind::authored)return invalid("CharacterAppliedPresenceOriginKindRequired");
  auto baseline=compare_world_moments(origin->created_at.exact_moment,outer_at);if(!baseline.ok)return {false,{},baseline.error};
  if(baseline.value>0)return invalid("CharacterAppliedPresenceOuterBeforeAuthoredOrigin");
 }
 // Authored initial values already passed their true zero-baseline/catalog
 // reader. A missing dynamic creation never becomes an authored baseline.
 return {true,{initial.value,present},{}};
}
Outcome<CharacterInitialPlacementV2> read_created_character_initial_placement_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyCatalog& family,const CommonCatalog& shared,const StableId& actor,const WorldMoment& current){auto source=read_created_character_origin_v2(common,state,catalog,family,shared,actor,current);if(!source.ok)return {false,{},source.error};const auto* place=shared.places.find(family.character_creation->starting_place_id);if(!place)return {false,{},common_failure("CreatedOriginalPlacementRequired",actor).error};return {true,{actor,SettlementLocation{place->settlement_id,place->id},source.value.origin->created_at.exact_moment,{"family","actor_creation",actor},2},{}};}
}

namespace hanmo::resident {
Outcome<CharacterCreationMode> read_character_creation_mode_v2(const CommonStore& common,const AuthoredActorFoundationState& state,const ActorFoundationCatalog& catalog,const FamilyState& family,const FamilyCatalog& definition,const CommonCatalog& shared,const WorldMoment& now){
 if(!selected(catalog))return {false,{},common_failure("CreatedOriginSelectedSourceProfileRequired",{},ErrorKind::unsupported_capability).error};
 if(definition.founder_selection!=FamilyFounderSelection::created_character){for(const auto& row:state.character_origin_records.registration_sources.rows())if(row.second.registration_kind==RegistrationSourceV2::Kind::created)return {false,{},common_failure("UnexpectedCreatedFounderSource",row.first).error};return {true,CharacterCreationMode::standard,{}};}
 auto original=read_created_character_origin_v2(common,state,catalog,definition,shared,family.founder_actor_id,now);if(!original.ok)return {false,{},original.error};std::size_t count=0;for(const auto& row:state.character_origin_records.registration_sources.rows())if(row.second.registration_kind==RegistrationSourceV2::Kind::created)++count;if(count!=1)return {false,{},common_failure("CreatedFounderSourceCoverageMismatch").error};return {true,original.value.creation->draft.mode,{}};
}
}
