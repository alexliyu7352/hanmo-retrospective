#pragma once
#include <array>
#include "office_exact.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
namespace hanmo::resident::office_exact_detail {
using namespace movement_detail;
inline SourceRef start_ref(const StableId& id){return {"military","military_action",id};}
inline SourceRef completed_ref(const StableId& id){return {"military","military_action_completed",id};}
inline void bound(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactOfficeReadContext& x){if(!x.original.bound_to(c,m,s))fail("CrossCandidateExactOfficeSources");}
inline void bound(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ExactOfficeSourceReadContext& x){if(x.selected_action_time!=2||&x.common!=&c||&x.military!=&m||&x.actions!=&s)fail("CrossCandidateExactOfficeSourceRoots");}
inline void header(const CommonStore& c,const StableId& id){const auto* f=c.state().facts.find(id);if(!f||f->id!=id||f->kind!="military_action"||f->domain!="military")fail("ExactOfficeOriginalHeaderRequired",id);}
inline const DynamicMilitaryActionFact2& fact(const MilitaryActionTimeState& s,const StableId& id){const auto* f=s.dynamic_action_facts.find(id);if(!f||f->id!=id||f->command!="office_accept"||!std::holds_alternative<OfficeTerms>(f->original_terms))fail("ExactOfficeFactRequired",id);return *f;}
inline DynamicMilitaryActionFact2& fact(MilitaryActionTimeState& s,const StableId& id){auto* f=s.dynamic_action_facts.find(id);if(!f||f->id!=id||f->command!="office_accept"||!std::holds_alternative<OfficeTerms>(f->original_terms))fail("ExactOfficeFactRequired",id);return *f;}
inline std::array<const StableId*,2> participant_refs(const DynamicMilitaryActionFact2& f){const auto& t=std::get<OfficeTerms>(f.original_terms);return {&t.actor_id,&t.provider_actor_id};}
inline WorldMoment due(const DynamicMilitaryActionFact2& f,ExactArithmeticBudget b){return take(add_world_duration(f.started_at,HalfDayDuration{integer(1,b)},b));}
inline MilitaryEffect receipt(const DynamicMilitaryActionFact2& f,bool replay=false){MilitaryEffect out;out.action_id=f.id;out.success=f.success;out.already_completed=replay;out.code=f.success?"":"MilitaryActionInterrupted";if(f.result){out.office_id=f.result->office_id;out.army_id=f.result->army_id;out.order_id=f.result->order_id;out.details_ok=f.result->ok;}return out;}
// Only a successful completion has the later positive role recheck. Failed
// results remain legitimate without that later positive proof.
void validate_original_role_source(const CommonStore&,const ExactOfficeReadContext&,const DynamicMilitaryActionFact2&,bool completion,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* role_output=nullptr);
void validate_original_role_source(const CommonStore&,const ExactOfficeSourceReadContext&,const DynamicMilitaryActionFact2&,bool completion,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* role_output=nullptr);
// Same original identity kernel, without S0 or the three-method provider.
void validate_original_office_identity(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactOfficeSourceReadContext&,const DynamicMilitaryActionFact2&);
void validate_completion_header(const CommonStore&,const MilitaryActionTimeState&,const DynamicMilitaryActionFact2&);
const DynamicMilitaryActionFact2& source_local_office(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeReadContext&,const SourceRef&,Integer,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role=nullptr);
const DynamicMilitaryActionFact2& source_local_office(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactOfficeSourceReadContext&,const SourceRef&,Integer,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* start_role=nullptr,
 std::optional<OriginalMilitaryInteractionRoleBeforePrefix>* successful_completion_role=nullptr);
void validate_original_interaction_availability(const CommonStore&,const MilitaryState&,const ExactOfficeReadContext&,const DynamicMilitaryActionFact2&,bool completion,CharacterAvailabilityProvenanceSink* retained_body=nullptr);
Status validate_fact_history(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ExactOfficeReadContext&,const DynamicMilitaryActionFact2&,const ActionElapsedState&,bool,CharacterAvailabilityProvenanceSink* retained_body=nullptr);
}
