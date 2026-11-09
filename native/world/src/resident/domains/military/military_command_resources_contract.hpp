// 兵法恢复读只服务原同一contact入战与精确entry前界，不从今日技艺补旧指挥资源。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitaryTacticsEntryObservation {
 WorldIdentity source_identity;
 StableId actor_id,skill_id="tactics",before_fact_id,authored_actor_id;
 Integer observed_tick=0,experience=0,level=0;
 std::vector<StableId> causal_fact_ids;
};
class MilitaryTacticsEntryRead {
public:
 virtual ~MilitaryTacticsEntryRead()=default;
 virtual Outcome<MilitaryTacticsEntryObservation> current_tactics_entry(const StableId& actor_id)const=0;
 virtual Outcome<MilitaryTacticsEntryObservation> tactics_before_entry(const StableId& actor_id,const StableId& actual_entry_fact_id)const=0;
};
}
