// 下属补任原三步永久来源；本人受领只产生意愿，真实到任才引用唯一Organization Office变更。
#pragma once
#include "political_subordinate_appointment_contract.hpp"
namespace hanmo::resident {
struct PoliticalSubordinateAppointmentOffer {
 StableId id,policy_id,organization_id,office_id,head_office_id,issuer_actor_id,candidate_actor_id,grant_id;
 Integer head_epoch=0,target_epoch=0,jurisdiction_revision=0,tick=0,response_until=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalSubordinateAppointmentAnswer {
 StableId id,offer_fact_id,actor_id;Integer tick=0,arrival_until=0;bool accepted=false;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalSubordinateAppointmentWithdrawal {
 StableId id,offer_fact_id,actor_id,grant_id;Integer head_epoch=0,tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalSubordinateAppointmentArrival {
 StableId id,acceptance_fact_id,actor_id,office_change_fact_id;Integer tick=0,installed_epoch=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 尚未注册的新1版完整形状：真实NPC只可显式原政治事业意图，和玩家结果互斥。
struct PoliticalSubordinateAppointmentAuthoredOrigin {StableId intent_id,director_policy_id,actor_id,offer_fact_id,stage;};
struct PoliticalSubordinateAppointmentState {
 Integer version=1;
 OrderedTable<PoliticalSubordinateAppointmentOffer> offers;
 OrderedTable<PoliticalSubordinateAppointmentAnswer> answers;
 OrderedTable<PoliticalSubordinateAppointmentWithdrawal> withdrawals;
 OrderedTable<PoliticalSubordinateAppointmentArrival> arrivals;
 OrderedTable<PoliticalSubordinateAppointmentAuthoredOrigin> authored_origins;
};
// 期限只从原政策与真实签发/接受时刻派生，无隐形续约或重复到任。
Status validate_political_subordinate_appointment_layout(const PoliticalSubordinateAppointmentState&);
}
