// 生前自愿换控只由当时玩家明确提交；受理对方真实私人SAY不等于当场接续。
#pragma once
#include "voluntary_path_receipts.hpp"
namespace hanmo::resident {
struct VoluntaryPathCommand {StableId kind,request_id,successor_actor_id,relation_fact_id,plan_id;};
struct VoluntaryPathQuote {
    std::optional<VoluntaryPathConsentQuote> consent;
    std::optional<SuccessionPersonalQuote> personal;
    std::optional<KnownPersonalDialogueProof> withdrawal;
    StableId plan_id;
};
Outcome<VoluntaryPathCommand> decode_voluntary_path_command(const StableId&,const BoundaryValue&);
Outcome<VoluntaryPathQuote> preview_voluntary_path_command(const VoluntaryPathContext&,const VoluntaryPathState&,
    const StableId& actor,const VoluntaryPathCommand&);
// 原玩家入口拥有Common点并回滚同Family/state对象；NPC/Director必须在写前拒这些命令。
// 外层World仍须绑定actual result并过完整新晚门，否则不可发布。
Outcome<FamilyEffect> apply_voluntary_path_command(CommonStore&,FamilyState&,VoluntaryPathState&,
    const VoluntaryPathContext&,const StableId& actor,const VoluntaryPathCommand&,const VoluntaryPathQuote&);
}
