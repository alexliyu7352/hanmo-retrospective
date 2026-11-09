// 独立trade-grant/1单一grants根；binary/JSON共用严格字段表，不接受partial能力来源。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/kernel/source_ref.hpp"
#include "growth_trade_grant_codec.hpp"
#include "growth_capability_application_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 每行都指向原同key真实成交；新的有限ref只在显式trade_grants格式内开放。
template <class C> Status row(C &c, GrowthTradeGrant &value) {
  c.field("action_id", value.action_id);
  c.field("actor_id", value.actor_id);
  c.field("skill_id", value.skill_id);
  c.object("original_trade_result_ref", [&](auto &x) {
    x.field("domain", value.original_trade_result_ref.domain);
    x.field("kind", value.original_trade_result_ref.kind);
    return x.field("id", value.original_trade_result_ref.id);
  });
  c.field("original_trade_source_schema", value.original_trade_source_schema);
  c.field("xp_before", value.xp_before);
  c.field("actual_xp_gain", value.actual_xp_gain);
  c.field("xp_after", value.xp_after);
  c.optional("capability_application", value.capability_application,
             [](auto &x, auto &application) {
               return growth_capability_codec::application(
                   x, application, GrowthContributionWireVersion::trade_grants);
             });
  if (c.status().ok &&
      (value.action_id.empty() || value.actor_id.empty() ||
       value.skill_id != "commerce" ||
       value.original_trade_result_ref.domain != "growth" ||
       value.original_trade_result_ref.kind != "life_activity" ||
       value.original_trade_result_ref.id != value.action_id ||
       value.original_trade_source_schema != 2 || value.xp_before < 0 ||
       value.xp_before > 1200 ||
       value.actual_xp_gain != std::min<Integer>(10, 1200 - value.xp_before) ||
       value.xp_after != value.xp_before + value.actual_xp_gain ||
       bool(value.capability_application) != (value.actual_xp_gain > 0)))
    return c.reject("InvalidGrowthTradeGrant");
  return c.status();
}
// JSON保原有序字典；binary沿原key/value表归档并拒重复key。
template <class C>
Status table(C &c, OrderedTable<GrowthTradeGrant> &values) {
  auto result = c.table("grants", values,
                        [](auto &x, auto &value) { return row(x, value); });
  if (!result.ok)
    return result;
  for (const auto &entry : values.rows())
    if (entry.first != entry.second.action_id)
      return c.reject("GrowthTradeGrantKeyMismatch", entry.first);
  return c.status();
}
Status table(CodecVisitor &c, OrderedTable<GrowthTradeGrant> &values) {
  auto result = common_checkpoint::table(
      c, "grants", values,
      [](auto &x, auto &value) { return row(x, value); });
  if (!result.ok)
    return result;
  for (const auto &entry : values.rows())
    if (entry.first != entry.second.action_id)
      return c.reject("GrowthTradeGrantKeyMismatch", entry.first);
  return c.status();
}
} // namespace
Status visit_growth_trade_grants_v1(CodecVisitor &c,
                                    OrderedTable<GrowthTradeGrant> &values) {
  return table(c, values);
}
Status emit_growth_trade_grants_v1_projection(
    ProjectionWriter &sink, std::string_view name,
    const OrderedTable<GrowthTradeGrant> &values) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, values,
      [](auto &c, auto &value) { return table(c, value); });
}
Outcome<BoundaryValue>
encode_growth_trade_grants_v1(const OrderedTable<GrowthTradeGrant> &values) {
  BoundaryValue out;
  BoundaryProjectionWriter sink(out);
  auto result = emit_growth_trade_grants_v1_projection(sink, "", values);
  if (result.ok)
    result = sink.finish();
  return {result.ok, std::move(out), result.error};
}
Outcome<OrderedTable<GrowthTradeGrant>>
decode_growth_trade_grants_v1(const BoundaryValue &input) {
  OrderedTable<GrowthTradeGrant> out;
  ActorFoundationJsonVisitor c(input);
  auto result = table(c, out);
  if (result.ok)
    result = c.finish();
  return {result.ok, std::move(out), result.error};
}
} // namespace hanmo::resident
