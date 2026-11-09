// 新尾仅保存原同意的具名来源、原撤回因果与实际结果绑定，不复制Family控制或财产账。
#pragma once
#include "voluntary_path_source.hpp"
namespace hanmo::resident {
inline constexpr const char* voluntary_path_module="family_voluntary_paths";
inline constexpr const char* voluntary_path_cancelled_kind="family_voluntary_path_cancelled";
struct VoluntaryPathCancellation {
    StableId kind=voluntary_path_cancelled_kind,plan_id,actor_id,reason;
    Integer tick=0,revision=0;
    // predecessor_cancel 不要求对方再同意；successor_withdrawal 必须真实本人撤回SAY。
    std::optional<KnownPersonalDialogueProof> withdrawal;
    FieldOrder field_order;
};
struct VoluntaryPathCommitBinding {
    StableId result_id,actor_id,kind;
    Integer request_seq=0,revision=0;
    FieldOrder field_order;
};
struct VoluntaryPathState {
    // key是原family_succession_consent fact；原agreement不在此复制。
    OrderedTable<VoluntaryPathSource> consents;
    OrderedTable<VoluntaryPathCancellation> cancellations;
    // 原完成fact对应的真实提交前revision；不重存双方/钱货或控制链。
    OrderedTable<Integer> completion_revisions;
    // 原接受/原完成/新撤回三种真实source各绑定唯一actual result/request。
    OrderedTable<VoluntaryPathCommitBinding> commits;
    FieldOrder field_order;
};
Status validate_voluntary_path_state_shape(const VoluntaryPathState&);
// 已完整Known/生命/出生/关系先验；原Legacy控制只读折叠，不递归新World总验。
Status validate_voluntary_path_history(const VoluntaryPathContext&,const VoluntaryPathState&);
// 外层完整候选承担回滚；同一Common/Family对象，不内嵌保存点或替换回调借用对象。
Outcome<StableId> accept_voluntary_path_in_candidate(CommonStore&,FamilyState&,VoluntaryPathState&,
    const VoluntaryPathContext&,const VoluntaryPathConsentQuote&);
// 显式新profile内，旧岗位许可仍沿原本人来源，但同样绑定新永久结果。
Outcome<StableId> accept_original_voluntary_path_in_candidate(CommonStore&,FamilyState&,VoluntaryPathState&,
    const VoluntaryPathContext&,const InteractionState&,const InteractionAccess&,const SuccessionConsentQuote&);
Outcome<SuccessionPersonalQuote> preview_accepted_voluntary_path_assets(const VoluntaryPathContext&,
    const VoluntaryPathState&,const StableId& plan_id);
Outcome<StableId> complete_voluntary_path_in_candidate(CommonStore&,FamilyState&,VoluntaryPathState&,
    const VoluntaryPathContext&,const StableId& plan_id,const SuccessionPersonalQuote&);
// 撤回不需要仍有原关系/资产，只核原计划、原控制提交者、在世本人和真实撤回声明。
Outcome<KnownPersonalDialogueProof> preview_voluntary_path_withdrawal(const VoluntaryPathContext&,
    const VoluntaryPathState&,const StableId& plan_id);
Outcome<StableId> cancel_voluntary_path_in_candidate(CommonStore&,FamilyState&,VoluntaryPathState&,
    const VoluntaryPathContext&,const StableId& plan_id,const std::optional<KnownPersonalDialogueProof>& withdrawal={});
Outcome<VoluntaryPathState> decode_voluntary_path_state(const BoundaryValue&);
BoundaryValue encode_voluntary_path_state(const VoluntaryPathState&);
Status visit_voluntary_path_checkpoint(CodecVisitor&,VoluntaryPathState&);
}
