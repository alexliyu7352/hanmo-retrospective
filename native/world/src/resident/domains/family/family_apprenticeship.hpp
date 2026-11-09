// 师承账持真实双方接受与退出；原 XP、物品、亲缘、职业和控制链不由此账写入。
#pragma once
#include "resident/kernel/common_fact_prefix.hpp"
#include "apprenticeship_source.hpp"
#include <set>
namespace hanmo::resident {
struct ApprenticeshipProposal {
    StableId kind="family_apprenticeship_proposed";
    ApprenticeshipStatementSource source;
    FieldOrder field_order;
};
struct ApprenticeshipReply {
    StableId kind="family_apprenticeship_replied",proposal_id,actor_id,decision;
    Integer tick=0,revision=0;
    std::optional<ApprenticeshipStatementSource> confirmation;
    FieldOrder field_order;
};
// 退出是本人拒绝继续关系，不要求对方许可；不会取消或结清已有课程与资产责任。
struct ApprenticeshipExit {
    StableId kind="family_apprenticeship_exited",bond_id,actor_id;
    Integer tick=0,revision=0;
    FieldOrder field_order;
};
// 原结果/请求一对一绑定真实关系事件；缓存裁剪后仍保来源，不分配额外 Common 事实。
struct ApprenticeshipCommitBinding {
    StableId result_id,actor_id,kind;
    Integer request_seq=0,revision=0;
    FieldOrder field_order;
};
struct FamilyApprenticeshipState {
    OrderedTable<ApprenticeshipProposal> proposals;
    OrderedTable<ApprenticeshipReply> replies;
    OrderedTable<ApprenticeshipExit> exits;
    OrderedTable<ApprenticeshipCommitBinding> commits;
    FieldOrder field_order;
};
// 关系以实际接受事实为唯一 ID；退出仍保原关系，死亡不伪造一份本人退出决定。
struct ApprenticeshipBond {
    StableId id,proposal_id,teacher_actor_id,student_actor_id,skill_id;
    Integer accepted_tick=0;
    std::optional<StableId> exit_fact_id;
};
struct ApprenticeshipTimeline {
    OrderedTable<ApprenticeshipBond> bonds;
    std::set<StableId> terminal_proposals;
};
// 结构门同时供 JSON、binary 和原生候选使用，不能仅靠重新编码才发现坏字段。
Status validate_family_apprenticeship_shape(const FamilyApprenticeshipState&);
// 完整 Family/Creation/Birth、军史/死亡/占用、历史控制须先验；本读不递归 World 或 Growth。
Outcome<ApprenticeshipTimeline> validate_family_apprenticeship(const CommonStore&,const InteractionCatalog&,const ApprenticeshipCatalog&,const FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&);
// 冷边界只允许真实头，由调用者先核自己的 payload.tick；不把后来退出倒填旧师承。
Outcome<std::optional<ApprenticeshipBond>> read_apprenticeship_bond_before_fact(const CommonStore&,const FamilyApprenticeshipState&,const StableId& bond_id,Integer tick,const StableId& actual_before_fact);
Outcome<std::optional<ApprenticeshipBond>> read_apprenticeship_bond_at_original_prefix(const CommonStore&,const FamilyApprenticeshipState&,const StableId& bond_id,Integer tick,const CommonActualFactPrefix&);
// 以下写口只在外层完整 Common＋本账候选事务中使用；失败由调用方原保存点整候选回滚。
// 不嵌套保存点、不提交调用方保存点；同栈访问器必须始终借这份实际可变 state。
Outcome<StableId> receive_apprenticeship_proposal_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&,const ApprenticeshipStatementQuote&);
Outcome<StableId> reply_apprenticeship_proposal_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&,const StableId& proposal_id,const StableId& decision,const std::optional<ApprenticeshipStatementQuote>& confirmation);
Outcome<StableId> exit_apprenticeship_in_candidate(CommonStore&,const InteractionCatalog&,const ApprenticeshipCatalog&,FamilyApprenticeshipState&,const ApprenticeshipIdentityRead&,const StableId& bond_id);
Outcome<FamilyApprenticeshipState> decode_family_apprenticeship(const BoundaryValue&);
BoundaryValue encode_family_apprenticeship(const FamilyApprenticeshipState&);
Status visit_family_apprenticeship_checkpoint(CodecVisitor&,FamilyApprenticeshipState&);
}
