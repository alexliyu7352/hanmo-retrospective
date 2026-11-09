// B 的真实命令身份与提交尾；不以此替代 A、渠道或身体准入。
#pragma once
#include "family_communication_links2.hpp"
#include "family_communication_post_receipt.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
// 冷读只借原 actor:seq 成功结果与唯一 History。重复命令不能改写首次绑定。
Outcome<const CommitResult *>
read_family_post_actual_command1(const CommonStore &,
                                 const FamilyCommunicationLinksState2 &,
                                 const SourceRef &actual_post);
// 原 World 命令尾在真实结果已保留后调用；before 是同根命令前快照。
// 首次只给本次唯一新增 B 绑定结果；新序号重复必须四根全不变。
// 此叶不创建 B、Report 或 Knowledge；失败由外层撤回完整私有候选。
Status bind_family_post_actual_result_in_candidate1(
    CommonStore &, const FamilyCommunicationLinksState2 &before,
    FamilyCommunicationLinksState2 &after, const StableId &actual_actor,
    Integer actual_sequence, const CommitResult &actual_result);
} // namespace hanmo::resident
