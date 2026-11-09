#pragma once
#include "repair_service_agreement.hpp"
#include "resident/domains/commerce/service_agreement.hpp"
#include "repair_service_internal.hpp"
#include "repair_service_rules.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident::repair_service_agreement_detail {
using namespace repair_service_detail;
WorldMoment context(const RepairServiceAgreementReadContext&);
void inspection(const RepairServiceAgreementReadContext&,const RepairServiceQuotedTerms&,const RepairServiceAcceptanceInspection&,const MomentOwnerSource& contact,const WorldMoment&,const SourceRef*cut);
void validate_adopted_agreement(const RepairServiceAgreementReadContext&,const RepairServiceOfferSource&,const RepairServiceAssentSource&,const WorldMoment&,const FactRecord*actual_contract=nullptr);
const RepairServiceOfferSource& offer(const RepairServiceAgreementReadContext&,const MomentOwnerSource&);
const RepairServiceAssentSource& assent(const RepairServiceAgreementReadContext&,const MomentOwnerSource&);
LifeActorRoute route(const RepairServiceAgreementReadContext&,const StableId&actor,const MomentOwnerSource&actual_source,const MomentOwnerSource&command,const WorldMoment&);
bool same_route(const LifeActorRoute&,const LifeActorRoute&);
const RepairServiceCommandReceipt& command(const RepairServiceAgreementReadContext&,const StableId&actor,const StableId&kind,const MomentOwnerSource&actual,const MomentOwnerSource&command,const WorldMoment&);
Status original_route_pins(const RepairServiceAgreementReadContext&,const LifeActorRoute&,const StableId&,RepairServiceAgreementPinSink&);
bool matches_accepted_terms(const RepairServiceOfferSource&,const RepairServiceAssentSource&,const RepairServiceContractTerms&);
RepairServiceContractTerms accepted_terms(const RepairServiceOfferSource&,const RepairServiceAssentSource&);
}
