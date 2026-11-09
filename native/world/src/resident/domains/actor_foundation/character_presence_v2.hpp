// 同一Origin2/DeathRef来源的历史存在与生命读面；HP0/伤势不变成死亡。
#pragma once
#include "created_origin.hpp"
#include "resident/domains/family/family_life_exact.hpp"
namespace hanmo::resident {
class CharacterAvailabilityProvenanceSink;
struct CharacterPresenceV2 {StableId actor_id;bool present=false,alive=false;StableId origin_ref;std::optional<DeathRef> death_ref;WorldMoment observed_at;};
// actual_boundary/actual_at 由所属业务先核真实payload，随后本口再核同一个Common头与原因果顺序。
// 未接入的动态Origin分支保持明确required，不借旧birth_tick路径或今日alive。
Outcome<CharacterPresenceV2> read_character_presence_before_fact_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyDeathState&,const FamilyLifeCauseSources4&,const StableId& actor_id,const SourceRef& actual_boundary,const WorldMoment& actual_at,const CurrentMomentRead& actual_clock,const FamilyCatalog* family_catalog=nullptr,const CommonCatalog* shared=nullptr,const BornCharacterSourceRead* born_sources=nullptr,CharacterAvailabilityProvenanceSink* provenance=nullptr,MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial);
// Actual-applied alive state, distinct from the effective-cause APIs below.
// No nested observation M is invented. Evidence only borrows retained roots;
// absent actors never authorize a dead-person delegation. Applied death means
// the real family_death application head is inside nested, not merely its cause.
struct CharacterAppliedPresenceV2 {
 bool present=false,alive=false;
 const CharacterOriginRecordV2* origin=nullptr;
 const FamilyDeathRecord* applied_death=nullptr;
};
Outcome<CharacterAppliedPresenceV2> read_character_applied_presence_at_prefix_v2(
 const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,
 const FamilyDeathState&,const FamilyLifeCauseSources4&,const StableId& actor,
 const CommonActualFactPrefix& nested,const CommonActualFactPrefix& outer,
 const WorldMoment& outer_at,const CurrentMomentRead& actual_clock,
 const FamilyCatalog* family_catalog=nullptr,const CommonCatalog* shared=nullptr,
 const BornCharacterSourceRead* born_sources=nullptr);
// 原入境/接受事实前必须确实已存在且存活。返回该实际切点之后、当前真实钟之前
// 的唯一 Death4 原行；nullptr 仅表示原链没有该终局。same-M 用真实 cause 的
// Common append 次序，不以死亡登记头、当前 alive 或 ID 字典序代替。
// 借用寿命限于同一候选与输入 FamilyDeathState；不保留为第二死亡记录。
Outcome<const FamilyDeathRecord*> read_character_death_after_fact_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyDeathState&,const FamilyLifeCauseSources4&,const StableId& actor_id,const SourceRef& actual_entry,const WorldMoment& entered_at,const CurrentMomentRead& actual_clock,const FamilyCatalog* family_catalog=nullptr,const CommonCatalog* shared=nullptr,const BornCharacterSourceRead* born_sources=nullptr,MilitaryCasualtyHistoryOrder ordering=MilitaryCasualtyHistoryOrder::legacy_serial);
}
