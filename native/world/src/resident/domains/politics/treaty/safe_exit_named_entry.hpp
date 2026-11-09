// P21.1原全具名、无货/人口/军务份额入境的初始分区；不是新的资格或继承账。
#pragma once
#include "safe_exit_entry.hpp"
#include "resident/domains/actor_foundation/character_presence_v2.hpp"
namespace hanmo::resident {
struct SafeExitNamedEntryContext {
 const CommonStore& common;const SpatialCarrierState& carriers;
 const MovementExactState& movement;const GroupMovementExecutionState& executions;
 const TravelCatalog& travel_catalog;const TransportManifestHistorySourceRead& manifest_history;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& foundation_catalog;
 const FamilyDeathState& deaths;const FamilyLifeCauseSources4& life;
 const CurrentMomentRead& clock;
 const FamilyCatalog* family_catalog=nullptr;const CommonCatalog* shared=nullptr;
 const BornCharacterSourceRead* born_sources=nullptr;ExactArithmeticBudget arithmetic;
};
// 纯原源门可供当前保留范围反验复用，不分配另一份manifest/身份名单。
Status validate_original_named_safe_exit_entry_sources(const SafeExitNamedEntryContext&,
 const BoundaryCrossingReceipt&,const TransportEntryManifestSource&);
// 输入必须是本候选真实crossing与其唯一不可变entry整行。只投影初态全部原身份，
// 采用者仍必须另核实际Decision、同版term与entry资格。非本有限分支明确required。
Outcome<SafeExitEntryPartition> read_original_named_safe_exit_partition(
 const SafeExitNamedEntryContext&,const BoundaryCrossingReceipt&,
 const TransportEntryManifestSource&);
Status validate_original_named_safe_exit_partition(const SafeExitNamedEntryContext&,
 const BoundaryCrossingReceipt&,const TransportEntryManifestSource&,
 const SafeExitEntryPartition&);
// 借用原Death4行，仅说明原具名入境者的真实后续终局。不将死亡改作outside、
// 不把份额分给余人；line终态消费仍须原明确后继。空列表表示完整原链无此终局。
Outcome<std::vector<const FamilyDeathRecord*>> read_original_named_safe_exit_deaths(
 const SafeExitNamedEntryContext&,const BoundaryCrossingReceipt&,
 const TransportEntryManifestSource&);
}
