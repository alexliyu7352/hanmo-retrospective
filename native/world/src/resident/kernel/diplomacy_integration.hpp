// 外交组合叶持真实业务与有限视图；中央仅登记typed命令、阶段和共同冷状态。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_delegation.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_delegation_npc.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_missions.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_views.hpp"
#include "resident/domains/tasks/envoy_task_actions.hpp"
#include "resident/domains/tasks/envoy_task_cancellation.hpp"
#include "resident/domains/tasks/envoy_task_report_rules.hpp"
#include "resident/domains/tasks/envoy_task_view.hpp"
#include "resident/domains/travel/travel_navigation.hpp"
namespace hanmo::resident {
struct DiplomaticEnvoyAcceptCommand {
  StableId authority_fact_id;
  Integer expected_world_revision = 0;
};
struct DiplomaticEnvoyDeliverCommand {
  StableId task_id, provider_actor_id;
  Integer expected_world_revision = 0;
};
struct DiplomaticEnvoyReportCommand {
  StableId task_id, provider_actor_id;
  Integer expected_world_revision = 0;
};
struct DiplomaticEnvoyCancelCommand {
  StableId task_id;
  Integer expected_world_revision = 0;
};
using DiplomacyCommand =
    std::variant<DiplomaticProposeCommand, DiplomaticMissionCommand,
                 DiplomaticPresentCommand, DiplomaticReplyCommand,
                 DiplomaticEnvoyAcceptCommand, DiplomaticEnvoyDeliverCommand,
                 DiplomaticEnvoyCancelCommand, DiplomaticDelegateCommand,
                 DiplomaticEnvoyReportCommand>;
struct DiplomacyQuote {
  WorldIdentity identity;
  StableId actor_id;
  DiplomacyCommand command;
  Integer duration = 0;
};
struct DiplomacyEffects {
  StableId fact_id, task_id, action_id;
};
struct DiplomacyBusinessViewIndex {
  DiplomacyViewIndex politics;
  std::size_t task_count = 0;
  CheckpointMap<StableId, std::vector<StableId>> task_ids_by_actor;
  CheckpointMap<StableId, std::vector<StableId>> delegation_ids_by_envoy;
};
struct DiplomacyNavigationRead {
  const TravelState &state;
  const TravelCatalog &catalog;
  const TravelServices &services;
};
struct DiplomacyReadContext {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const DiplomacyState &diplomacy;
  const DiplomacyCatalog &diplomacy_catalog;
  const InteractionState &interaction;
  const InteractionCatalog &interaction_catalog;
  const InteractionAccess &interaction_access;
  const TaskState &tasks;
  const EnvoyTaskState &envoy_tasks;
  const ActivityRead &activity;
  const DiplomacyBusinessViewIndex *view_index = nullptr;
  const DiplomacyNavigationRead *navigation = nullptr;
};
// 调用者负责同候选Common/Diplomacy/Task/Envoy整体备份，不能只回滚其中一个领域。
struct DiplomacyWriteContext {
  CommonStore &common;
  const CommonCatalog &common_catalog;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  DiplomacyState &diplomacy;
  const DiplomacyCatalog &diplomacy_catalog;
  const InteractionState &interaction;
  const InteractionCatalog &interaction_catalog;
  const InteractionAccess &interaction_access;
  TaskState &tasks;
  EnvoyTaskState &envoy_tasks;
  const ActivityRead &activity;
  EnvoyReleaseAction release_action;
  const DiplomacyBusinessViewIndex *view_index = nullptr;
  const DiplomacyNavigationRead *navigation = nullptr;
  DiplomacyReadContext read() const;
};
bool diplomacy_command_kind(const StableId &);
StableId diplomacy_command_kind(const DiplomacyCommand &);
Outcome<DiplomacyQuote> preview_diplomacy_business(const DiplomacyReadContext &,
                                                   const StableId &actor,
                                                   const DiplomacyCommand &);
Outcome<DiplomacyEffects> apply_diplomacy_business(DiplomacyWriteContext &,
                                                   const StableId &actor,
                                                   const DiplomacyCommand &,
                                                   const DiplomacyQuote &);
DiplomacyBusinessViewIndex
make_diplomacy_business_view_index(const DiplomacyReadContext &);
Outcome<BoundaryValue> query_diplomacy_business(const DiplomacyReadContext &,
                                                const StableId &observer,
                                                const StableId &place_id);
BoundaryValue encode_diplomacy_command(const DiplomacyCommand &);
Outcome<DiplomacyCommand> decode_diplomacy_command(const StableId &kind,
                                                   const BoundaryValue &);
BoundaryValue encode_diplomacy_quote(const DiplomacyQuote &);
Outcome<DiplomacyQuote> decode_diplomacy_quote(const BoundaryValue &);
} // namespace hanmo::resident
