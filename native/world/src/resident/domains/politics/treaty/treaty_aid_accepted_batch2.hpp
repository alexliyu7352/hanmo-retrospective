#pragma once
#include "treaty_aid_accepted_resource_read2.hpp"
namespace hanmo::resident {
struct TreatyAidActualRecipientInput2 {
  StableId term_id;
  MomentOwnerSource actual_recipient_consent;
};
// 原激活候选显式传入真实本人同意，顺序必须等于原条款中 Aid 条款顺序。
// 本函数不推选默认同意、不增玩家命令字段、不建立 Schedule 或资金占用。
// 全部原接受源成功后才允许 S5 原编排消费返回的真实来源；失败丢弃整个候选。
Outcome<std::vector<MomentOwnerSource>>
accept_treaty_aid_resource_set_in_candidate2(
    CommonStore &, OrderedTable<TreatyAidAcceptedResourceSource2> &,
    const TreatyAidAcceptedResourceContext2 &,
    const MomentOwnerSource &actual_activation,
    const std::vector<TreatyAidActualRecipientInput2> &);
} // namespace hanmo::resident
