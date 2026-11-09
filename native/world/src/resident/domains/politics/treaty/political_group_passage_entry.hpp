// 群旅通行采用的真实S1接头；准入权源仍由PoliticalPassageSources/HistoryRead独立核验。
#pragma once
#include "political_passage_source.hpp"
#include "resident/domains/organization/office_authority.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
struct GroupPoliticalPassageEntryContext {
 const CommonStore& common;
 const PoliticalPassageDecisionState& decisions;
 const SpatialCarrierState& carriers;
 const MovementExactState& movement;
 const GroupMovementExecutionState& executions;
 const TravelCatalog& travel_catalog;
 const TransportManifestHistorySourceRead& manifest_history;
 const OrganizationAuthorityClock& clock;
 ExactArithmeticBudget arithmetic;
};
// 热读只在S1已选正span、真实from准入并捕获原manifest后调用；无权自行创建crossing。
// 冷读沿原正activity和manifest→decision→crossing先序，不能重算今日位置作历史证明。
class NativeGroupPoliticalPassageEntryRead final:public PoliticalPassageEntryRead {
 const GroupPoliticalPassageEntryContext& x_;
public:
 explicit NativeGroupPoliticalPassageEntryRead(const GroupPoliticalPassageEntryContext& x):x_(x){}
 Outcome<ActualPassageEntry> read_actual_entry(const PassageRequest&,const TransportManifestRead&)const override;
 Status validate_historical_entry(const PoliticalPassageDecisionSource&)const override;
};
}
