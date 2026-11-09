// P§4.3 / INJ：原真实 wounded 的唯一身体消费；死亡仍由 Life4 同源处理。
#pragma once
#include "condition_capture.hpp"
#include "resident/domains/military/military_exact_sources.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct MilitaryCasualtyConditionCause;
struct MilitaryDeathCause4;
struct MilitaryWoundApplication {ConditionReceiptRefV2 receipt_ref;std::vector<BoundActorConditionWrite> actual_writes;bool already_applied=false;};
// 整个失败候选由 WorldBundle 丢弃；不得只回滚 Common 后保留本域源。
Outcome<MilitaryWoundApplication> apply_verified_military_wound(CommonStore&,AuthoredActorFoundationState&,const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,const SourceRef&,Integer source_ordinal,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
Status validate_military_wound_condition(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,const ConditionReceiptV2&,const WorldMoment& actual_now,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial);
// Original immutable cause/result only. Verifies the actual S2 source, scalar
// formula and exact locked effect selection; does not admit episode membership,
// current effect indices, recovery targets or a completed World.
Status validate_military_wound_condition_origin(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,const ConditionReceiptV2&,const WorldMoment& actual_now,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial);
Status validate_military_wound_coverage(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,const WorldMoment& actual_now,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr,MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial);
// 固定 cause tag4 必须明确 wounded/dead；不通过 life_transition_ref 有无猜型。
MilitaryCasualtyConditionCause military_death_condition_cause(const MilitaryDeathCause4&,Integer schema,Integer ordinal);
}
