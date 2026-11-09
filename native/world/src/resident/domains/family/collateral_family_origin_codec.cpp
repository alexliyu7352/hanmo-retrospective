// 两条连续真实亲代路径保原Tagged形状；结构自洽不能替代事实前重读。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "collateral_family_origin.hpp"
#include "family_projection.hpp"
#include "family_source_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "succession_personal_assets.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
const std::set<StableId> fields{"first_actor_id", "second_actor_id",
                                "common_ancestor_actor_id", "first_descent",
                                "second_descent"};
bool reference(const StableId &id) {
  return !id.empty() && id.size() <= 256;
}
Status shape(const CollateralFamilyOrigin &s) {
  if (!reference(s.first_actor_id) || !reference(s.second_actor_id) ||
      !reference(s.common_ancestor_actor_id) ||
      s.first_actor_id == s.second_actor_id ||
      s.common_ancestor_actor_id == s.first_actor_id ||
      s.common_ancestor_actor_id == s.second_actor_id)
    return common_failure("InvalidCollateralFamilyParties");
  if (!s.field_order.empty() &&
      (s.field_order.size() != fields.size() ||
       std::set<StableId>(s.field_order.begin(), s.field_order.end()) !=
           fields))
    return common_failure("BoundaryFieldOrderMismatch");
  std::set<StableId> children;
  for (const auto *path : {&s.first_descent, &s.second_descent}) {
    if (path->empty() || path->size() > CodecVisitor::max_items)
      return common_failure("InvalidCollateralFamilyPath");
    StableId parent = s.common_ancestor_actor_id;
    for (const auto &edge : *path) {
      if (edge.parent_actor_id != parent || !reference(edge.child_actor_id) ||
          edge.child_actor_id == s.common_ancestor_actor_id ||
          !children.insert(edge.child_actor_id).second)
        return common_failure("InvalidCollateralFamilyPath");
      parent = edge.child_actor_id;
    }
    if (parent !=
        (path == &s.first_descent ? s.first_actor_id : s.second_actor_id))
      return common_failure("InvalidCollateralFamilyPath");
  }
  return common_success();
}
Outcome<std::vector<BiologicalControlOrigin>> decode_path(const V &value) {
  const auto *a = std::get_if<V::Array>(&value.data);
  if (!a || a->empty() || a->size() > CodecVisitor::max_items)
    return {false, {}, common_failure("InvalidCollateralFamilyPath").error};
  std::vector<BiologicalControlOrigin> out;
  checkpoint_decode_boundary_array(a->size(), sizeof(BiologicalControlOrigin));
  out.reserve(a->size());
  for (const auto &item : *a) {
    auto edge = decode_biological_control_origin(item);
    if (!edge.ok)
      return {false, {}, edge.error};
    out.push_back(std::move(edge.value));
  }
  return {true, std::move(out), {}};
}
} // namespace
Outcome<CollateralFamilyOrigin>
decode_collateral_family_origin(const BoundaryValue &v) {
  try {
    auto unique = validate_succession_json_unique(v);
    if (!unique.ok)
      return {false, {}, unique.error};
    boundary::fields(v,
                     {"first_actor_id", "second_actor_id",
                      "common_ancestor_actor_id", "first_descent",
                      "second_descent"},
                     {}, "collateral_family_origin");
    CollateralFamilyOrigin s;
    s.first_actor_id = boundary::get_string(v, "first_actor_id", "");
    s.second_actor_id = boundary::get_string(v, "second_actor_id", "");
    s.common_ancestor_actor_id =
        boundary::get_string(v, "common_ancestor_actor_id", "");
    auto a = decode_path(*v.find("first_descent")),
         b = decode_path(*v.find("second_descent"));
    if (!a.ok)
      return {false, {}, a.error};
    if (!b.ok)
      return {false, {}, b.error};
    s.first_descent = std::move(a.value);
    s.second_descent = std::move(b.value);
    s.field_order = boundary::order(v);
    auto valid = shape(s);
    return valid.ok ? Outcome<CollateralFamilyOrigin>{true, std::move(s), {}}
                    : Outcome<CollateralFamilyOrigin>{false, {}, valid.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status emit_collateral_family_origin_fields(ProjectionWriter &w,
                                            const CollateralFamilyOrigin &s) {
  auto valid = shape(s);
  if (!valid.ok)
    return w.reject(valid.error.original.code, "collateral_family_origin");
  w.field("first_actor_id", s.first_actor_id);
  w.field("second_actor_id", s.second_actor_id);
  w.field("common_ancestor_actor_id", s.common_ancestor_actor_id);
  w.sequence("first_descent", s.first_descent,
             [](auto &rows, const auto &edge) {
               return rows.object("", [&](auto &item) {
                 return emit_biological_control_origin_fields(item, edge);
               });
             });
  w.sequence("second_descent", s.second_descent,
             [](auto &rows, const auto &edge) {
               return rows.object("", [&](auto &item) {
                 return emit_biological_control_origin_fields(item, edge);
               });
             });
  return w.order_fields(s.field_order);
}
BoundaryValue encode_collateral_family_origin(const CollateralFamilyOrigin &s) {
  return family_projection::encode(
      [&](auto &w) { return emit_collateral_family_origin_fields(w, s); });
}
Status visit_collateral_family_origin_checkpoint(CodecVisitor &c,
                                                 CollateralFamilyOrigin &s) {
  if (!c.reading()) {
    auto valid = shape(s);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  c.field("first_actor_id", s.first_actor_id);
  c.field("second_actor_id", s.second_actor_id);
  c.field("common_ancestor_actor_id", s.common_ancestor_actor_id);
  c.sequence("first_descent", s.first_descent,
             [](CodecVisitor &v, BiologicalControlOrigin &e) {
               return v.object("edge", [&](CodecVisitor &n) {
                 return visit_biological_control_origin_checkpoint(n, e);
               });
             });
  c.sequence("second_descent", s.second_descent,
             [](CodecVisitor &v, BiologicalControlOrigin &e) {
               return v.object("edge", [&](CodecVisitor &n) {
                 return visit_biological_control_origin_checkpoint(n, e);
               });
             });
  common_checkpoint::strings(c, "field_order", s.field_order);
  if (!c.status().ok)
    return c.status();
  auto valid = shape(s);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
} // namespace hanmo::resident
