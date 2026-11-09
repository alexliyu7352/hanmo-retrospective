// 野外交接永久事实只追加来源与实际位置变化；旧形成账、兵籍与私产不重写。
#pragma once
#include "military_field_command_contract.hpp"
namespace hanmo::resident {
struct MilitaryFieldCommandPolicy {StableId profile="casualty_appointment_v1";};
struct MilitaryFieldCommandAssign {StableId request_key,authorization_fact_id,acceptance_fact_id;FieldOrder field_order;};
struct MilitaryFieldCommandFact {
 StableId id,request_key,actor_id,authorization_fact_id,acceptance_fact_id;
 MilitaryFieldCommandTarget target;
 StableId issuer_actor_id,office_id,grant_id,vessel_id;
 Integer authority_epoch=0,issued_tick=0,accepted_tick=0,tick=0;
 // 本片只接受已在原军内的真人。army模式从原InArmy移到亡帅未移动的真实锚点，unit不搬人。
 std::optional<Location> army_anchor;
 FieldOrder field_order,target_field_order,source_field_order,candidate_source_field_order,death_field_order;
};
struct MilitaryFieldCommandQuote {bool replayed=false;MilitaryFieldCommandFact fact;};
}
