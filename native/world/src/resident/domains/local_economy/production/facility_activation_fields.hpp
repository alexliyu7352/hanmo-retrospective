// 首次 production_work_applications/1 候选唯一字段序；不修改原设施 /1。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "facility_activation_model.hpp"
#include "facility_construction_fields.hpp"
#include "resident/domains/tasks/work_execution/work_result_source_codec.hpp"
namespace hanmo::resident::facility_activation_fields {
using namespace exact_fields;
using facility_construction_fields::begin;
template <class C> Status work(C &c, WorkRef &v) {
  begin(c);
  required_ref(c, "owner_domain", v.owner_domain);
  Integer kind = static_cast<Integer>(v.kind);
  finite(c, "kind", kind, 2);
  if (c.status().ok && c.reading())
    v.kind = static_cast<EconomicWorkKind>(kind);
  return required_ref(c, "work_id", v.work_id);
}
template <class C>
Status economic_result(C &c, ProductionApplicationEconomicResult &v) {
  begin(c);
  Integer kind = v.index();
  finite(c, "kind", kind, 1);
  if (!c.status().ok)
    return c.status();
  if (c.reading()) {
    if (kind == 0) {
      if (!prepare_checkpoint_default_values<
               ProductionApplicationCompletedPeriod>(c)
               .ok)
        return c.status();
      v = ProductionApplicationCompletedPeriod{};
    } else {
      if (!prepare_checkpoint_default_values<
               ProductionApplicationCompletedStage>(c)
               .ok)
        return c.status();
      v = ProductionApplicationCompletedStage{};
    }
  }
  if (auto *period = std::get_if<ProductionApplicationCompletedPeriod>(&v))
    return c.object("receipt_ref", [&](auto &out) {
      begin(out);
      return source(out, period->receipt_ref);
    });
  auto &stage = std::get<ProductionApplicationCompletedStage>(v);
  return c.object("stage_result_ref", [&](auto &out) {
    begin(out);
    return source(out, stage.stage_result_ref);
  });
}
template <class C> Status work_source(C &c, WorkApplicationSource &v) {
  begin(c);
  Integer kind = v.index();
  finite(c, "kind", kind, 1);
  if (!c.status().ok)
    return c.status();
  if (c.reading()) {
    if (kind == 0) {
      if (!prepare_checkpoint_default_values<ProductionApplicationS3Work>(c).ok)
        return c.status();
      v = ProductionApplicationS3Work{};
    } else {
      if (!prepare_checkpoint_default_values<ProductionApplicationEconomicWork>(
               c)
               .ok)
        return c.status();
      v = ProductionApplicationEconomicWork{};
    }
  }
  if (auto *supervised = std::get_if<ProductionApplicationS3Work>(&v))
    return c.object("source", [&](auto &out) {
      begin(out);
      return work_source_codec::result(out, supervised->source);
    });
  auto &economic = std::get<ProductionApplicationEconomicWork>(v);
  c.object("work_ref", [&](auto &out) { return work(out, economic.work_ref); });
  return c.object("result", [&](auto &out) {
    return economic_result(out, economic.result);
  });
}
template <class C>
Status sources(C &c, const char *name, std::vector<SourceRef> &values) {
  std::size_t index = 0;
  return c.sequence(name, values, [&](auto &out, auto &row) {
    begin(out);
    source(out, row);
    if (out.status().ok)
      for (std::size_t prior = 0; prior < index; ++prior)
        if (values[prior].domain == row.domain &&
            values[prior].kind == row.kind && values[prior].id == row.id)
          return out.reject("DuplicateFacilityActivationSource", row.id);
    ++index;
    return out.status();
  });
}
template <class C> Status receipt(C &c, FacilityActivationReceipt &v) {
  begin(c);
  required_ref(c, "receipt_id", v.receipt_id);
  c.object("source_work_fact",
           [&](auto &out) { return work_source(out, v.source_work_fact); });
  required_ref(c, "asset_ref", v.asset_ref);
  sources(c, "material_receipt_refs", v.material_receipt_refs);
  sources(c, "worker_result_refs", v.worker_result_refs);
  c.field("capacity_units", v.capacity_units);
  required_ref(c, "capacity_unit_id", v.capacity_unit_id);
  c.field("active_from_tick", v.active_from_tick);
  if (c.status().ok && v.capacity_units < 0)
    return c.reject("FacilityActivationNegativeCapacity", v.receipt_id);
  return c.status();
}
template <class C> Status application(C &c, AppliedFacilityActivation &v) {
  begin(c);
  c.object("receipt", [&](auto &out) { return receipt(out, v.receipt); });
  c.object("applied_at", [&](auto &out) {
    begin(out);
    return moment(out, v.applied_at);
  });
  return c.object("adopted_definition_ref", [&](auto &out) {
    return facility_construction_fields::rule(out, v.adopted_definition_ref);
  });
}
template <class C> Status state(C &c, ProductionWorkApplicationsState &v) {
  begin(c);
  c.field("schema_version", v.schema_version);
  if (c.status().ok && v.schema_version != 1)
    return c.reject("UnsupportedProductionWorkApplicationsVersion");
  std::size_t index = 0;
  return c.sequence(
      "facility_activations", v.facility_activations,
      [&](auto &out, auto &row) {
        application(out, row);
        if (out.status().ok)
          for (std::size_t prior = 0; prior < index; ++prior)
            if (v.facility_activations[prior].receipt.receipt_id ==
                row.receipt.receipt_id)
              return out.reject("DuplicateFacilityActivationReceipt",
                                row.receipt.receipt_id);
        ++index;
        return out.status();
      });
}
} // namespace hanmo::resident::facility_activation_fields
