// 原 StateNeedPolicy 四个引用的不可变 Task 目录，不替代真实披露或收费事实。
#pragma once
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/affairs/state_need_catalog.hpp"
#include "state_need_task.hpp"
namespace hanmo::resident {
struct StateTaskTemplateRule {
  StableId id;
  StateObjectiveCriterion criterion =
      StateObjectiveCriterion::care_coverage_established;
  StableId required_actor_role;
};
// 只许可原合同的最小求助披露；本人或合法代理的实际同意另由原事实证明。
struct StateTaskDisclosureRule {
  StableId id;
};
// 金额完全来自作者同锁条款，不在代码内定价；实供费仍须实际源逐笔证明。
struct StateTaskFeeRewardRule {
  StableId id;
  Integer revision = 0, maximum_service_fee = 0, success_reward = 0;
};
// 唯一已批准含义为先停未来服务、支付实供、退原未用款，不增加取消罚款。
struct StateTaskCancellationRule {
  StableId id;
};
struct StateTaskOriginalCatalog {
  CheckpointCopyLease identity_memory;
  CatalogIdentity identity;
  Integer version = 1;
  OrderedTable<StateTaskTemplateRule> templates;
  OrderedTable<StateTaskDisclosureRule> disclosures;
  OrderedTable<StateTaskFeeRewardRule> fee_reward_terms;
  OrderedTable<StateTaskCancellationRule> cancellation_rules;
  StateTaskOriginalCatalog() = default;
  StateTaskOriginalCatalog(const StateTaskOriginalCatalog &) = delete;
  StateTaskOriginalCatalog &
  operator=(const StateTaskOriginalCatalog &) = delete;
  StateTaskOriginalCatalog(StateTaskOriginalCatalog &&) = default;
  StateTaskOriginalCatalog &operator=(StateTaskOriginalCatalog &&) = delete;
};
// 四个指针均借唯一已安装目录；调用者不能拿相同 ID 的临时行代替它。
struct StateTaskPolicySources {
  const StateTaskTemplateRule *task = nullptr;
  const StateTaskDisclosureRule *disclosure = nullptr;
  const StateTaskFeeRewardRule *money = nullptr;
  const StateTaskCancellationRule *cancellation = nullptr;
};
Status validate_state_task_original_catalog(const StateTaskOriginalCatalog &);
Outcome<StateTaskPolicySources>
read_state_task_policy_sources(const StateTaskOriginalCatalog &,
                               const CatalogIdentity &,
                               const StateNeedPolicy &);
Outcome<StateTaskOriginalCatalog>
// definitions 直接借原 prepared 顶层四字典；config 仅显式版本。
// 只安装已选 StateNeedPolicy 真引用的行，不拼第二棵 Boundary 树。
decode_state_task_original_catalog(const BoundaryValue &definitions,
                                   const BoundaryValue &selected_config,
                                   const CatalogIdentity &,
                                   const StateNeedCatalog &selected_policies);
class ProjectionWriter;
Status emit_state_task_original_catalog(ProjectionWriter &, std::string_view,
                                        const StateTaskOriginalCatalog &);
// 原 OrderedTable 在复制前逐真实字段收费，不使用零费用泛型后备。


} // namespace hanmo::resident
