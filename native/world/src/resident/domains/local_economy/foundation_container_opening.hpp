// B1 同一次真实初始安装拥有的 S5 容器创建证据；不新增第五表或阶段事实。
#pragma once
#include "resident/storage/resource_execution_draft.hpp"
namespace hanmo::resident {
class CommonStore;
class FoundationBodyInstallationToken;
class CheckpointMemoryCharge;
struct FoundationInitialInstitutionView;
// 每份值只能由原 Common 实际创建捕获产生；不是可消费余额或容量第二账。
struct FoundationWalletCreationEvidence {
  CreateWallet request;
  WalletView after;
};
struct FoundationInventoryCreationEvidence {
  CreateInventory request;
  InventoryView after;
};
struct FoundationContainerCreation {
  // definition.id 是锁内原型地址；实际实例 ID 只存在于原创建请求/结果。
  ResourceKey definition;
  std::variant<FoundationWalletCreationEvidence,
               FoundationInventoryCreationEvidence>
      actual;
};
// 移入唯一 S6 InitialInstallationSource 一次持有；不作为独立可发布根。
// 地点/用途由原基地/章程门提供，Common 容器没有新造地点字段。
struct FoundationContainerOpeningReceipt {
  CheckpointVector<FoundationContainerCreation> creations;
};
// 只借这份原 receipt 的真实实例地址，供 S6 原 Grant 做精确模板绑定。
// 不是当前使用权/支出授权；完整源验必须先于查询，借值不得持久保存。
Outcome<const StableId *>
read_foundation_created_container_id(const FoundationContainerOpeningReceipt &,
                                     ResourceKey::Kind,
                                     const StableId &definition_id);
// 冷门保原造容器正文、原型/实际组织/唯一分配 ID，不用今日余额重判历史。
Status validate_foundation_container_opening_evidence(
    const CommonStore &, const FoundationInitialInstitutionView &,
    const StableId &actual_organization,
    const FoundationContainerOpeningReceipt &);
// 热门还逐项匹配同候选的原活动 capture，不能用手写空钱包当实际创建。
Status validate_foundation_container_opening_capture(
    const CommonStore &, const FoundationInitialInstitutionView &,
    const StableId &actual_organization,
    const FoundationContainerOpeningReceipt &);
// 原 S6 私有 prepare 在真新身体后调用；token 不可外构、不得是自由 org_id。
// IDs 由 Common 唯一分配，无余额/库存初始注入；晚失败必须整候选弃置。
Outcome<FoundationContainerOpeningReceipt>
create_foundation_containers_in_candidate(
    CommonStore &, CheckpointMemoryCharge &actual_candidate_fields,
    FoundationBodyInstallationToken &);
} // namespace hanmo::resident
