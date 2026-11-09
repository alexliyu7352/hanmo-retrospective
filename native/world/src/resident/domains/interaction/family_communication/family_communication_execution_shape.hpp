// 原 B/D 值的局部关系及头/行定位；这不是渠道许可或 Knowledge 的成功证明。
#pragma once
#include "family_communication_execution.hpp"
namespace hanmo::resident {
// 当前已选的本地 Direct / NoFee 只做不可变 shape 校验。
// 真原规则、当时能力、控制与本人实际 command 仍须其 source reader 完整反读。
Status validate_family_communication_direct_post_shape(
    const FamilyCommunicationPostResult1 &);
// 借唯一 communication_links/2 的实际 B 表；不新增状态、不复制表或猜历史值。
// 此定位出口不能单独作为 Family received_source_ref 的准入。
Outcome<const FamilyCommunicationPostResult1 *>
read_family_communication_direct_post_shape(
    const CommonStore &,
    const OrderedTable<FamilyCommunicationPostResult1> &original_posts,
    const SourceRef &, Integer source_schema = 1);
} // namespace hanmo::resident
