// 本人随行页只读当前已显示邀请或真实成员关系；没有全作者列表或远方人物探测。
#pragma once
#include "party_invitation.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
// 一个当前会话节点最多一份邀请；成员状态以原Party为唯一真值。
struct PartyParticipationRow {
    StableId kind,invitation_id,party_id,leader_actor_id,leader_name,role_id;
    Integer cost_per_tick=0;
    bool available=false;
    StableId code;
    bool conversation_open=false;
};
struct PartyParticipationView {
    WorldIdentity source_identity;
    StableId observer_actor_id;
    std::vector<PartyParticipationRow> rows;
};
// actor必须是世界当前玩家，读取不建立邀请、不消耗消息或改变成员。
Outcome<PartyParticipationView> query_party_participation(const CommonStore&,const InteractionState&,const InteractionCatalog&,const PartyInvitationCatalog&,const InteractionAccess&,const StableId& actor_id);
// 动作仍需原公共preview/确认；DTO里的available不代表费用或承诺已成立。
BoundaryValue encode_party_participation(const PartyParticipationView&);
}
