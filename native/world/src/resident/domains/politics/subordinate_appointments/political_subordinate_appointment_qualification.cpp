// 先核本域实际载荷的候选/时刻，再借已完整验证人物身份来源；无正功勋历史不授正阈值资格。
#include "political_subordinate_appointment_qualification.hpp"
namespace hanmo::resident {namespace {
Outcome<PoliticalSubordinateQualificationSource> projected(const CommonStore& c,const StableId& actor,Integer tick,const StableId& fact,const Outcome<GrowthAttributeOrigin>& source){if(!source.ok)return {false,{},source.error};const auto& s=source.value;const auto& identity=c.state().identity;const auto* current=c.state().actors.find(actor);if(!current||current->merit<0||std::tie(s.source_identity.world_id,s.source_identity.execution_epoch,s.source_identity.revision,s.source_identity.tick)!=std::tie(identity.world_id,identity.execution_epoch,identity.revision,identity.tick)||s.actor_id!=actor||s.before_fact_id!=fact||s.observed_tick!=tick||s.created_tick>tick)return {false,{},common_failure("PoliticalSubordinateQualificationSourceMismatch",actor).error};return {true,{identity,actor,fact,tick,s.attributes.administration,0,false},{}};}
}
Outcome<PoliticalSubordinateQualificationSource> PoliticalSubordinateQualificationAccess::subordinate_office_qualification(const StableId& actor)const{return projected(common_,actor,common_.tick(),{},read_growth_attribute_origin(common_,origins_,actor));}
Outcome<PoliticalSubordinateQualificationSource> PoliticalSubordinateQualificationAccess::subordinate_office_qualification_before(const StableId& actor,Integer tick,const StableId& fact)const{
 const auto fail=[&](){return Outcome<PoliticalSubordinateQualificationSource>{false,{},common_failure("PoliticalSubordinateQualificationBoundaryRequired",fact).error};};const auto* h=common_.state().facts.find(fact);if(!h||!political_subordinate_appointment_fact_sequence(common_,fact))return fail();bool exact=false;
 if(const auto* o=state_.offers.find(fact))exact=o->id==fact&&o->tick==tick&&o->candidate_actor_id==actor&&h->kind=="political_subordinate_appointment_offered";
 else if(const auto* a=state_.answers.find(fact))exact=a->id==fact&&a->tick==tick&&a->actor_id==actor&&a->accepted&&h->kind=="political_subordinate_appointment_accepted";
 else if(const auto* a=state_.arrivals.find(fact))exact=a->id==fact&&a->tick==tick&&a->actor_id==actor&&h->kind=="political_subordinate_appointment_arrived";
 if(!exact)return fail();return projected(common_,actor,tick,fact,read_growth_attribute_origin_before_fact(common_,origins_,actor,tick,fact));
}
}
