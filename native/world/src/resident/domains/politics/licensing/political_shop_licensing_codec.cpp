// 独立新段严格逐字段保存；旧政治和Commerce字节不追加隐式许可。
#include "political_shop_licensing.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template <class A> void record(A &a, PrivateShopPermissionRequest &v) {
  a("actor_id", v.actor_id);
  a("premises_id", v.premises_id);
  a("place_id", v.place_id);
}
template <class A> void record(A &a, PoliticalWantedSeed &v) {
  a("actor_id", v.actor_id);
  a("reason_code", v.reason_code);
}
template <class A> void record(A &a, PoliticalShopCourt &v) {
  a("id", v.id);
  a("jurisdiction_id", v.jurisdiction_id);
  a("office_id", v.office_id);
  a("role_id", v.role_id);
  a("dialogue_id", v.dialogue_id);
  a("request_node_id", v.request_node_id);
  a("premises_ids", v.premises_ids);
  a("warrant_reason_codes", v.warrant_reason_codes);
  a("initial_wanted", v.initial_wanted);
  a("complete_registry", v.complete_registry);
  a("permit_ticks", v.permit_ticks);
}
template <class A> void record(A &a, PoliticalShopLicensingCatalog &v) {
  a("version", v.version);
  a("courts", v.courts);
}
template <class A> void record(A &a, PoliticalShopRegistry &v) {
  a("id", v.id);
  a("court_id", v.court_id);
  a("tick", v.tick);
}
template <class A> void record(A &a, PoliticalWantedRecord &v) {
  a("id", v.id);
  a("court_id", v.court_id);
  a("actor_id", v.actor_id);
  a("reason_code", v.reason_code);
  a("registry_fact_id", v.registry_fact_id);
  a("grant_id", v.grant_id);
  a("authority", v.authority);
  a("tick", v.tick);
}
template <class A> void record(A &a, PoliticalWantedRelease &v) {
  a("id", v.id);
  a("warrant_fact_id", v.warrant_fact_id);
  a("grant_id", v.grant_id);
  a("authority", v.authority);
  a("tick", v.tick);
}
template <class A> void record(A &a, PoliticalShopClearance &v) {
  a("id", v.id);
  a("court_id", v.court_id);
  a("registry_fact_id", v.registry_fact_id);
  a("grant_id", v.grant_id);
  a("request", v.request);
  a("authority", v.authority);
  a("session_id", v.session_id);
  a("role_id", v.role_id);
  a("dialogue_id", v.dialogue_id);
  a("node_id", v.node_id);
  a("tick", v.tick);
  a("valid_through_tick", v.valid_through_tick);
}
template <class A> void record(A &a, PoliticalShopPermit &v) {
  a("id", v.id);
  a("clearance_fact_id", v.clearance_fact_id);
  a("tick", v.tick);
}
template <class A> void record(A &a, PoliticalShopRevocation &v) {
  a("id", v.id);
  a("permit_fact_id", v.permit_fact_id);
  a("grant_id", v.grant_id);
  a("authority", v.authority);
  a("tick", v.tick);
}
template <class A> void record(A &a, PoliticalShopLicensingState &v) {
  a("version", v.version);
  a("registries", v.registries);
  a("warrants", v.warrants);
  a("releases", v.releases);
  a("clearances", v.clearances);
  a("permits", v.permits);
  a("revocations", v.revocations);
}
} // namespace hanmo::resident::dynamic_economy_codec
namespace hanmo::resident {
namespace {
// 直接BoundaryValue也递归拒重复键；文本JSON的SAX门不是唯一入口。
template <class T>
Outcome<T> decode_law(const BoundaryValue &v, const StableId &path) {
  auto guard = validate_political_takeover_payload(v);
  if (!guard.ok)
    return {false, {}, guard.error};
  try {
    T out;
    dynamic_economy_codec::decode(v, out, path);
    if (out.version != 1)
      return {false,
              {},
              common_failure("UnsupportedPoliticalShopLicensingVersion").error};
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_political_shop_licensing_catalog_fields(
    ProjectionWriter &writer, const PoliticalShopLicensingCatalog &value) {
  try {
    return dynamic_economy_codec::emit_fields(writer, value);
  } catch (const boundary::DecodeFailure &error) {
    return writer.reject(error.error.original.code, error.error.original.path);
  }
}
BoundaryValue encode_political_shop_licensing_catalog(
    const PoliticalShopLicensingCatalog &value) {
  return politics_projection_detail::encode_fields(
      value, emit_political_shop_licensing_catalog_fields);
}
Outcome<PoliticalShopLicensingCatalog>
decode_political_shop_licensing_catalog(const BoundaryValue &v) {
  return decode_law<PoliticalShopLicensingCatalog>(
      v, "/political_shop_licensing_catalog");
}
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status
emit_political_shop_licensing_fields(ProjectionWriter &writer,
                                     const PoliticalShopLicensingState &value) {
  if (value.version != 1)
    return writer.reject("UnsupportedPoliticalShopLicensingVersion");
  auto valid = validate_political_shop_licensing_time_layout(value);
  if (!valid.ok)
    return writer.reject(valid.error.original.code);
  try {
    return dynamic_economy_codec::emit_fields(writer, value);
  } catch (const boundary::DecodeFailure &error) {
    return writer.reject(error.error.original.code, error.error.original.path);
  }
}
BoundaryValue
encode_political_shop_licensing(const PoliticalShopLicensingState &value) {
  return politics_projection_detail::encode_fields(
      value, emit_political_shop_licensing_fields);
}
Outcome<PoliticalShopLicensingState>
decode_political_shop_licensing(const BoundaryValue &v) {
  return decode_law<PoliticalShopLicensingState>(v,
                                                 "/political_shop_licensing");
}
Status
visit_political_shop_licensing_checkpoint(CodecVisitor &a,
                                          PoliticalShopLicensingState &v) {
  if (!a.reading()) {
    auto valid = validate_political_shop_licensing_time_layout(v);
    if (!valid.ok)
      return a.reject(valid.error.original.code);
  }
  if (!a.reading() && v.version != 1)
    return a.reject("UnsupportedPoliticalShopLicensingVersion");
  auto result =
      dynamic_economy_codec::checkpoint(a, political_shop_licensing_module, v);
  return !result.ok ? result
         : v.version == 1
             ? common_success()
             : a.reject("UnsupportedPoliticalShopLicensingVersion");
}
} // namespace hanmo::resident
