// Actual Repair quote/assent sources, independent of fee settlement and FSL.
#pragma once
#include "repair_service_command_model.hpp"
#include "resident/domains/craft/item_condition_head.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
#include "resident/domains/interaction/face_to_face_contact_source.hpp"
namespace hanmo::resident {
struct DirectorState;struct DirectorCatalog;
class RepairServiceInspectionOriginalRead {public:virtual ~RepairServiceInspectionOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept=0;
 // Independent original title/owner/payer/receiver/custody authority, genuine
 // selected item and workstation/material profile. A snapshot is not title.
 // A nonnull cut is the actual offer or assent head, never a caller ordinal.
 virtual Status validate_original_repair_inspection(const RepairServiceQuotedTerms&,
  const RepairServiceAcceptanceInspection&,const WorldMoment&,const SourceRef* actual_cut)const=0;
};
struct RepairServiceAgreementReadContext {
 const CommonStore& common;const ItemInstancesState& items;
 const RepairPolicyCatalog& policies;const RepairServiceRuleCatalog& rules;
 const ItemConditionReadContext& conditions;const RepairServiceInspectionOriginalRead& inspections;
 const InteractionState& interactions;const InteractionCatalog& interaction_catalog;
 const ProfileControlEligibilitySources& controls;const CurrentMomentRead& clock;
 const DirectorState* director=nullptr;const DirectorCatalog* director_catalog=nullptr;
};
Outcome<const RepairServiceOfferSource*> read_repair_service_offer(const RepairServiceAgreementReadContext&,const MomentOwnerSource&);
Outcome<const RepairServiceAssentSource*> read_repair_service_assent(const RepairServiceAgreementReadContext&,const MomentOwnerSource&);
// Reads only quote/assent and original command/title/contact sources. No full
// Repair, finance or Director execution reader is called recursively.
class NativeRepairServiceAgreementRead {
 const RepairServiceAgreementReadContext& x_;
public:explicit NativeRepairServiceAgreementRead(const RepairServiceAgreementReadContext&x):x_(x){}
 bool bound_to(const CommonStore&,const ItemInstancesState&)const noexcept;
 Status validate_original_repair_service_rules(const RepairServiceContractTerms&,const RepairPolicy&)const;
 Status validate_original_repair_service_acceptance(const RepairServiceContractTerms&,const WorldMoment&,const RepairServiceContractSource*)const;
};
Outcome<WorldMoment> read_repair_service_agreement_moment(const RepairServiceAgreementReadContext&,const MomentOwnerSource&);
Status validate_repair_service_agreements(const RepairServiceAgreementReadContext&);
// Dynamic player control is emitted through source() with its original
// payload schema. This callback retains only authored control / NPC roots.
class RepairServiceAgreementPinSink:public RepairServiceSourcePinSink {public:virtual Status original_actor_route(const LifeActorRoute&,const StableId& actual_actor)=0;};
Status visit_repair_service_agreement_pins(const RepairServiceAgreementReadContext&,const MomentOwnerSource&,RepairServiceAgreementPinSink&);
}
