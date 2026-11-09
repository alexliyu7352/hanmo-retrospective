// 原旁支 source2 的唯一借用投影；S3不能借它另存Family条款或替本域核许可。
#pragma once
#include "collateral_personal.hpp"
namespace hanmo::resident {
enum class CollateralPersonalExpressionKind2 { proposed, replied, recognized };
// 生命周期仅在同一次原只读候选内；ancestry只在原proposal持有一次。
// 提供者必须先完整核原source/header/route/command/当时资格，不能仅拼地址。
struct CollateralPersonalOriginalReadView {
  const FactRecord *source_header = nullptr;
  SourceRef proposal_source;
  const CollateralPersonalProposal2 *proposal = nullptr;
  const FamilyPersonalExpression2 *expression = nullptr;
  CollateralPersonalExpressionKind2 kind =
      CollateralPersonalExpressionKind2::proposed;
  std::optional<FamilyPersonalReplyDecision2> decision;
  // 仅由实际A原validator反读已采用known_basis与真实S3收知后给出。
  // 这是临时投影，不持久化；accepted/refused缺causal必须拒，不默认“已收到”。
  std::optional<SourceRef> causal_received_source_ref;
  std::optional<SourceRef> reply_to_communication_ref;
};
class CollateralPersonalSourceRead {
public:
  virtual ~CollateralPersonalSourceRead() = default;
  virtual bool bound_to(const CommonStore &) const noexcept = 0;
  // 当前可读的真实已发布A，不代表对方已收到或原proposal仍可接受。
  virtual Outcome<CollateralPersonalOriginalReadView>
  read_current_expression(const SourceRef &) const = 0;
  // 仅当前第一次真实发送准入：已终结proposal的旧初提不可再发；
  // 实际accepted/refused/withdrawn仍可发其真实终局通知，不复活原提议。
  // 这个当前gate不影响既有B的历史反读。
  // 由原Family owner核当前三表，S3不复制终局规则；缺实现不能默认Sendable。
  virtual Status validate_current_first_send(const SourceRef &) const = 0;
  // actual_before必须本世界已存在的真实B/D消费头，不能空串或任意Tick冷读。
  // 此读口不回调该B/D的收知验证，避免自己的采用结果循环证明自己。
  virtual Outcome<CollateralPersonalOriginalReadView>
  read_expression_before_fact(const SourceRef &,
                              const SourceRef &actual_before) const = 0;
};
} // namespace hanmo::resident
