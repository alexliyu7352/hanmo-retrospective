#include "resident/kernel/catalog_section_observation.hpp"
#include "resource_site_absence.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/craft/item_service.hpp"
#include "resident/kernel/native_item_craft_state.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <string_view>
namespace hanmo::resident {
namespace {
// 这些名称只用于拒绝孤立来源，不能据此承认尚无原 producer 的业务。
bool resource_kind(std::string_view kind) {
  return kind == "SurveyResourceSite" || kind == "PreviewGatherResource" ||
         kind == "StartGatherResource" || kind == "StopGatherResource" ||
         kind == "GatherReceipt" || kind == "GatherClaim" ||
         kind == "ResourceSiteObservation" ||
         kind == "ResourceRecoveryReceipt" || kind == "Gather" ||
         kind == "gather" || kind == "resource_harvest" ||
         kind == "resource_recovery" || kind == "resource_observation" ||
         kind == "resource_modifier" || kind.starts_with("resource_site") ||
         kind.starts_with("gather_") || kind.starts_with("survey_resource");
}
bool resource_source(const SourceRef &source) {
  return source.domain == "resource_sites" ||
         source.domain == "resource_site" || resource_kind(source.kind);
}
Status unimplemented(const StableId &id = {}) {
  return common_failure("ResourceSitesNotImplemented", id,
                        ErrorKind::unsupported_capability);
}
Status original_source_absent(const SourceRef &source) {
  return resource_source(source) ? unimplemented(source.id) : common_success();
}
// Gather 仍只能由真实 Actor Action 领作；未选时不得残留伪造的领域预约。
Status resource_reservation_absent(const ReservationRef &r) {
  const bool named_resource = r.domain_id == "resource_sites" ||
                              r.domain_id == "resource_site" ||
                              resource_kind(r.activity_kind);
  if (named_resource)
    return unimplemented(r.id);
  // 真 action/contract/org_activity 不从其普通 id 字符串猜成资源业务。
  switch (r.kind) {
  case ReservationRef::Kind::action:
  case ReservationRef::Kind::contract:
  case ReservationRef::Kind::org_activity:
  case ReservationRef::Kind::domain_activity:
    return common_success();
  }
  return common_failure("ResourceSitesInvalidReservationKind", r.id);
}
} // namespace
Status validate_unselected_resource_sites_catalog(const PreparedCatalog &p) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 76, "PreparedCatalog", "validate_unselected_resource_sites_catalog", &p);

  for (const auto &module : p.identity.module_versions)
    if (module.first == "resource_sites")
      return unimplemented(module.first);
  if (!p.common)
    return common_failure("ResourceSitesOriginalCommonCatalogRequired");
  // 原作者角色不能在未选模块时向玩家宣称已提供采集或调查服务。
  for (const auto &[id, role] : p.common->roles.rows())
    for (const auto &service : role.services)
      if (resource_kind(service))
        return unimplemented(id);
  return common_success();
}
Status validate_unselected_resource_sites_world(const WorldBundle &w) {
  if (!w.catalog)
    return common_failure("ResourceSitesOriginalCatalogRequired");
  auto valid = validate_unselected_resource_sites_catalog(*w.catalog);
  if (!valid.ok)
    return valid;
  const auto &identity = w.catalog->identity;
  const auto &common = w.common.state();
  if (common.metadata.content_id != identity.content_id ||
      common.metadata.content_version != identity.content_version ||
      common.metadata.content_digest != identity.content_digest ||
      common.metadata.ruleset_id != identity.ruleset_id)
    return common_failure("ResourceSitesActualCatalogIdentityRequired");
  valid = validate_native_item_craft_layout(w);
  if (!valid.ok)
    return valid;
  // Common 只存头；坏 domain 也不能让保留的采集 kind 逃过反扫。
  for (const auto &[id, fact] : common.facts.rows())
    if (fact.domain == "resource_sites" || fact.domain == "resource_site" ||
        resource_kind(fact.kind))
      return unimplemented(id);
  for (const auto &[id, action] : common.actions.rows())
    if (resource_kind(action.kind))
      return unimplemented(id);
  for (const auto &history : common.history)
    if (resource_kind(history.kind))
      return unimplemented(history.result_id);
  for (const auto &[id, result] : common.results.rows())
    if (result && resource_kind(result->kind))
      return unimplemented(id);
  for (const auto &[id, wallet] : common.wallets.rows())
    for (const auto &reservation : wallet.reserved) {
      valid = resource_reservation_absent(reservation.action_id);
      if (!valid.ok)
        return valid;
    }
  for (const auto &[id, inventory] : common.inventories.rows())
    for (const auto &lot : inventory.lots) {
      valid = resource_reservation_absent(lot.reserved_by);
      if (!valid.ok)
        return valid;
    }
  if (w.item_instances) {
    for (const auto &adoption : w.item_instances->output_adoptions) {
      if (adoption.source_kind == ItemOutputKind::resource_harvest)
        return unimplemented(adoption.key.source_ref.id);
      valid = original_source_absent(adoption.key.source_ref);
      if (!valid.ok)
        return valid;
    }
    for (const auto &result : w.item_instances->condition_results) {
      if (result.use_source &&
          std::holds_alternative<GatherToolWear>(*result.use_source))
        return unimplemented(result.result_id);
      valid = original_source_absent(result.source_fact_ref);
      if (!valid.ok)
        return valid;
      valid = original_source_absent(result.source_activity_ref);
      if (!valid.ok)
        return valid;
      for (const auto &source : result.resource_effect_refs) {
        valid = original_source_absent(source);
        if (!valid.ok)
          return valid;
      }
    }
    for (const auto &termination : w.item_instances->terminations) {
      valid = original_source_absent(termination.cause_ref);
      if (!valid.ok)
        return valid;
    }
  }
  if (w.lot_properties) {
    for (const auto &adoption : w.lot_properties->output_adoptions) {
      if (adoption.source_kind == ItemOutputKind::resource_harvest)
        return unimplemented(adoption.key.source_ref.id);
      valid = original_source_absent(adoption.key.source_ref);
      if (!valid.ok)
        return valid;
    }
    for (const auto &[id, properties] : w.lot_properties->properties.rows()) {
      valid = original_source_absent(properties.origin_receipt_ref);
      if (!valid.ok)
        return valid;
    }
  }
  return common_success();
}
} // namespace hanmo::resident
