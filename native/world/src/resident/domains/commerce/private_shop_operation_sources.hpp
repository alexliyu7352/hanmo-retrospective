// Genuine original acceptance and S1 source cut. This is not full operation/P29 validation.
#pragma once
#include "private_shop_operation_sources_model.hpp"
#include "private_shop_capability_model.hpp"
#include "private_shop_operation.hpp"
#include "resident/domains/local_economy/household_purchase_contract.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include <memory>
namespace hanmo::resident {
class FamilyLifeHistoryRead4;
struct PrivateShopOperationSourceContext {
 const PrivateShopContext& shops;
 const PrivateShopOperationSourcesState& sources;
 const PrivateShopOperationCapabilitiesState& capabilities;
 const HouseholdPurchaseCatalog& markets;
 const ActorSpatialHeadSourceReadContext& spatial;
 const CharacterConditionSources& body;
 const CurrentMomentRead& clock;
 const ActionElapsedStore& elapsed;
 const TimeScaleRef& scale;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
 const FamilyLifeHistoryRead4* life4=nullptr;
};
class PrivateShopOperationSourcePinSink;
SourceRef private_shop_operation_started_source(const StableId&);
Outcome<const PrivateShopOperationAcceptance*> read_original_private_shop_operation_acceptance(
 const PrivateShopOperationSourceContext&,const MomentOwnerSource&);
Outcome<const PrivateShopOperationTerminalSource*> read_original_private_shop_operation_terminal(
 const PrivateShopOperationSourceContext&,const MomentOwnerSource&);
Status visit_private_shop_operation_terminal_pins(const PrivateShopOperationSourceContext&,
 const MomentOwnerSource&,PrivateShopOperationSourcePinSink&);
Status validate_private_shop_operation_sources(const PrivateShopOperationSourceContext&);
Status emit_private_shop_operation_source_payload_fields(ProjectionWriter&,
 const PrivateShopOperationSourceContext&,const MomentOwnerSource&);
// Bootstrap original payload identity only; full S4/S1/financial checks follow
// through the actual complete source context after typed root hydration.
Outcome<Integer> read_private_shop_bootstrap_payload_schema(const CommonStore&,const PrivateShopState&,
 const PrivateShopOperationSourcesState&,const PrivateShopOperationCapabilitiesState&,const FactRecord&);
Status emit_private_shop_operation_source_payload_fields(ProjectionWriter&,const CommonStore&,
 const PrivateShopState&,const PrivateShopOperationSourcesState&,const FactRecord&,Integer source_schema);
class PrivateShopOperationSourcePinSink {public:virtual ~PrivateShopOperationSourcePinSink()=default;
 virtual Status source(const MomentOwnerSource&)=0;
 virtual Status locked_rule(const ActorFoundationRuleRef&)=0;
 virtual Status fixed_transition(const StableId& actual_transition_id)=0;
};
Status visit_private_shop_operation_acceptance_pins(const PrivateShopOperationSourceContext&,
 const MomentOwnerSource&,PrivateShopOperationSourcePinSink&);
SourceRef private_shop_labor_applied_source(const StableId&);
// Named source-local dependency cut: proves the actual original labor before
// reading its downstream wage. It never returns an amount or payment authority.
Outcome<const PrivateShopLaborAppliedSource*> read_original_private_shop_labor_for_wage(
 const PrivateShopOperationSourceContext&,const MomentOwnerSource&);
Outcome<const PrivateShopLaborAppliedSource*> read_original_private_shop_labor(
 const PrivateShopOperationSourceContext&,const MomentOwnerSource&);
Status visit_private_shop_labor_pins(const PrivateShopOperationSourceContext&,
 const MomentOwnerSource&,PrivateShopOperationSourcePinSink&);
// Pure original S4/S1 input validation, shared by the real action-time owner
// and the actual labor consumer. S0 alone constructs/measures these intervals.
Status validate_private_shop_original_provided_intervals(const PrivateShopOperationSourceContext&,
 const StableId&,const ActualServiceIntervalView&,const std::vector<FixedServiceTransition>&);
class PrivateShopLaborSpanVisitor {public:virtual ~PrivateShopLaborSpanVisitor()=default;
 virtual Status applied_span(const MomentSpan&,const CharacterCapabilityView&,
  const ActorStationaryLocationSpan&)=0;
};
// Full original applied labor is checked first. The spans are borrowed reads of
// the actual S4 boundaries inside S0's real provided intervals, not saved DTOs.
Status visit_private_shop_original_provided_spans(const PrivateShopOperationSourceContext&,
 const StableId&,const ActualServiceIntervalView&,PrivateShopLaborSpanVisitor&);
Status visit_original_private_shop_labor_spans(const PrivateShopOperationSourceContext&,
 const MomentOwnerSource&,PrivateShopLaborSpanVisitor&);
Outcome<StableId> apply_private_shop_labor_period_in_candidate(CommonStore&,LocalEconomyState&,
 PrivateShopState&,PrivateShopOperationSourcesState&,PrivateShopOperationCapabilitiesState&,const PrivateShopOperationSourceContext&,
 const StableId& actual_operation,Integer actual_period_ordinal);
// Prepared only before the actual original start append. It is not serializable,
// cannot be copied, and cannot adopt a historical integer attendance operation.
class PreparedPrivateShopOperationAcceptance {
 struct Impl;std::unique_ptr<Impl>impl_;
 explicit PreparedPrivateShopOperationAcceptance(std::unique_ptr<Impl>);
 friend struct PrivateShopOperationAcceptanceAccess;
public:
 PreparedPrivateShopOperationAcceptance();~PreparedPrivateShopOperationAcceptance();
 PreparedPrivateShopOperationAcceptance(PreparedPrivateShopOperationAcceptance&&)noexcept;
 PreparedPrivateShopOperationAcceptance&operator=(PreparedPrivateShopOperationAcceptance&&)noexcept;
 PreparedPrivateShopOperationAcceptance(const PreparedPrivateShopOperationAcceptance&)=delete;
 PreparedPrivateShopOperationAcceptance&operator=(const PreparedPrivateShopOperationAcceptance&)=delete;
};
Outcome<PreparedPrivateShopOperationAcceptance> prepare_private_shop_operation_acceptance(
 const PrivateShopOperationSourceContext&,const PrivateShopOperationQuote&,const ActorFoundationRuleRef& market_definition);
Status adopt_private_shop_operation_acceptance_in_candidate(CommonStore&,PrivateShopOperationSourcesState&,PrivateShopOperationCapabilitiesState&,
 const PrivateShopOperationSourceContext&,PreparedPrivateShopOperationAcceptance&&,const StableId& actual_operation);
}
