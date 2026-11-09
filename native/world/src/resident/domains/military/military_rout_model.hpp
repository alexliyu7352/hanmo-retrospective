// 显式溃退只延续原六角会战；真实轮次和原伤亡凭据用于证明受阻，不把当前计数当历史来源。
#pragma once
#include "battle_model.hpp"
#include "military_rout_contract.hpp"
namespace hanmo::resident {
struct MilitaryRoutRetreatQuote {
 StableId battle_id,unit_id;Integer round=0;
 HexPoint origin,destination;Integer heading=0,cost=0;
 std::vector<HexPoint> path;
 bool exit_reachable=false,at_exit=false,can_progress=false;
 // 只列原快照中真实相邻且能行动的敌军；不选择远方或同轮也溃退的人作俘虏保管者。
 std::vector<StableId> adjacent_captor_unit_ids;
};
// 首个完整注册须以Military10保存原回合前后与原结算引用；旧1..9不会多写空段。
// 这里是待接合同，不能把未注册的快照直接当作真实接管、军籍或Common授权。
struct MilitaryRoutRoundFact {
 StableId id,battle_id,battle_round_fact_id,casualty_fact_id;
 Integer tick=0,round=0;
 StableId resolution_kind="round",termination_reason;
 MilitaryBattleRecord before,after;
 MilitaryDamageSources damage_sources;
 std::vector<MilitaryRoutProgress> progress_before,progress_after;
 FieldOrder field_order;
};
struct MilitaryRoutEntryFact {StableId id;Integer tick=0;MilitaryBattleRecord initial;FieldOrder field_order;};
struct MilitaryRoutState {std::vector<MilitaryRoutEntryFact> entries;std::vector<MilitaryRoutRoundFact> rounds;FieldOrder field_order;};
}
