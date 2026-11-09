// The original inheritance decision borrows actual LIFE4, age and personally
// acquired knowledge. No notice, grant, learned fact or death is created here.
#pragma once
#include "family_continuation_people4.hpp"
#include "resident/domains/social/known_death_receipt.hpp"
#include "family_life_exact_codec_detail.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
namespace hanmo::resident {
struct FamilyInheritanceKnowledge2 {
 ActorKnownDeathReceiptRef receipt;WorldMoment learned_at;LifeAgeBasisRef beneficiary_age;
};
template<class C>Status family_inheritance_knowledge2_fields(C& c,FamilyInheritanceKnowledge2& v){
 c.object("receipt",[&](auto& a){return known_death_receipt_codec::fields(a,v.receipt);});
 c.object("learned_at",[&](auto& a){return family_life4_codec::moment(a,v.learned_at);});
 return c.object("beneficiary_age",[&](auto& a){return life_age_basis_fields(a,v.beneficiary_age);});
}
struct FamilyInheritanceSpatialContext2 {
 const CommonCatalog& common_catalog;const MovementExactState& movement;
 const SpatialCarrierState& carriers;const TransportRegistrationState& registrations;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& foundation_catalog;
 const SpatialCarrierSources& original;const FamilyCatalog& family_catalog;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const BirthPlacementRead* births=nullptr;
 const MilitaryActionPlacementRead* military_actions=nullptr;
};
class FamilyInheritanceSources2 final {
 const ActorReportKnowledgeContext& knowledge_;const FamilyDeathPolicy& policy_;
 const FamilyDeathState& deaths_;const FamilyLifeCauseSources4& causes_;
 const FamilyContinuationPeopleRead4& people_;const FamilyInheritanceSpatialContext2* spatial_=nullptr;
public:
 FamilyInheritanceSources2(const ActorReportKnowledgeContext& k,const FamilyDeathPolicy& p,const FamilyDeathState& d,const FamilyLifeCauseSources4& s,const FamilyContinuationPeopleRead4& people,const FamilyInheritanceSpatialContext2* spatial=nullptr):knowledge_(k),policy_(p),deaths_(d),causes_(s),people_(people),spatial_(spatial){}
 bool bound_to(const CommonStore& c,const FamilyDeathState& d)const noexcept{return &knowledge_.common==&c&&&deaths_==&d&&people_.bound_to(c,d);}
 const FamilyContinuationPeopleRead4& people()const noexcept{return people_;}
 Outcome<WorldMoment> current_moment()const;
 Status validate_original_physical_contact(const StableId& donor,const StableId& beneficiary,const StableId& place,const StableId& settlement,const WorldMoment&,const SourceRef* original_settlement=nullptr)const;
 Outcome<FamilyInheritanceKnowledge2> capture_personal_death_knowledge(const StableId& actor,const ActorKnownDeathReceiptRef&)const;
 Status validate_personal_death_knowledge(const StableId& actor,const FamilyInheritanceKnowledge2&,const WorldMoment& actual_at,const SourceRef* original_acceptance=nullptr)const;
};
Status validate_inheritance_exact_moment(const FamilyInheritanceSources2&,Integer projected_tick,const std::optional<WorldMoment>&);
bool same_inheritance_knowledge2(const FamilyInheritanceKnowledge2&,const FamilyInheritanceKnowledge2&);
}
