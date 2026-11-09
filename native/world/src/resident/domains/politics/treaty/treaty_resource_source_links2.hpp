// 显式 signature/2 的非递归原事实链接；本层仅证明原链接，不代替完整产权、
// 授权、本人同意或实际预算占用验证。
#pragma once
#include "treaty_aid_accepted_resource_source2.hpp"
#include "treaty_aid_resource_proposal2.hpp"
namespace hanmo::resident {
struct TreatyResourceSourceLinks2 {
  const TreatyRevisionContext &revisions;
  const OrderedTable<TreatyAidResourceProposalSource2> &proposals;
  const OrderedTable<TreatyResourceSignatureSource2> &signatures;
  // 同一个 selected 条约体，不是由 Schedule 派生的平行根。
  const OrderedTable<TreatyAidAcceptedResourceSource2> &accepted_aid_resources;
};
Outcome<const TreatyAidResourceProposalSource2 *>
read_treaty_aid_resource_proposal2_links(const TreatyResourceSourceLinks2 &,
                                         const MomentOwnerSource &);
Outcome<const TreatyResourceSignatureSource2 *>
read_treaty_resource_signature2_links(const TreatyResourceSourceLinks2 &,
                                      const MomentOwnerSource &);
// 原接受链接不证明产权、权限或下游占用；必须再经过具名完整读取。
Outcome<const TreatyAidAcceptedResourceSource2 *>
read_treaty_aid_accepted_resource2_links(const TreatyResourceSourceLinks2 &,
                                         const MomentOwnerSource &);
// selected /2 原表与 Common 正反扫描；完整根另须原授权、知情、本人同意验证。
Status
validate_treaty_resource_source_links(const TreatyResourceSourceLinks2 &);
} // namespace hanmo::resident
