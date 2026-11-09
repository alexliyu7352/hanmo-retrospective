// v1e §2.2/§5.2 原旁支表达的真实命令绑定；不另建 command/许可账。
#pragma once
#include "collateral_personal.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
struct ProfileControlEligibilitySources;
struct CollateralControlState;
struct ProposeCollateralRecognition2;
struct RecognizeCollateral2;
struct RespondCollateralRecognition2;
// 只有执行结果身份，不是许可或完整来源证明；所有成员都借同原根。
struct CollateralPersonalCommandSource2 {
  static constexpr Integer source_schema = 2;
  const FactRecord *header = nullptr;
  const FamilyPersonalExpression2 *expression = nullptr;
  const CommitResult *command = nullptr;
  // 三者恰有其一，仍借原表，不保第二payload或重定位规则。
  const CollateralPersonalProposal2 *proposal = nullptr;
  const CollateralPersonalReply2 *reply = nullptr;
  const CollateralPersonalAccepted2 *accepted = nullptr;
};
// 原result/history已晚绑定、但Director adoption尚未写入时也可读取。
// 不捕获P10、不验未来adoption、不将参与者当受众。完整发布仍须原SourceRead。
Outcome<CollateralPersonalCommandSource2>
read_collateral_personal_command_source2(const CommonStore &,
                                         const CollateralControlState &,
                                         const SourceRef &actual_source);

// 原Director采用前：把真正已执行A与借用typed命令逐值核对；不核未来采用。
// caller另核同次route actor/epoch/decision_seq/admission和实际Actor执行绑定。
Status validate_collateral_personal_command_request2(
    const CommonStore &, const CollateralControlState &, const SourceRef &,
    const ProposeCollateralRecognition2 &);
Status validate_collateral_personal_command_request2(
    const CommonStore &, const CollateralControlState &, const SourceRef &,
    const RecognizeCollateral2 &);
Status validate_collateral_personal_command_request2(
    const CommonStore &, const CollateralControlState &, const SourceRef &,
    const RespondCollateralRecognition2 &);

// 原 source2 三命令唯一名；旧 source1 family_collateral_recognize 不变。
std::string_view
collateral_personal_command_kind(std::string_view actual_fact_kind) noexcept;
// 原 owner 先从唯一三表借出 expression，随后以同 Common 实际头与唯一
// result/history 反验晚绑定。这里只证明实际命令身份，不证明模板/已知/身体准入，
// 不能作为 CollateralPersonalSourceRead 的完整实现或自知 Report 的许可来源。
Outcome<const CommitResult *>
read_collateral_personal_actual_command(const CommonStore &, const FactRecord &,
                                        const FamilyPersonalExpression2 &);
// A 的真实当前 PlayerActor 路由从唯一控制 owner 捕获，不接受请求自报控制来源。
// 仅形成原 route value；模板、对象已知、成年和身体门仍须在原 writer 分别重验。
Outcome<FamilyPlayerExpressionRoute> capture_collateral_personal_player_route(
    const ProfileControlEligibilitySources &, const StableId &actual_actor_id);
// 仅 PlayerActor 原分支，沿真实 A 之前的控制链重读全部 nominal 字段。
// NPC 必须另经原 Director5 registration/admission/采用来源，不能借此降成玩家，
// 也不能强借 physical6 的 accepted_command 布局改变原所选合同。
Status validate_collateral_personal_player_route(
    const ProfileControlEligibilitySources &, const SourceRef &,
    const FamilyPersonalExpression2 &);
} // namespace hanmo::resident
