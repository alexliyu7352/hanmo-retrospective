#include "born_character_source.hpp"
#include "family_birth_creation.hpp"
#include "resident/domains/growth/growth_skill_api.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
Status broken(const StableId& code,const StableId& id={}){return common_failure(code,id,ErrorKind::internal);}
bool before(const CommonStore& c,const StableId& first,const StableId& last){const auto& ids=c.state().facts.order();const auto a=std::find(ids.begin(),ids.end(),first),b=std::find(ids.begin(),ids.end(),last);return a!=ids.end()&&b!=ids.end()&&a<b;}
bool allocated(const CommonStore& c,const StableId& id,std::string_view prefix){if(id.compare(0,prefix.size(),prefix)!=0)return false;const auto raw=std::string_view(id).substr(prefix.size());Integer n=0;const auto read=std::from_chars(raw.data(),raw.data()+raw.size(),n);return read.ec==std::errc{}&&read.ptr==raw.data()+raw.size()&&n>0&&n<c.state().next_instance_id&&raw==std::to_string(n);}
Outcome<InnateParentSource> parent(const BornCharacterSourceContext& x,const StableId& actor,const StableId& family,const StableId* cut){
 if(!x.common.actor(actor)||!x.lineage.endowment.families.find(family))return {false,{},broken("BirthInnateParentSourceRequired",actor).error};
 InnateParentSource out;out.actor_id=actor;
 if(std::find(x.common_catalog.actor_ids.begin(),x.common_catalog.actor_ids.end(),actor)!=x.common_catalog.actor_ids.end()){
  if(const auto* levels=x.lineage.original_innate_levels.find(actor))if(const auto* level=levels->find(family))out.level=*level;return {true,std::move(out),{}};
 }
 const FamilyBirthRecord* actual=nullptr;const StableId* fact=nullptr;
 for(const auto& row:x.births.births.rows())if(row.second.actor_id==actor){if(actual)return {false,{},broken("DuplicateBiologicalActor",actor).error};actual=&row.second;fact=&row.first;}
 const auto* creation=x.common.state().facts.find(actor);
 if(!creation||creation->id!=actor||creation->kind!="actor_creation"||(cut&&!before(x.common,actor,*cut)))return {false,{},broken("BirthParentCreationBeforeSourceRequired",actor).error};
 if(actual){if(creation->domain!="family_birth"||!fact||!before(x.common,*fact,actor)||(cut&&!before(x.common,*fact,*cut)))return {false,{},broken("BirthParentCreationBeforeSourceRequired",actor).error};const auto* result=actual->innate.find(family);if(!result)return {false,{},broken("BirthParentInnateResultRequired",actor).error};out.level=result->level;}
 else if(creation->domain!="family")return {false,{},common_failure("BirthParentInitialInnateSourceRequired",actor,ErrorKind::unsupported_capability).error};
 // Original Created/Named initialization has no supplied congenital family
 // level here. Unknown stays null; current XP never supplies an innate level.
 return {true,std::move(out),{}};
}
Status selected(const BornCharacterSourceContext& x){if(!x.catalog.enabled||x.catalog.selected_version!=4||!x.lineage.enabled||!x.access.people||!x.access.people->bound_to(x.common)||!x.access.lineage||x.access.lineage!=&x.lineage||x.interactions.common.get()!=&x.common_catalog)return common_failure("BornCharacterSource4Required",{},ErrorKind::unsupported_capability);auto valid=validate_birth_arrangement_catalog(x.catalog,x.common_catalog,x.interactions,x.family_catalog);return valid.ok?validate_family_lineage_catalog(x.common,x.common_catalog,x.lineage):valid;}
}
Outcome<BirthPlacementArrangementView> read_birth_placement_arrangement_at_due(const BornCharacterSourceContext& x,const StableId& delivery,const StableId& accepted_id,const ReadStamp& stamp,const ReadStampAuthority& stamps,const DomainReadVersionSource& versions){
 auto valid=selected(x);if(!valid.ok)return {false,{},valid.error};
 auto source=read_scheduled_birth_source(x.common,x.interactions,x.family_catalog,x.catalog,x.births,x.access,accepted_id);if(!source.ok)return {false,{},source.error};
 const auto* accepted=x.births.replies.find(accepted_id);const auto* head=x.common.state().facts.find(accepted_id);
 if(!accepted||!accepted->confirmation||!head||accepted->confirmation->terms.gestating_actor_id!=delivery)return {false,{},broken("BirthDeliveryArrangementMismatch",accepted_id).error};
 auto now=x.access.people->read_current_moment();if(!now.ok)return {false,{},now.error};auto due=compare_world_moments(now.value,source.value.occurrence);if(!due.ok)return {false,{},due.error};if(due.value!=0)return {false,{},broken("BirthPlacementAtActualDueRequired",accepted_id).error};
 // A schedule is retained after fulfilment. It cannot authorize a second
 // query/adoption, including while a different due is being completed.
 for(const auto& row:x.births.births.rows())if(row.second.accepted_id==accepted_id)return {false,{},broken("BirthAlreadyTerminalUseOriginalResult",accepted_id).error};
 for(const auto& row:x.births.closures.rows())if(row.second.accepted_id==accepted_id)return {false,{},broken("BirthAlreadyTerminalUseOriginalResult",accepted_id).error};
 for(const auto& row:x.births.birth_due_executions.rows())if(row.second.source.arrangement_id==accepted_id)return {false,{},broken("BirthAlreadyTerminalUseOriginalResult",accepted_id).error};
 valid=validate_birth_care_reservation(x.common,accepted->confirmation->terms,accepted_id,accepted->reserved_food);if(!valid.ok)return {false,{},valid.error};
 valid=stamps.validate(stamp,x.common.state().identity,ReadMode::phase_current,x.common,versions);if(!valid.ok)return {false,{},valid.error};
 auto stamp_at=compare_world_moments(stamp.span().to,now.value);if(!stamp_at.ok)return {false,{},stamp_at.error};bool actor_read=false;for(const auto& row:stamp.resources())if(row.resource.kind==ResourceKey::Kind::actor&&row.resource.id==delivery)actor_read=true;
 if(stamp.phase()!="birth_due"||stamp_at.value!=0||!actor_read||std::find(stamp.source_ids().begin(),stamp.source_ids().end(),accepted_id)==stamp.source_ids().end())return {false,{},broken("BirthDueReadStampMismatch",accepted_id).error};
 auto presence=x.access.people->read_current_presence(delivery);if(!presence.ok)return {false,{},presence.error};if(!presence.value.alive)return {false,{},common_failure("BirthDeliveryActorDeadUseOriginalCancellation",delivery).error};
 return {true,{accepted,head,std::move(source.value)},{}};
}
Status validate_born_character_source_structure(const BornCharacterSourceContext& x){
 auto valid=selected(x);if(!valid.ok)return valid;auto now=x.access.people->read_current_moment();if(!now.ok)return {false,{},now.error};
 for(const auto& row:x.births.births.rows()){
  const auto& birth=row.second;const auto* head=x.common.state().facts.find(row.first);const auto* accepted=x.births.replies.find(birth.accepted_id);const auto* accepted_head=x.common.state().facts.find(birth.accepted_id);const auto* creation=x.common.state().facts.find(birth.actor_id);const auto* actor=x.common.state().actors.find(birth.actor_id);
  if(std::find(x.common_catalog.actor_ids.begin(),x.common_catalog.actor_ids.end(),birth.actor_id)!=x.common_catalog.actor_ids.end()||!allocated(x.common,row.first,"family_birth:")||!allocated(x.common,birth.actor_id,"actor:")||!head||head->id!=row.first||head->domain!="family_birth"||head->kind!="family_birth_recorded"||birth.kind!=head->kind||!accepted||accepted->decision!="accepted"||!accepted->confirmation||!accepted_head||accepted_head->id!=birth.accepted_id||accepted_head->domain!="family_birth"||accepted_head->kind!="family_birth_accepted"||!before(x.common,birth.accepted_id,row.first)||!creation||creation->domain!="family_birth"||creation->kind!="actor_creation"||creation->id!=birth.actor_id||!before(x.common,row.first,birth.actor_id))return broken("BornOriginalSourceIdentityMismatch",row.first);
  const auto& original=*accepted->confirmation;const auto& terms=original.terms;auto moment=compare_world_moments(WorldMoment{birth.tick,{}},now.value);if(!moment.ok)return {false,{},moment.error};
  if(birth.tick!=accepted->due_tick||birth.tick<0||moment.value>0||birth.revision<accepted->revision||birth.revision>x.common.revision()||birth.prototype_id!=terms.child_prototype_id||birth.biological_parent_ids.size()!=2||birth.biological_parent_ids[0]!=original.speaker_actor_id||birth.biological_parent_ids[1]!=original.listener_actor_id||birth.biological_parent_ids[0]==birth.biological_parent_ids[1]||birth.biological_parent_ids[0]==birth.actor_id||birth.biological_parent_ids[1]==birth.actor_id||!birth.place_id.empty()||!allocated(x.common,birth.allocation_source_ref,"initial_capability_allocation:"))return broken("BornOriginalTermsMismatch",row.first);
  if(!actor||actor->id!=birth.actor_id||actor->prototype_id!=birth.prototype_id||actor->display_name!=birth.display_name||birth.display_name.empty()||birth.display_name.size()>256||actor->birth_tick!=birth.tick||actor->created_tick!=birth.tick||actor->wallet_id!="personal:"+birth.actor_id||actor->inventory_id!="bag:"+birth.actor_id)return broken("BornOriginalActorMismatch",row.first);
  const auto* prototype=x.family_catalog.prototypes.find(birth.prototype_id);if(!prototype||prototype->historical)return broken("BornOriginalPrototypeRequired",row.first);valid=family_prototype_skill_eligibility(x.family_catalog,*prototype);if(!valid.ok)return valid;for(const auto& skill:growth_skill_ids(x.family_catalog.extended_skills))if(growth_skill_xp(prototype->skills,skill)!=0)return broken("BirthCannotInheritLearnedSkills",skill);if(!actor->appearance)return broken("BirthAppearanceSourceRequired",row.first);valid=validate_original_birth_appearance(*prototype,*actor->appearance);if(!valid.ok)return valid;
  for(const auto& family:x.lineage.endowment.families.rows()){
   auto first=parent(x,original.speaker_actor_id,family.first,&row.first),second=parent(x,original.listener_actor_id,family.first,&row.first);if(!first.ok)return {false,{},first.error};if(!second.ok)return {false,{},second.error};const auto* result=birth.innate.find(family.first);if(!result||result->first.actor_id!=first.value.actor_id||result->first.level!=first.value.level||result->second.actor_id!=second.value.actor_id||result->second.level!=second.value.level)return broken("BirthInnateParentSnapshotMismatch",row.first);
  }
  valid=validate_birth_random_receipt(x.common,x.lineage,birth);if(!valid.ok)return valid;
  for(const auto& other:x.births.births.rows())if(other.first!=row.first&&(other.second.actor_id==birth.actor_id||other.second.accepted_id==birth.accepted_id||other.second.allocation_source_ref==birth.allocation_source_ref))return broken("DuplicateBornOriginalSource",row.first);
  for(const auto& closure:x.births.closures.rows())if(closure.second.accepted_id==birth.accepted_id&&before(x.common,closure.first,row.first))return broken("BirthAfterArrangementClosed",row.first);
 }
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="family_birth"){
  if(row.second.kind=="family_birth_recorded"&&!x.births.births.find(row.first))return broken("MissingBiologicalBirthPayload",row.first);
  if(row.second.kind=="actor_creation"){std::size_t count=0;for(const auto& birth:x.births.births.rows())if(birth.second.actor_id==row.first)++count;if(count!=1)return broken("BiologicalCreationPayloadMismatch",row.first);}
 }
 return common_success();
}
Outcome<BornCharacterSourceView> read_born_character_source(const BornCharacterSourceContext& x,const BirthFactRef& ref){
 auto valid=validate_born_character_source_structure(x);if(!valid.ok)return {false,{},valid.error};const auto* birth=x.births.births.find(ref.birth_fact_id);if(!birth)return {false,{},broken("BornOriginalSourceRequired",ref.birth_fact_id).error};
 auto schedule=read_scheduled_birth_source(x.common,x.interactions,x.family_catalog,x.catalog,x.births,x.access,birth->accepted_id);if(!schedule.ok)return {false,{},schedule.error};if(birth->tick!=schedule.value.due_tick)return {false,{},broken("BornOriginalDueMismatch",ref.birth_fact_id).error};
 const auto* accepted=x.births.replies.find(birth->accepted_id);const auto& delivery=accepted->confirmation->terms.gestating_actor_id;auto alive=x.access.people->read_presence_before_original_fact(delivery,birth->tick,{"family_birth","family_birth_recorded",ref.birth_fact_id});if(!alive.ok)return {false,{},alive.error};if(!alive.value.alive)return {false,{},broken("BirthDeliveryActorAlreadyDead",ref.birth_fact_id).error};
 return {true,{birth,accepted,x.family_catalog.prototypes.find(birth->prototype_id),x.common.state().facts.find(ref.birth_fact_id),schedule.value.occurrence},{}};
}
Outcome<InnateParentSource> read_born_original_innate_parent(const BornCharacterSourceContext& x,const StableId& actor,const StableId& family){auto valid=validate_born_character_source_structure(x);if(!valid.ok)return {false,{},valid.error};return parent(x,actor,family,nullptr);}
}
