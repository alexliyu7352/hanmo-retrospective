// 完整Care根唯一字段组合：实际catalog选3/4/5，不保存第二选择器。
#pragma once
#include "care_arrangement_codec_fields.hpp"
#include "care_coverage_codec_fields.hpp"
#include "care_education_codec_fields.hpp"
#include "care_facility_clearance_codec_fields.hpp"
#include "care_facility_disposition_codec_fields.hpp"
#include "care_facility_occupancy_codec_fields.hpp"
#include "care_period_codec_fields.hpp"
#include "resident/domains/family/care_requirements_state.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_requirements_state.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_location_scoped_codec.hpp"
#include "resident/storage/common_projection.hpp"
namespace hanmo::resident::care_requirements_codec {
// 只借Common原字段/同job冷读，carrier由实际已核目录提供，不由payload猜选。
template <class C>
Status original_location(C &c, Location &value, bool carrier_selected) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>) {
    if (c.carrier_location_enabled != carrier_selected)
      return c.reject("CareRootCommonLocationSelectionMismatch");
    return common_checkpoint::location(c, value);
  } else {
    return c.foreign_record(
        value,
        [carrier_selected](
            const BoundaryValue &raw,
            ScopedCheckpointJsonDecode &scope) -> Outcome<Location> {
          try {
            return {true,
                    boundary::decode_location(raw, "care_requirements/location",
                                              scope, carrier_selected),
                    {}};
          } catch (const boundary::DecodeFailure &error) {
            return {false, {}, error.error};
          }
        },
        [carrier_selected](ProjectionWriter &out, const Location &location) {
          return emit_common_location_fields(out, location, carrier_selected);
        });
  }
}
// 每张表只借其原owner字段表；未知T没有通用成功出口。
template <class C, class T, class LocationFields>
Status row(C &c, T &v, Integer version, LocationFields &location_fields) {
  if constexpr (std::is_same_v<T, CareRequirement>)
    return care_requirement_codec::requirement(c, v);
  else if constexpr (std::is_same_v<T, CareRequirementTransition>)
    return care_requirement_codec::transition(c, v);
  else if constexpr (std::is_same_v<T, AcceptedCaregiver>)
    return care_arrangement_codec::accepted(c, v, version);
  else if constexpr (std::is_same_v<T, CareBinding>)
    return care_coverage_codec::binding(c, v, version);
  else if constexpr (std::is_same_v<T, CareCoverageSatisfied>)
    return care_coverage_codec::satisfied(c, v, version);
  else if constexpr (std::is_same_v<T, CareCoverageLost>)
    return care_coverage_codec::lost(c, v, version);
  else if constexpr (std::is_same_v<T, CarePeriod>)
    return care_period_codec::period(c, v, version);
  else if constexpr (std::is_same_v<T, VersionedEducationPlan>)
    return care_education_codec::plan(c, v, version);
  else if constexpr (std::is_same_v<T, MaturityResult>)
    return care_maturity_codec::record(c, v);
  else if constexpr (std::is_same_v<T, CareHandoverExecution>)
    return care_handover_codec::execution(c, v, version);
  else if constexpr (std::is_same_v<T, CareTemporalEvent>)
    return care_requirement_codec::temporal_event(c, v);
  else if constexpr (std::is_same_v<T, CarePeriodSource>)
    return care_period_codec::original_source(c, v, version);
  else if constexpr (std::is_same_v<T, EducationChoice2>)
    return care_education_codec::choice(c, v);
  else if constexpr (std::is_same_v<T, EducationActivation2>)
    return care_education_codec::activation(c, v);
  else if constexpr (std::is_same_v<T, CareFacilityUseBinding>)
    return care_facility_codec::binding(c, v, version);
  else if constexpr (std::is_same_v<T, CareFacilityOccupancyEvent>)
    return care_facility_occupancy_codec::event(c, v);
  else if constexpr (std::is_same_v<T, CareFacilityDispositionResult>)
    return care_facility_disposition_codec::disposition(c, v);
  else if constexpr (std::is_same_v<T, FacilityClearanceExecution>)
    return care_facility_clearance_codec::execution(c, v, location_fields);
  else if constexpr (std::is_same_v<T, FacilityTransportAcceptance>)
    return care_facility_transport_codec::acceptance(c, v);
  else
    static_assert(!std::is_same_v<T, T>,
                  "Care root row lacks its original codec");
}
// 实际唯一catalog与世界情景先核，再访问任一表；未选/未来尾不被静默扔掉。
// LocationFields为瞬时原Common codec借用，不提供来源、权源或运行许可。
template <class C, class LocationFields>
Status root(C &c, CareRequirementsState &v, const PreparedCatalog &catalog,
            const StableId &scenario, LocationFields &location_fields) {
  auto valid = validate_care_requirements_state_shape(v, catalog, scenario);
  if (!valid.ok)
    return c.reject(valid.error.original.code);
  auto selected = read_native_care_root_capability_selection(catalog, scenario);
  if (!selected.ok)
    return c.reject(selected.error.original.code);
  if (!selected.value.physical_version)
    return c.reject("CareRootNotSelected");
  const Integer version = *selected.value.physical_version;
  auto one = [&](std::string_view name, auto &table) {
    auto fields = [&](auto &child, auto &value) {
      return row(child, value, version, location_fields);
    };
    if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
      return common_checkpoint::table(c, name, table, fields);
    else
      return c.table(name, table, fields);
  };
  // Binary有固定表序及尾部order；JSON写端只按原真实key序投影，不写第二元数据键。
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>) {
    valid = visit_selected_care_root_tables(v, catalog, scenario, one);
    if (!valid.ok)
      return c.reject(valid.error.original.code);
    c.sequence("field_order", v.field_order, [](auto &a, auto &x) {
      return care_requirement_codec::text(a, "value", x);
    });
  } else {
    if (c.reading() || v.field_order.empty())
      valid = visit_selected_care_root_tables(v, catalog, scenario, one);
    else
      for (const auto &requested : v.field_order) {
        valid = visit_selected_care_root_tables(
            v, catalog, scenario, [&](std::string_view name, auto &table) {
              return name == requested ? one(name, table) : c.status();
            });
        if (!valid.ok)
          break;
      }
    if (!valid.ok)
      return c.reject(valid.error.original.code);
  }
  if (!c.status().ok)
    return c.status();
  valid = validate_care_requirements_state_shape(v, catalog, scenario);
  return valid.ok ? c.status() : c.reject(valid.error.original.code);
}
// 冷JSON只借原树并按同job实际预算拥有key序；不造另一个Boundary封套。
template <class LocationFields>
Outcome<CareRequirementsState>
decode(const BoundaryValue &raw, const PreparedCatalog &catalog,
       const StableId &scenario, ScopedCheckpointJsonDecode &budget,
       LocationFields &location_fields) {
  if (ScopedCheckpointJsonDecode::current() != &budget)
    return {
        false, {}, common_failure("CareRootSameJsonCandidateRequired").error};
  const auto *fields = raw.as<BoundaryValue::Dict>();
  if (!fields)
    return {false, {}, common_failure("InvalidCareRootObject").error};
  auto selected = read_native_care_root_capability_selection(catalog, scenario);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (!selected.value.physical_version)
    return {false, {}, common_failure("CareRootNotSelected").error};
  auto count = care_root_table_count(*selected.value.physical_version);
  if (!count.ok)
    return {false, {}, count.error};
  if (fields->size() != count.value)
    return {false, {}, common_failure("CareRootFieldCountMismatch").error};
  // 所有表默认构造为空；原OrderedTable负责其后真实rows分配与按成员解码。
  budget.default_values<CareRequirementsState>();
  CareRequirementsState value;
  budget.collection("field_order", fields->size());
  budget.reserve_block(value.field_order, fields->size());
  budget.default_values<StableId>(fields->size());
  value.field_order.reserve(fields->size());
  for (const auto &field : *fields) {
    value.field_order.emplace_back();
    budget.text(value.field_order.back(), field.first);
  }
  ActorFoundationJsonVisitor visitor(raw);
  auto status = root(visitor, value, catalog, scenario, location_fields);
  if (status.ok)
    status = visitor.finish();
  return {status.ok, std::move(value), status.error};
}
// 只写投影借原拥有值，不复制十九表或为codec构第二状态。
template <class LocationFields>
Status emit(ProjectionWriter &writer, const CareRequirementsState &value,
            const PreparedCatalog &catalog, const StableId &scenario,
            LocationFields &location_fields) {
  ActorFoundationProjectionArchive archive(writer);
  return root(archive, const_cast<CareRequirementsState &>(value), catalog,
              scenario, location_fields);
}
} // namespace hanmo::resident::care_requirements_codec
