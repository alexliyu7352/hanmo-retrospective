#pragma once
#include "political_shop_licensing_exact2.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
#include "resident/domains/politics/treaty/treaty_revision_codec_fields.hpp"
namespace hanmo::resident::shop_exact2_fields {
template <class C> Status request(C &c, PrivateShopPermissionRequest &r) {
  c.field("actor_id", r.actor_id);
  c.field("premises_id", r.premises_id);
  return c.field("place_id", r.place_id);
}
template <class C> Status row(C &c, PoliticalShopRegistry &r) {
  c.field("id", r.id);
  c.field("court_id", r.court_id);
  c.field("tick", r.tick);
  if (c.reading())
    r.occurred_at.emplace();
  if (!r.occurred_at)
    return c.reject("PoliticalShopExactTimeMissing");
  c.object("occurred_at",
           [&](auto &a) { return exact_fields::moment(a, *r.occurred_at); });
  if (c.status().ok && r.occurred_at->half_day_tick != r.tick)
    return c.reject("PoliticalShopExactTickMismatch");
  return c.status();
}
template <class C> Status row(C &c, PoliticalWantedRecord &r) {
  c.field("id", r.id);
  c.field("court_id", r.court_id);
  c.field("actor_id", r.actor_id);
  c.field("reason_code", r.reason_code);
  c.field("registry_fact_id", r.registry_fact_id);
  c.field("grant_id", r.grant_id);
  c.optional("authority", r.authority, [&](auto &a, auto &value) {
    return treaty_revision_codec_fields::acting(a, value);
  });
  c.field("tick", r.tick);
  if (c.reading())
    r.occurred_at.emplace();
  if (!r.occurred_at)
    return c.reject("PoliticalShopExactTimeMissing");
  c.object("occurred_at",
           [&](auto &a) { return exact_fields::moment(a, *r.occurred_at); });
  if (c.status().ok && r.occurred_at->half_day_tick != r.tick)
    return c.reject("PoliticalShopExactTickMismatch");
  return c.status();
}
template <class C> Status row(C &c, PoliticalWantedRelease &r) {
  c.field("id", r.id);
  c.field("warrant_fact_id", r.warrant_fact_id);
  c.field("grant_id", r.grant_id);
  c.object("authority", [&](auto &a) {
    return treaty_revision_codec_fields::acting(a, r.authority);
  });
  c.field("tick", r.tick);
  if (c.reading())
    r.occurred_at.emplace();
  if (!r.occurred_at)
    return c.reject("PoliticalShopExactTimeMissing");
  c.object("occurred_at",
           [&](auto &a) { return exact_fields::moment(a, *r.occurred_at); });
  if (c.status().ok && r.occurred_at->half_day_tick != r.tick)
    return c.reject("PoliticalShopExactTickMismatch");
  return c.status();
}
template <class C> Status row(C &c, PoliticalShopClearance &r) {
  c.field("id", r.id);
  c.field("court_id", r.court_id);
  c.field("registry_fact_id", r.registry_fact_id);
  c.field("grant_id", r.grant_id);
  c.object("request", [&](auto &a) { return request(a, r.request); });
  c.object("authority", [&](auto &a) {
    return treaty_revision_codec_fields::acting(a, r.authority);
  });
  c.field("session_id", r.session_id);
  c.field("role_id", r.role_id);
  c.field("dialogue_id", r.dialogue_id);
  c.field("node_id", r.node_id);
  c.field("tick", r.tick);
  c.field("valid_through_tick", r.valid_through_tick);
  if (c.reading())
    r.occurred_at.emplace();
  if (!r.occurred_at)
    return c.reject("PoliticalShopExactTimeMissing");
  c.object("occurred_at",
           [&](auto &a) { return exact_fields::moment(a, *r.occurred_at); });
  if (c.status().ok && r.occurred_at->half_day_tick != r.tick)
    return c.reject("PoliticalShopExactTickMismatch");
  return c.status();
}
template <class C> Status row(C &c, PoliticalShopPermit &r) {
  c.field("id", r.id);
  c.field("clearance_fact_id", r.clearance_fact_id);
  c.field("tick", r.tick);
  if (c.reading())
    r.occurred_at.emplace();
  if (!r.occurred_at)
    return c.reject("PoliticalShopExactTimeMissing");
  c.object("occurred_at",
           [&](auto &a) { return exact_fields::moment(a, *r.occurred_at); });
  if (c.status().ok && r.occurred_at->half_day_tick != r.tick)
    return c.reject("PoliticalShopExactTickMismatch");
  return c.status();
}
template <class C> Status row(C &c, PoliticalShopRevocation &r) {
  c.field("id", r.id);
  c.field("permit_fact_id", r.permit_fact_id);
  c.field("grant_id", r.grant_id);
  c.object("authority", [&](auto &a) {
    return treaty_revision_codec_fields::acting(a, r.authority);
  });
  c.field("tick", r.tick);
  if (c.reading())
    r.occurred_at.emplace();
  if (!r.occurred_at)
    return c.reject("PoliticalShopExactTimeMissing");
  c.object("occurred_at",
           [&](auto &a) { return exact_fields::moment(a, *r.occurred_at); });
  if (c.status().ok && r.occurred_at->half_day_tick != r.tick)
    return c.reject("PoliticalShopExactTickMismatch");
  return c.status();
}
template <class C> Status state(C &c, PoliticalShopLicensingState &r) {
  c.field("version", r.version);
  if (!c.status().ok)
    return c.status();
  if (r.version != 2)
    return c.reject("UnsupportedPoliticalShopLicensingVersion");
  organization_lifecycle_fields::table(
      c, "registries", r.registries,
      [](auto &a, auto &v) { return row(a, v); });
  organization_lifecycle_fields::table(
      c, "warrants", r.warrants, [](auto &a, auto &v) { return row(a, v); });
  organization_lifecycle_fields::table(
      c, "releases", r.releases, [](auto &a, auto &v) { return row(a, v); });
  organization_lifecycle_fields::table(
      c, "clearances", r.clearances,
      [](auto &a, auto &v) { return row(a, v); });
  organization_lifecycle_fields::table(
      c, "permits", r.permits, [](auto &a, auto &v) { return row(a, v); });
  organization_lifecycle_fields::table(
      c, "revocations", r.revocations,
      [](auto &a, auto &v) { return row(a, v); });
  return c.status();
}
} // namespace hanmo::resident::shop_exact2_fields
