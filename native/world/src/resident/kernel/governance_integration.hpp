// 安民命令组合真实政治批准、唯一任务行动与公共资源；共享调度只登记本叶typed接口。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/local_economy/relief/public_relief_catalog_links.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include "resident/domains/politics/governance_replies/political_governance_invitation_contract.hpp"
#include "resident/domains/politics/politics_governance_validation.hpp"
#include "resident/domains/politics/politics_view_index.hpp"
#include "resident/domains/tasks/governance_task_admission.hpp"
#include "resident/domains/tasks/governance_task_view.hpp"
namespace hanmo::resident {
// 玩家只选具名邀请或本人任务；钱粮、工期、事实与动作ID均由原领域生成。
struct GovernanceAcceptCommand {
  StableId authority_fact_id;
  Integer expected_world_revision = 0;
};
struct GovernanceCancelCommand {
  StableId task_id;
  Integer expected_world_revision = 0;
};
using GovernanceCommand =
    std::variant<PoliticalGovernanceCommissionCommand, GovernanceAcceptCommand,
                 GovernanceCancelCommand>;
struct GovernanceQuote {
  WorldIdentity identity;
  StableId actor_id;
  GovernanceCommand command;
  Integer duration = 0;
};
// 回执只保存可追索来源；效果数值与Task进度仍只有各领域一份真值。
struct GovernanceEffects {
  StableId authority_fact_id, task_id, action_id, result_fact_id,
      relief_fact_id;
};
// 只索引本人亲历批准和本人任务ID；重建在候选发布前完成，不入银行。
struct GovernanceViewIndex {
  WorldIdentity identity;
  std::size_t authority_count = 0, task_count = 0;
  CheckpointMap<StableId, std::vector<StableId>> authority_ids_by_actor,
      task_ids_by_actor;
};
struct GovernanceReadContext {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const PoliticsState &politics;
  const PoliticsCatalog &politics_catalog;
  const PoliticsGovernanceState &governance;
  const PoliticsGovernanceCatalog &governance_catalog;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const LocalEconomyState &economy;
  const PublicReliefCatalog &relief_catalog;
  const PublicReliefState &relief;
  const TaskState &tasks;
  const GovernanceTaskState &governance_tasks;
  const ActivityRead &activity;
  const PoliticsViewIndex *politics_index = nullptr;
  const GovernanceViewIndex *view_index = nullptr;
  const GovernanceInvitationReplyRead *invitation_replies = nullptr;
  const PoliticalGovernanceAgencyRead *agencies = nullptr;
};
// apply进入前调用者已持完整候选；任何失败须还原共同表与三域写集。
struct GovernanceWriteContext {
  CommonStore &common;
  const CommonCatalog &common_catalog;
  const PoliticsState &politics;
  const PoliticsCatalog &politics_catalog;
  PoliticsGovernanceState &governance;
  const PoliticsGovernanceCatalog &governance_catalog;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const LocalEconomyState &economy;
  const PublicReliefCatalog &relief_catalog;
  PublicReliefState &relief;
  TaskState &tasks;
  GovernanceTaskState &governance_tasks;
  const ActivityRead &activity;
  std::function<Status(const StableId &)> release_action;
  const GovernanceInvitationReplyRead *invitation_replies = nullptr;
  const PoliticalGovernanceAgencyRead *agencies = nullptr;
  GovernanceReadContext read() const;
};
bool governance_command_kind(const StableId &);
StableId governance_command_kind(const GovernanceCommand &);
Outcome<GovernanceQuote>
preview_governance_business(const GovernanceReadContext &,
                            const StableId &actor, const GovernanceCommand &);
Outcome<GovernanceEffects> apply_governance_business(GovernanceWriteContext &,
                                                     const StableId &actor,
                                                     const GovernanceCommand &,
                                                     const GovernanceQuote &);
Status validate_governance_receipt_source(const GovernanceReadContext &,
                                          const StableId &actor,
                                          const StableId &kind, Integer tick,
                                          const GovernanceEffects &);
GovernanceViewIndex make_governance_view_index(const CommonStore &,
                                               const PoliticsGovernanceState &,
                                               const TaskState &,
                                               const GovernanceTaskState &);
Outcome<BoundaryValue> query_governance_business(const GovernanceReadContext &,
                                                 const StableId &observer,
                                                 const StableId &place_id);
BoundaryValue encode_governance_command(const GovernanceCommand &);
Outcome<GovernanceCommand> decode_governance_command(const StableId &kind,
                                                     const BoundaryValue &);
BoundaryValue encode_governance_quote(const GovernanceQuote &);
Outcome<GovernanceQuote> decode_governance_quote(const BoundaryValue &);
BoundaryValue encode_governance_effects(const GovernanceEffects &);
Outcome<GovernanceEffects> decode_governance_effects(const BoundaryValue &);
Status visit_governance_effects(CodecVisitor &, GovernanceEffects &);
} // namespace hanmo::resident
