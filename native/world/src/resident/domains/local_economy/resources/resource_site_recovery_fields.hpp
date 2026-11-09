// 仅把已存在的恢复 DTO 列为唯一字段表；opening 临时借用不进入存档。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resource_site_recovery.hpp"
#include <limits>
namespace hanmo::resident::resource_recovery_fields {
using namespace exact_fields;
template <class C> Status begin(C &c) {
  if constexpr (std::is_same_v<std::decay_t<C>, ActorFoundationJsonVisitor>)
    return c.ordered_fields();
  return c.status();
}
template <class C> Status rule_ref(C &c, ActorFoundationRuleRef &v) {
  begin(c);
  required_ref(c, "id", v.id);
  c.field("version", v.version);
  required_ref(c, "catalog_digest", v.catalog_digest);
  if (c.status().ok && v.version <= 0)
    return c.reject("InvalidResourceRuleVersion", v.id);
  return c.status();
}
template <class C> Status season(C &c, ResourceRecoverySeasonRow &v) {
  begin(c);
  required_ref(c, "season_id", v.season_id);
  c.field("from_tick", v.from_tick);
  c.field("until_tick", v.until_tick);
  c.field("numerator", v.numerator);
  if (c.status().ok &&
      (v.from_tick < 0 || v.until_tick <= v.from_tick || v.numerator < 0))
    return c.reject("InvalidResourceRecoverySeason", v.season_id);
  return c.status();
}
template <class C> Status rule(C &c, ResourceRecoveryRule &v) {
  begin(c);
  c.object("rule_ref", [&](auto &out) { return rule_ref(out, v.rule_ref); });
  c.field("denominator", v.denominator);
  c.field("base_numerator", v.base_numerator);
  c.sequence("season_rows", v.season_rows,
             [](auto &out, auto &row) { return season(out, row); });
  Integer disabled = static_cast<Integer>(v.disabled_policy);
  finite(c, "disabled_policy", disabled, 1);
  if (c.status().ok && c.reading())
    v.disabled_policy = static_cast<ResourceDisabledRecoveryPolicy>(disabled);
  if (c.status().ok) {
    const auto valid = validate_resource_recovery_rule(v);
    if (!valid.ok)
      return c.reject("InvalidResourceRecoveryRule", v.rule_ref.id);
  }
  return c.status();
}
template <class C>
Status ids(C &c, const char *name, CheckpointVector<StableId> &v) {
  std::size_t index = 0;
  return c.sequence(name, v, [&](auto &out, auto &id) {
    required_ref(out, "", id);
    if (out.status().ok)
      for (std::size_t prior = 0; prior < index; ++prior)
        if (v[prior] == id)
          return out.reject("DuplicateResourceModifierReference", id);
    ++index;
    return out.status();
  });
}
template <class C> Status modifier(C &c, ResourceDepletionModifier &v) {
  begin(c);
  required_ref(c, "modifier_id", v.modifier_id);
  required_ref(c, "source_gather_receipt", v.source_gather_receipt);
  c.field("effective_from", v.effective_from);
  c.field("until_exclusive", v.until_exclusive);
  c.object("rule_ref", [&](auto &out) { return rule_ref(out, v.rule_ref); });
  c.field("numerator_adjustment", v.numerator_adjustment);
  if (c.status().ok &&
      (v.effective_from < 0 || v.until_exclusive <= v.effective_from))
    return c.reject("InvalidResourceModifier", v.modifier_id);
  return c.status();
}
template <class C> Status state(C &c, ResourceSiteState &v) {
  begin(c);
  required_ref(c, "site_id", v.site_id);
  c.field("remaining_units", v.remaining_units);
  c.field("last_recovery_tick", v.last_recovery_tick);
  c.field("rate_remainder", v.rate_remainder);
  ids(c, "active_depletion_modifier_refs", v.active_depletion_modifier_refs);
  c.field("revision", v.revision);
  c.object("recovery_rule_ref",
           [&](auto &out) { return rule_ref(out, v.recovery_rule_ref); });
  c.field("recovery_denominator", v.recovery_denominator);
  if (c.status().ok &&
      (v.remaining_units < 0 || v.last_recovery_tick < 0 ||
       v.rate_remainder < 0 || v.recovery_denominator <= 0 ||
       v.rate_remainder >= v.recovery_denominator || v.revision < 0))
    return c.reject("InvalidResourceRecoveryState", v.site_id);
  // 容量、同规则分母、原 modifier 和 claim 覆盖仍须原资源根联合校验。
  return c.status();
}
template <class C> Status original(C &c, MomentOwnerSource &v) {
  begin(c);
  c.object("source_ref", [&](auto &out) {
    begin(out);
    return source(out, v.source_ref);
  });
  c.field("source_schema", v.source_schema);
  if (c.status().ok && v.source_schema <= 0)
    return c.reject("InvalidResourceRecoveryStateSource");
  return c.status();
}
template <class C> Status receipt(C &c, ResourceRecoveryReceipt &v) {
  begin(c);
  required_ref(c, "site_id", v.site_id);
  c.field("from_tick", v.from_tick);
  c.field("to_tick", v.to_tick);
  c.object("rule_ref", [&](auto &out) { return rule_ref(out, v.rule_ref); });
  c.field("season_id", v.season_id);
  ids(c, "modifier_source_refs", v.modifier_source_refs);
  c.field("standing_before", v.standing_before);
  c.field("standing_after", v.standing_after);
  c.field("recovered_units", v.recovered_units);
  c.field("discarded_whole_units", v.discarded_whole_units);
  c.field("remainder_before", v.remainder_before);
  c.field("remainder_after", v.remainder_after);
  c.field("denominator", v.denominator);
  std::size_t index = 0;
  c.sequence("state_source_refs", v.state_source_refs,
             [&](auto &out, auto &row) {
               original(out, row);
               if (out.status().ok)
                 for (std::size_t prior = 0; prior < index; ++prior)
                   if (same_source_ref(v.state_source_refs[prior].source_ref,
                                       row.source_ref))
                     return out.reject("DuplicateResourceRecoveryStateSource",
                                       row.source_ref.id);
               ++index;
               return out.status();
             });
  if (c.status().ok &&
      (v.from_tick < 0 || v.from_tick == std::numeric_limits<Integer>::max() ||
       v.to_tick != v.from_tick + 1 || v.standing_before < 0 ||
       v.standing_after < v.standing_before ||
       v.recovered_units != v.standing_after - v.standing_before ||
       v.discarded_whole_units < 0 || v.denominator <= 0 ||
       v.remainder_before < 0 || v.remainder_before >= v.denominator ||
       v.remainder_after < 0 || v.remainder_after >= v.denominator))
    return c.reject("InvalidResourceRecoveryReceipt", v.site_id);
  // 这里只验既有收据形状，不用保存后站点推导或补跑恢复公式。
  return c.status();
}
} // namespace hanmo::resident::resource_recovery_fields
