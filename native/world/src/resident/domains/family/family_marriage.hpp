// 独立婚姻账只记录个人提议、本人答复和双方解除；原财产、血亲、照护与控制权均不写。
#pragma once
#include "marriage_source.hpp"
#include "family_death.hpp"
#include "family_model.hpp"
#include <map>
namespace hanmo::resident {
struct MarriageProposal { StableId kind="family_marriage_proposed";MarriageStatementSource source;FieldOrder field_order; };
// 接受时重新在原意愿节点确认，拒绝和撤回只需本人决定，不能带另一份同意来源。
struct MarriageReply { StableId kind="family_marriage_replied",proposal_id,actor_id,decision;Integer tick=0,revision=0;std::optional<MarriageStatementSource> confirmation;FieldOrder field_order; };
struct MarriageDissolution { StableId kind="family_marriage_dissolved",marriage_id;MarriageStatementSource source;FieldOrder field_order; };
struct FamilyMarriageState { OrderedTable<MarriageProposal> proposals;OrderedTable<MarriageReply> replies;OrderedTable<MarriageDissolution> dissolutions;FieldOrder field_order; };
// 历史玩家与军事处境均按确切事实前边界读取；不把今天的自由身倒填过去。
struct MarriageAccess {
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
    std::function<Status(const StableId&,Integer,const StableId&)> personally_available;
    // 显式谱系profile另核生物＋收养混合亲缘；未启用旧档保持原收养规则。
    std::function<Status(const std::map<StableId,StableId>&,const StableId&,const StableId&)> known_kinship;
};
// 真值只供权威规则使用。人物页面必须另外证明死亡知情，不能直接把此投影公开。
struct MarriageBond { StableId id,first_actor_id,second_actor_id;bool authored_origin=false; };
struct MarriageTimeline { OrderedTable<MarriageBond> active_by_actor;std::set<StableId> replied_proposals,death_knowledge_pending;std::map<StableId,StableId> adoptive_parents;std::function<Status(const std::map<StableId,StableId>&,const StableId&,const StableId&)> known_kinship; };
// 原Family/Military/死亡历史已通过是前置；整个本账重放不写Common、不抽随机数。
Outcome<MarriageTimeline> validate_family_marriage(const CommonStore&,const FamilyState&,const FamilyDeathState&,const InteractionCatalog&,const MarriageSourceCatalog&,const FamilyMarriageState&,const MarriageAccess&);
// 预检只消费重放结果，不创建预测事实；当前已知直系或收养手足不得形成新的配偶关系。
Status validate_marriage_current_pair(const MarriageSourceCatalog&,const MarriageTimeline&,const StableId&,const StableId&);
// 原成年收养只借当前婚姻投影，不能让新父子/手足关系与既存个人婚姻相冲突。
Status validate_marriage_adoption_boundary(const MarriageTimeline&,const StableId& adopter,const StableId& adoptee);
// 听取提议与答应分别是不同命令；收到了原提议不等于当前本人已接受。
Outcome<StableId> receive_marriage_proposal(CommonStore&,const FamilyState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,FamilyMarriageState&,const MarriageAccess&,const MarriageStatementQuote&);
Outcome<StableId> reply_marriage_proposal(CommonStore&,const FamilyState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,FamilyMarriageState&,const MarriageAccess&,const StableId& proposal_id,const StableId& decision,const std::optional<MarriageStatementQuote>& confirmation);
// 仅新账中双方同意的个人关系可按此解除；作者旧婚姻缺原条款时明确保留未知责任。
Outcome<StableId> dissolve_personal_marriage(CommonStore&,const FamilyState&,const FamilyDeathState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const MarriageSourceCatalog&,FamilyMarriageState&,const MarriageAccess&,const StableId& marriage_id,const MarriageStatementQuote&);
Outcome<FamilyMarriageState> decode_family_marriage(const BoundaryValue&);
BoundaryValue encode_family_marriage(const FamilyMarriageState&);
Status visit_family_marriage_checkpoint(CodecVisitor&,FamilyMarriageState&);
}
