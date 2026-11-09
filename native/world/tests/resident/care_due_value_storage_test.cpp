// Family 原值的单一存读回归，不构造 World，不冒真实出生、供养、清场或命名门。
// 测试中的来源地址仅作为 codec 数据；没有 source reader 将它们判为已发生事实。
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/actor_foundation/facility_construction_capability.hpp"
#include "resident/domains/actor_foundation/family_personal_capability.hpp"
#include "resident/domains/family/care_body_scope.hpp"
#include "resident/domains/family/care_clearance_history_values.hpp"
#include "resident/domains/family/care_clearance_origin_codec_fields.hpp"
#include "resident/domains/family/care_handover_execution_codec_fields.hpp"
#include "resident/domains/family/care_maturity_record.hpp"
#include "resident/domains/family/care_physical_position_codec_fields.hpp"
#include "resident/domains/family/care_requirements_fact_headers.hpp"
#include "resident/domains/family/care_root_layout.hpp"
#include "resident/domains/family/care_transport_carrier_codec_fields.hpp"
#include "resident/domains/family/collateral_personal_command_source.hpp"
#include "resident/domains/family/collateral_personal.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace hanmo::resident;
namespace {
// 每个失败指出原存读条件，不把组件断言包装成全世界验收。
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
// 只测真实字段 visitor 的边界往返，不提供任何业务成功 mock。
template <class T, class Fields> Outcome<T> round_trip(T value, Fields fields) {
  BoundaryValue encoded;
  ActorFoundationJsonVisitor writer(encoded);
  auto status = fields(writer, value);
  if (status.ok)
    status = writer.finish();
  if (!status.ok)
    return {false, {}, status.error};
  T decoded;
  const BoundaryValue &input = encoded;
  ActorFoundationJsonVisitor reader(input);
  status = fields(reader, decoded);
  if (status.ok)
    status = reader.finish();
  return {status.ok, std::move(decoded), status.error};
}
// 原空顺序可 canonical 输出；已有顺序必须精确本版集合且不含未来键。
void root_order() {
  check(care_root_table_count(3).ok && care_root_table_count(3).value == 12,
        "Care3 root count changed");
  check(care_root_table_count(4).ok && care_root_table_count(4).value == 14,
        "Care4 root count changed");
  check(care_root_table_count(5).ok && care_root_table_count(5).value == 19,
        "Care5 root count changed");
  for (const Integer version : {Integer{0}, Integer{1}, Integer{2}, Integer{6}})
    check(!care_root_table_count(version).ok,
          "unsupported root layout accepted");
  FieldOrder order;
  for (const auto key : care_root_table_names)
    order.emplace_back(key);
  check(validate_care_root_field_order(order, 5).ok,
        "full Care5 order rejected");
  std::reverse(order.begin(), order.end());
  check(validate_care_root_field_order(order, 5).ok,
        "saved field order discarded");
  check(!validate_care_root_field_order(order, 4).ok,
        "future Care5 fields accepted in Care4");
  order[0] = order[1];
  check(!validate_care_root_field_order(order, 5).ok,
        "duplicate root field accepted");
  order[0] = "invented_care_root";
  check(!validate_care_root_field_order(order, 5).ok,
        "unknown root field accepted");
  check(validate_care_root_field_order({}, 3).ok,
        "fresh canonical order rejected");
}
// 匿名身体份额保实际嵌套引用与已知子集，绝不由死亡数自动减人。
void body_scope() {
  CareResidenceBodyScope row;
  row.agreement_id = "residence";
  row.intake_line_id = "line";
  row.actual_scope_source_ref = {"estate", "residence_physical_occupancy",
                                 "scope"};
  row.actual_scope_schema = 1;
  row.body_count = 6;
  row.known_named_actor_ids = {"known_actor"};
  row.unresolved_loss_source_refs = {
      {{"local_economy", "population_loss", "loss"}, 1}};
  auto encoded = encode_care_body_scope(CareBodyScopeSlice{row});
  check(encoded.ok, "complete ResidenceSlice could not encode");
  const auto decoded = decode_care_body_scope(encoded.value);
  check(decoded.ok &&
            std::holds_alternative<CareResidenceBodyScope>(decoded.value),
        "ResidenceSlice tag lost");
  const auto &actual = std::get<CareResidenceBodyScope>(decoded.value);
  check(actual.body_count == 6 &&
            actual.known_named_actor_ids == row.known_named_actor_ids &&
            actual.unresolved_loss_source_refs.size() == 1 &&
            actual.unresolved_loss_source_refs[0].source_ref.id == "loss",
        "ResidenceSlice deep fields lost");
  row.known_named_actor_ids.push_back("known_actor");
  check(!encode_care_body_scope(CareBodyScopeSlice{row}).ok,
        "duplicate known body accepted");
  row.known_named_actor_ids.clear();
  row.body_count = 0;
  check(!encode_care_body_scope(CareBodyScopeSlice{row}).ok,
        "zero ResidenceSlice accepted as Empty");
  CareNamedBodyScope named{
      "child", "other_actor", InitialConditionSeedRef{"seed"}, {}};
  check(!encode_care_body_scope(CareBodyScopeSlice{named}).ok,
        "wrong Origin actor key accepted");
}
// Maturity 仅保存九字段，不因 typed record 有值就宣称依赖适用规则通过。
void maturity() {
  MaturityResult value;
  value.result_id = "maturity";
  value.actor_id = "child";
  value.age_basis_ref = BirthFactRef{"birth"};
  value.calendar_profile_ref = {original_calendar_rule_id, 1, "digest"};
  value.calendar_boundary_ordinal = 18;
  value.effective_tick = 18 * 720;
  value.original_condition_or_life_boundary_ref = {
      {"family_life", "original_boundary", "boundary"}, 1};
  value.previous_stage = CareAgeStage::apprentice;
  value.new_stage = CareAgeStage::adult;
  const auto encoded = encode_care_maturity_record(value);
  check(encoded.ok, "complete MaturityResult could not encode");
  const auto decoded = decode_care_maturity_record(encoded.value);
  check(decoded.ok && decoded.value.effective_tick == value.effective_tick &&
            decoded.value.new_stage == CareAgeStage::adult,
        "MaturityResult did not preserve original values");
  value.new_stage = static_cast<CareAgeStage>(4);
  check(!encode_care_maturity_record(value).ok, "unknown age stage accepted");
}
// 同一 handover 期只保存 ref，不能用错源或未准入 field_order 混成本地已供服务。
void work_gate() {
  FacilityClearanceWorkGate value;
  value.payload = FacilityClearanceOriginalCarePeriod{
      {"care_requirements", "care_handover_period", "period"}, 2};
  value.field_order = {"payload", "tag", "field_order"};
  const auto encoded = encode_facility_clearance_work_gate(value);
  check(encoded.ok, "original handover period reference rejected");
  const auto decoded = decode_facility_clearance_work_gate(encoded.value);
  check(decoded.ok && decoded.value.field_order == value.field_order &&
            decoded.value.payload.index() == 1,
        "work gate reference or order changed");
  value.field_order[0] = "unknown";
  check(!encode_facility_clearance_work_gate(value).ok,
        "unknown local value field order accepted");
  value.field_order.clear();
  std::get<FacilityClearanceOriginalCarePeriod>(value.payload).source_schema =
      1;
  check(!encode_facility_clearance_work_gate(value).ok,
        "old care period schema accepted");
}
// Sea 原来源严格完整七字段；此存读不提供 S2 工作完成或载荷能力。
void work_origin() {
  ClearanceSeaWorkOrigin sea;
  sea.action_ref = "sea_action";
  sea.care_transport_binding_ref = {"sea_action", "care_binding"};
  sea.terms_source_ref = {"military", "sea_transfer_terms", "terms"};
  sea.action_elapsed_ref = "sea_action";
  FacilityClearanceWorkOrigin value = sea;
  const auto fields = [](auto &archive, auto &row) {
    return care_clearance_origin_codec::work_origin(archive, row);
  };
  const auto decoded = round_trip(value, fields);
  check(decoded.ok && decoded.value.index() == 2 &&
            std::get<ClearanceSeaWorkOrigin>(decoded.value)
                    .care_transport_binding_ref.binding_id == "care_binding",
        "Sea seven-field origin lost binding");
  std::get<ClearanceSeaWorkOrigin>(value).result_source_schema = 3;
  check(!round_trip(value, fields).ok, "unpaired Sea result schema accepted");
  std::get<ClearanceSeaWorkOrigin>(value).result_source_schema.reset();
  std::get<ClearanceSeaWorkOrigin>(value)
      .care_transport_binding_ref.transfer_action_id = "other_action";
  check(!round_trip(value, fields).ok, "foreign Sea binding accepted");
}
// 嵌套单位复用经济原字段，重复必须按完整安装身份判断，不能只比较 unit 名。
void physical_position() {
  CareAggregatePhysicalPosition aggregate;
  aggregate.placement_claim_id = "claim";
  aggregate.position_rule_ref = {"position", 1, "digest"};
  FacilityPhysicalUnitRef unit{
      "facility",
      InstalledPhysicalUnitSource{
          {"local_economy", "physical_facility_installed", "installation"}, 1},
      "unit"};
  aggregate.affected_unit_refs = {unit};
  CarePhysicalPositionBasis value = aggregate;
  const auto fields = [](auto &archive, auto &row) {
    return care_physical_position_codec::position(archive, row);
  };
  const auto decoded = round_trip(value, fields);
  check(decoded.ok && std::get<CareAggregatePhysicalPosition>(decoded.value)
                              .affected_unit_refs.size() == 1,
        "original physical unit failed round trip");
  std::get<CareAggregatePhysicalPosition>(value).affected_unit_refs.push_back(
      unit);
  check(!round_trip(value, fields).ok, "duplicate physical unit accepted");
}

// 原 carrier 两支共用实际 S1/S2 字段表；这些地址仅 codec 输入，不冒承运许可。
void transport_carrier() {
  const auto fields = [](auto &archive, auto &row) {
    return care_transport_carrier_codec::carrier(archive, row);
  };
  CareGroundTransportCarrier ground;
  ground.transport_unit_ref = {{SubjectKind::actor, "operator"}, "unit"};
  ground.expected_composition_revision = 3;
  CareTransportCarrier value = ground;
  const auto decoded_ground = round_trip(value, fields);
  check(decoded_ground.ok &&
            std::get<CareGroundTransportCarrier>(decoded_ground.value)
                    .transport_unit_ref.transport_unit_id == "unit" &&
            std::get<CareGroundTransportCarrier>(decoded_ground.value)
                    .expected_composition_revision == 3,
        "original ground carrier failed round trip");
  std::get<CareGroundTransportCarrier>(value).expected_composition_revision =
      -1;
  check(!round_trip(value, fields).ok,
        "negative composition revision accepted");
  value = ground;
  std::get<CareGroundTransportCarrier>(value)
      .transport_unit_ref.subject.id.clear();
  check(!round_trip(value, fields).ok, "empty transport subject accepted");

  value = CareActualVesselCarrier{VesselRef{"ship"}};
  BoundaryValue encoded;
  ActorFoundationJsonVisitor json_writer(encoded);
  auto status = fields(json_writer, value);
  if (status.ok)
    status = json_writer.finish();
  const auto *vessel = encoded.find("vessel_ref");
  check(status.ok && vessel && vessel->as<StableId>() &&
            *vessel->as<StableId>() == "ship",
        "original vessel scalar wire was replaced");
  const auto decoded_vessel = round_trip(value, fields);
  check(decoded_vessel.ok &&
            std::get<CareActualVesselCarrier>(decoded_vessel.value)
                    .vessel_ref.vessel_id == "ship",
        "original vessel carrier failed round trip");

  CodecVisitor::Buffer bytes;
  CodecVisitor binary_writer(bytes);
  status = fields(binary_writer, value);
  if (status.ok)
    status = binary_writer.finish();
  check(status.ok, "original vessel binary write failed");
  const CodecVisitor::Buffer &input = bytes;
  CodecVisitor binary_reader(input);
  CareTransportCarrier binary_value;
  status = fields(binary_reader, binary_value);
  if (status.ok)
    status = binary_reader.finish();
  check(status.ok &&
            std::holds_alternative<CareActualVesselCarrier>(binary_value) &&
            std::get<CareActualVesselCarrier>(binary_value)
                    .vessel_ref.vessel_id == "ship",
        "original vessel binary round trip failed");

  std::get<CareActualVesselCarrier>(value).vessel_ref.vessel_id.clear();
  check(!round_trip(value, fields).ok, "empty original vessel key accepted");
  auto &dict = std::get<BoundaryValue::Dict>(encoded.data);
  for (auto &field : dict)
    if (field.first == "tag")
      field.second = BoundaryValue{Integer{2}};
  const BoundaryValue &unknown_input = encoded;
  ActorFoundationJsonVisitor unknown_reader(unknown_input);
  check(!fields(unknown_reader, value).ok, "unknown carrier tag accepted");
  for (auto &field : dict)
    if (field.first == "tag")
      field.second = BoundaryValue{Integer{1}};
  dict.emplace_back("fleet_ref", BoundaryValue{StableId{"fleet"}});
  const BoundaryValue &extra_input = encoded;
  ActorFoundationJsonVisitor extra_reader(extra_input);
  status = fields(extra_reader, value);
  if (status.ok)
    status = extra_reader.finish();
  check(!status.ok, "fleet field accepted as original vessel");
}
#include "care_due_checkpoint_values.inc"
#include "care_root_codec_regression.inc"
#include "collateral_personal_command_regression.inc"
} // namespace
// 一份受影响存读组件门，不注册 checklist 状态，也不建立替代 prepared 世界。
int main() {
  try {
    root_order();
    collateral_personal_actual_command_binding();
    care_checkpoint_values();
    care_complete_root_codec_values();
    body_scope();
    maturity();
    work_gate();
    work_origin();
    physical_position();
    transport_carrier();
    std::cout << "care_due_value_storage_test: PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "care_due_value_storage_test: " << error.what() << '\n';
    return 1;
  }
}
