// 明确认可与原Legacy控制账分离：此处仅新增旁支认可，控制许可仍只写原三表。
#pragma once
#include "collateral_personal.hpp"
#include "collateral_recognition_source.hpp"
namespace hanmo::resident {
struct CollateralRecognitionFact {
  StableId kind = "family_collateral_recognized";
  CollateralRecognitionSource source;
  FieldOrder field_order;
};
// 同一原 recognitions 表中的明确物理后继。旧 /1 不写wrapper；新 /2
// 必须写schema1|2，不能靠是否有source字段猜版本或给旧值补假expression。
struct VersionedCollateralRecognitionFact {
  std::variant<CollateralRecognitionFact, CollateralPersonalAccepted2> payload;
  FieldOrder field_order;
  VersionedCollateralRecognitionFact() = default;
  VersionedCollateralRecognitionFact(CollateralRecognitionFact original)
      : payload(std::move(original)) {
  }
  VersionedCollateralRecognitionFact(CollateralPersonalAccepted2 original)
      : payload(std::move(original)) {
  }
};

struct CollateralCommitBinding {
  StableId result_id, actor_id, kind;
  Integer request_seq = 0, revision = 0;
  FieldOrder field_order;
};
struct CollateralControlState {
  OrderedTable<VersionedCollateralRecognitionFact> recognitions;
  OrderedTable<CollateralCommitBinding> recognition_commits;
  // 只钉原Legacy接受/撤回/激活与真实结果；不重存许可、激活或第二条控制链。
  OrderedTable<CollateralCommitBinding> legacy_commits;
  // source2表达自己的真实command绑定；不塞入旧recognition_commits伪装玩家。
  OrderedTable<CollateralPersonalProposal2> proposals;
  OrderedTable<CollateralPersonalReply2> replies;
  FieldOrder field_order;
};
struct CollateralRecognitionBasis {
  WorldIdentity source_identity;
  StableId recognition_fact_id, first_actor_id, second_actor_id, before_fact_id;
  Integer recognized_tick = 0, observed_tick = 0;
  CollateralFamilyOrigin ancestry;
};
Status validate_collateral_control_shape(const CollateralControlState &);
Status validate_collateral_control_shape(const CollateralControlState &,
                                         Integer selected_version);
// 原Common/Family/Birth/Lineage/Life、Known
// opening及机械历史控制先验；不回调完整Legacy或World。
Status
validate_collateral_recognition_history(const CollateralRecognitionContext &,
                                        const CollateralControlState &);
// 只有外层完整Common+本state候选可调用，失败由原调用者整候选回滚，不嵌套/提交他人的保存点。
Outcome<StableId> record_collateral_recognition_in_candidate(
    CommonStore &, const CollateralRecognitionContext &,
    CollateralControlState &, const CollateralRecognitionSource &);
// 完整认可历史和实际result绑定先验；只读精确原关系，当前/事实前均不扫描“最新”来代换。
Outcome<CollateralRecognitionBasis>
read_collateral_recognition(const CommonStore &, const CollateralControlState &,
                            const StableId &exact_recognition);
Outcome<CollateralRecognitionBasis> read_collateral_recognition_before_fact(
    const CommonStore &, const CollateralControlState &,
    const StableId &exact_recognition, Integer tick,
    const StableId &actual_fact);
Outcome<CollateralControlState>
decode_collateral_control_state(const BoundaryValue &);
Outcome<CollateralControlState>
decode_collateral_control_state(const BoundaryValue &,
                                Integer selected_version);
BoundaryValue encode_collateral_control_state(const CollateralControlState &);
BoundaryValue encode_collateral_control_state(const CollateralControlState &,
                                              Integer selected_version);
Status visit_collateral_control_checkpoint(CodecVisitor &,
                                           CollateralControlState &);
Status visit_collateral_control_checkpoint(CodecVisitor &,
                                           CollateralControlState &,
                                           Integer selected_version);
Status emit_collateral_control_state_fields(ProjectionWriter &,
                                            const CollateralControlState &,
                                            Integer selected_version);
} // namespace hanmo::resident
