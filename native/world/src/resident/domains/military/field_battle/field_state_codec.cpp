#include "field_event_codec_bridge.hpp"
#include "field_result_codec.hpp"
#include "field_view_projection.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
// 会战使用同一严格字段回调编码与投影；有序 map 读端拒绝重复或反序，不静默修档。
#include "field_state_codec.hpp"
#include "resident/domains/military/field_battle/field_view_model.hpp"
#include "resident/domains/military/field_battle/field_world.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
#include "resident/kernel/common_fact_prefix_codec_fields.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <type_traits>
namespace hanmo::resident {
namespace field_state_fields {
#include "field_state_codec_declarations.inc"
template <class C> Status value(C &c, StableId &v) {
  return c.field("value", v);
}
template <class C> Status value(C &c, bool &v) { return c.field("value", v); }
template <class C> Status value(C &c, Integer &v) {
  return c.field("value", v);
}
template <class C> Status value(C &c, std::size_t &v) {
  std::uint64_t number = v;
  auto r = c.unsigned_field("value", number);
  if (r.ok && c.reading()) {
    if (number > std::numeric_limits<std::size_t>::max())
      return c.reject("FieldIndexOverflow");
    v = static_cast<std::size_t>(number);
  }
  return r;
}
template <class C> Status value(C &c, PointI &v) {
  c.field("x", v.x);
  return c.field("y", v.y);
}
template <class C> Status value(C &c, SourceRef &v) {
  c.field("domain", v.domain);
  c.field("kind", v.kind);
  return c.field("id", v.id);
}
// 仅应用时刻复用原 S0 严格数值字段，不启用整套旧军事 exact profile。
template <class C> Status value(C &c, WorldMoment &v) {
  return exact_fields::moment(c, v);
}
// 离场的精确路线进度复用原约分分数格式与合法性门。
template <class C> Status value(C &c, CanonicalFraction &v) {
  return movement_record_fields::value(c, v, MovementRecordCodecSelection{});
}
// 接触字段直接复用原 S1 同一 callback，不造第二套 RoadTravelMotion schema。
template <class C> Status value(C &c, ContactHit &v) {
  return movement_record_fields::value(c, v, MovementRecordCodecSelection{});
}
// 边界的单方快照也复用原 S1 字段，不能把它包装成伪造的双方接触。
template <class C> Status value(C &c, RoadContactEndpoint &v) {
  return movement_record_fields::value(c, v, MovementRecordCodecSelection{});
}
template <class C> Status value(C &c, ContactEndpoint &v) {
  return movement_record_fields::value(c, v, MovementRecordCodecSelection{});
}
// 离场沿原 S1 路线格式，不另造世界位置类型。
template <class C> Status value(C &c, OnRouteLocation &v) {
  return movement_record_fields::value(c, v, MovementRecordCodecSelection{});
}
// 城村出口沿同一 S1 据点位置字段，不另造地名或默认城池。
template <class C> Status value(C &c, SettlementLocation &v) {
  return movement_record_fields::value(c, v, MovementRecordCodecSelection{});
}
template <class T> constexpr Integer enum_limit();
template <> constexpr Integer enum_limit<FieldBoundaryStatus>() { return 2; }
template <> constexpr Integer enum_limit<FieldFormationState>() { return 6; }
template <> constexpr Integer enum_limit<FieldTroopCategory>() { return 2; }
template <> constexpr Integer enum_limit<FieldOrderStatus>() { return 5; }
template <> constexpr Integer enum_limit<FieldAiTrigger>() { return 3; }
template <> constexpr Integer enum_limit<FieldBattleState>() { return 3; }
template <> constexpr Integer enum_limit<FieldCouncilStance>() { return 4; }
template <> constexpr Integer enum_limit<FieldCouncilRole>() { return 2; }
template <> constexpr Integer enum_limit<FieldCouncilLayout>() { return 3; }
template <> constexpr Integer enum_limit<FieldCouncilWave>() { return 2; }
template <> constexpr Integer enum_limit<FieldCouncilStratagemKind>() {
  return 1;
}
template <> constexpr Integer enum_limit<FieldCouncilRequestedRole>() {
  return 3;
}
template <> constexpr Integer enum_limit<FieldCouncilRequestStatus>() {
  return 2;
}
template <> constexpr Integer enum_limit<FieldCouncilFactKind>() { return 5; }
template <> constexpr Integer enum_limit<FieldTrait>() { return 5; }
template <> constexpr Integer enum_limit<FieldTacticKind>() { return 3; }
template <> constexpr Integer enum_limit<FieldStatusKind>() { return 4; }
template <> constexpr Integer enum_limit<FieldStratagemKind>() { return 2; }
template<class C> Status value(C &c, FieldBattleEventKind &v) {
  Integer tag=v.value;c.field("tag",tag);
  if(c.status().ok&&!registered_field_event_kind(FieldBattleEventKind{tag}))return c.reject("InvalidFieldBattleTag");
  if(c.status().ok&&c.reading())v.value=tag;
  return c.status();
}

template <class C, class T>
  requires std::is_enum_v<T>
Status value(C &c, T &v) {
  Integer tag = static_cast<Integer>(v);
  c.field("tag", tag);
  if (c.status().ok && (tag < 0 || tag > enum_limit<T>()))
    return c.reject("InvalidFieldBattleTag");
  if (c.status().ok && c.reading())
    v = static_cast<T>(tag);
  return c.status();
}
template <class C, class A, class B> Status value(C &, std::pair<A, B> &);
template <class C, class T, class A> Status value(C &, std::vector<T, A> &);
template <class C, class T> Status value(C &, std::optional<T> &);
template <class C, class T, std::size_t N>
Status value(C &, std::array<T, N> &);
template <class C, class... T> Status value(C &, std::variant<T...> &);
template <class C, class K, class T, class Compare>
Status value(C &, CheckpointMap<K, T, Compare> &);
// 每个实际子记录都有 object 边界；不跳过或自动补未知字段。
template <class C, class T> Status field(C &c, const char *name, T &v) {
  return c.object(name, [&](auto &child) { return value(child, v); });
}
template <class C, class A, class B> Status value(C &c, std::pair<A, B> &v) {
  field(c, "key", v.first);
  return field(c, "record", v.second);
}
template <class C, class T, class A> Status value(C &c, std::vector<T, A> &v) {
  return c.sequence("items", v,
                    [](auto &child, auto &row) { return value(child, row); });
}
template <class C, class T> Status value(C &c, std::optional<T> &v) {
  return c.optional("optional", v,
                    [](auto &child, auto &row) { return value(child, row); });
}
template <class C, class T, std::size_t N>
Status value(C &c, std::array<T, N> &v) {
  for (std::size_t index = 0; index < N; ++index) {
    const auto name = std::to_string(index);
    c.object(name, [&](auto &child) { return value(child, v[index]); });
  }
  return c.status();
}
// 新 variant 候选沿原 default-shape 预留后才构造；未知标签不会变成第一分支。
template <std::size_t I = 0, class C, class V>
Status variant(C &c, V &v, Integer tag) {
  if constexpr (I < std::variant_size_v<V>) {
    if (tag == static_cast<Integer>(I)) {
      if (c.reading()) {
        auto ready = c.template prepare_default_values<
            std::variant_alternative_t<I, V>>();
        if (!ready.ok)
          return ready;
        v.template emplace<I>();
      }
      return c.object(
          "record", [&](auto &child) { return value(child, std::get<I>(v)); });
    }
    return variant<I + 1>(c, v, tag);
  }
  return c.reject("InvalidFieldBattleVariant");
}
template <class C, class... T> Status value(C &c, std::variant<T...> &v) {
  Integer tag = static_cast<Integer>(v.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  return variant(c, v, tag);
}
// map 的唯一读候选按原序暂存后 move 到真实 tracked 节点；无另一个持久副本。
template <class C, class K, class T, class Compare>
Status value(C &c, CheckpointMap<K, T, Compare> &v) {
  if (c.reading()) {
    CheckpointVector<std::pair<K, T>> rows;
    auto result = c.sequence("items", rows, [](auto &child, auto &row) {
      return value(child, row);
    });
    if (!result.ok)
      return result;
    Compare less;
    for (std::size_t i = 1; i < rows.size(); ++i)
      if (!less(rows[i - 1].first, rows[i].first))
        return c.reject("FieldBattleMapOrderMismatch");
    v.clear();
    for (auto &row : rows)
      if (!v.emplace(std::move(row.first), std::move(row.second)).second)
        return c.reject("FieldBattleMapRepeated");
    return c.status();
  }
  auto emit = [](auto &child, const auto &row) {
    using MutableKey =
        std::remove_const_t<std::remove_reference_t<decltype(row.first)>>;
    field(child, "key", const_cast<MutableKey &>(row.first));
    return field(child, "record", const_cast<T &>(row.second));
  };
  if constexpr (std::is_same_v<C, ActorFoundationJsonVisitor>)
    return c.write_sequence("items", v, emit, false);
  else
    return c.write_sequence("items", v, emit);
}
#include "field_state_codec_fields.inc"
} // namespace field_state_fields
Status field_event_original_value(CodecVisitor &c, FieldMission &v) { return field_state_fields::value(c,v); }
Status field_event_original_value(CodecVisitor &c, FieldBattleTime &v) { return field_state_fields::value(c,v); }
Status field_event_original_value(ActorFoundationJsonVisitor &c, FieldMission &v) { return field_state_fields::value(c,v); }
Status field_event_original_value(ActorFoundationJsonVisitor &c, FieldBattleTime &v) { return field_state_fields::value(c,v); }
Status field_event_original_value(ActorFoundationProjectionArchive &c, FieldMission &v) { return field_state_fields::value(c,v); }
Status field_event_original_value(ActorFoundationProjectionArchive &c, FieldBattleTime &v) { return field_state_fields::value(c,v); }
Status visit_field_battle_world(CodecVisitor &codec,
                                FieldBattleWorldState &state) {

  if (codec.reading()) {
    auto ready = codec.prepare_default_values<FieldBattleWorldState>();
    if (!ready.ok)
      return ready;
    FieldBattleWorldState candidate;
    auto decoded = field_state_fields::value(codec, candidate);
    if (!decoded.ok)
      return decoded;
    if (candidate.schema != 1)
      return codec.reject("UnsupportedFieldBattleStateVersion");
    state = std::move(candidate);
    return codec.status();
  }
  if (state.schema != 1)
    return codec.reject("UnsupportedFieldBattleStateVersion");
  return field_state_fields::value(codec, state);
}
Outcome<FieldBattleWorldState>
decode_field_battle_world(const BoundaryValue &input) {
  ActorFoundationJsonVisitor reader(input);
  auto ready = reader.prepare_default_values<FieldBattleWorldState>();
  if (!ready.ok)
    return {false, {}, ready.error};
  FieldBattleWorldState state;
  auto decoded = field_state_fields::value(reader, state);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto finished = reader.finish();
  if (!finished.ok)
    return {false, {}, finished.error};
  if (state.schema != 1)
    return {
        false, {}, common_failure("UnsupportedFieldBattleStateVersion").error};
  return {true, std::move(state), {}};
}
Status emit_field_battle_world(ProjectionWriter &sink, std::string_view name,
                               const FieldBattleWorldState &state) {
  if (state.schema != 1)
    return sink.reject("UnsupportedFieldBattleStateVersion");
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, state, [](auto &archive, auto &value) {
        return field_state_fields::value(archive, value);
      });
}
Status visit_field_battle_receipt(CodecVisitor &c,
                                  FieldBattleReceipt &receipt) {

  return field_state_fields::value(c, receipt);
}
Status emit_field_battle_receipt_fields(ProjectionWriter &w,
                                        const FieldBattleReceipt &receipt) {
  ActorFoundationProjectionArchive archive(w);
  return field_state_fields::value(archive,
                                   const_cast<FieldBattleReceipt &>(receipt));
}
Outcome<FieldBattleReceipt>
decode_field_battle_receipt(const BoundaryValue &input) {
  ActorFoundationJsonVisitor reader(input);
  auto ready = reader.prepare_default_values<FieldBattleReceipt>();
  if (!ready.ok)
    return {false, {}, ready.error};
  FieldBattleReceipt out;
  auto decoded = field_state_fields::value(reader, out);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  auto finished = reader.finish();
  if (!finished.ok)
    return {false, {}, finished.error};
  return {true, std::move(out), {}};
}
Outcome<BoundaryValue>
encode_field_battle_read_quote(const FieldBattleReadQuote &quote) {
  return encode_field_battle_public_view(quote.view);
}
namespace {
// 唯一有限头目录同时供真实拥有者枚举、投影及冷边界反覆盖使用。
// 这里只验证结构与 Common 身份，不能代替完整同 World 业务来源门。
enum class OwnedFieldFact {
  Contact, Session, Queue, Entered, Segment, Started, Commander, Stratagem,
  Exit, Choice, Step, Ready, Council, Count
};
constexpr std::array<std::string_view,
                     static_cast<std::size_t>(OwnedFieldFact::Count)>
    owned_field_fact_names{{
        "field_battle_contact", "field_battle_session",
        "field_battle_entry_queue", "field_battle_entered",
        "field_battle_segment", "field_battle_started",
        "field_commander_changed", "field_battle_stratagem",
        "field_battle_exit", "field_army_exit_choice",
        "field_army_exit_step", "field_army_exit_ready", "military_council"}};
std::string_view owned_field_fact_name(OwnedFieldFact kind) {
  return owned_field_fact_names[static_cast<std::size_t>(kind)];
}
bool owned_field_source(const SourceRef &source, OwnedFieldFact kind) {
  return !source.id.empty() && source.domain == "military" &&
         source.kind == owned_field_fact_name(kind);
}
// 同步借用不可变领域 ID；seen 的 string_view 不越过此次枚举或持有者寿命。
template <class Emit>
Status visit_owned_field_fact_headers(const FieldBattleWorldState &state,
                                      std::set<std::string_view> &seen,
                                      Emit &&emit) {
  if (state.schema != 1)
    return common_failure("UnsupportedFieldBattleStateVersion");
  const auto record = [&](const StableId &id, OwnedFieldFact kind) -> Status {
    if (id.empty())
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    if (!seen.insert(id).second)
      return common_failure("DuplicateFieldBattleFactPayload", id);
    return emit(id, owned_field_fact_name(kind));
  };
  const auto source = [&](const SourceRef &ref, OwnedFieldFact kind) -> Status {
    if (!owned_field_source(ref, kind))
      return common_failure("FieldBattleFactPayloadIdentityMismatch", ref.id);
    return record(ref.id, kind);
  };
  for (const auto &[id, contact] : state.contacts) {
    if (id != contact.contact_id)
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    auto result = record(id, OwnedFieldFact::Contact);
    if (!result.ok) return result;
  }
  for (const auto &[id, boundary] : state.boundary_contacts) {
    if (id != boundary.contact_id)
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    auto result = record(id, OwnedFieldFact::Contact);
    if (!result.ok) return result;
    if (boundary.reinforcement_queue) {
      const auto &queue = *boundary.reinforcement_queue;
      result = source(queue.initialized_source, OwnedFieldFact::Queue);
      if (!result.ok) return result;
      // 边界 entry 是队列首项别名；空队列不能带伪入场来源。
      if (queue.admitted.empty()) {
        if (boundary.entry_application_source)
          return common_failure("FieldBattleFactAliasMismatch", id);
      } else {
        const auto &first = queue.admitted.front().application_source;
        if (!boundary.entry_application_source ||
            boundary.entry_application_source->id != first.id ||
            boundary.entry_application_source->kind != first.kind ||
            boundary.entry_application_source->domain != first.domain)
          return common_failure("FieldBattleFactAliasMismatch", id);
      }
      for (const auto &admission : queue.admitted) {
        result = source(admission.application_source, OwnedFieldFact::Entered);
        if (!result.ok) return result;
      }
    } else if (boundary.entry_application_source) {
      result = source(*boundary.entry_application_source, OwnedFieldFact::Entered);
      if (!result.ok) return result;
    }
  }
  for (const auto &[id, session] : state.sessions) {
    if (id != session.battle_id)
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    auto result = record(id, OwnedFieldFact::Session);
    if (!result.ok) return result;
    for (const auto &settlement : session.settlements) {
      result = record(settlement.fact_id, OwnedFieldFact::Segment);
      if (!result.ok) return result;
    }
    if (session.started_source) {
      const auto &started = *session.started_source;
      if (owned_field_source(started, OwnedFieldFact::Session)) {
        if (started.id != id)
          return common_failure("FieldBattleFactAliasMismatch", id);
      } else {
        result = source(started, OwnedFieldFact::Started);
        if (!result.ok) return result;
      }
    }
    for (const auto &[formation_id, formation] : session.formations)
      if (formation.ability) {
        if (formation_id != formation.formation_id)
          return common_failure("FieldBattleFactPayloadIdentityMismatch", formation_id);
        for (const auto &change : formation.ability->commander_changes) {
          result = source(change.source_ref, OwnedFieldFact::Commander);
          if (!result.ok) return result;
        }
      }
    // 只有 Confuse 拥有独立发动头，其余计策借军议源，由原完整校验核义。
    for (const auto &[stratagem_id, stratagem] : session.stratagems)
      if (stratagem.kind == FieldStratagemKind::Confuse) {
        if (stratagem_id != stratagem.stratagem_id ||
            stratagem_id != stratagem.adopted_source.id)
          return common_failure("FieldBattleFactPayloadIdentityMismatch", stratagem_id);
        result = source(stratagem.adopted_source, OwnedFieldFact::Stratagem);
        if (!result.ok) return result;
      }
  }
  for (const auto &[id, exit] : state.exits) {
    if (id != exit.fact_id)
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    auto result = record(id, OwnedFieldFact::Exit);
    if (!result.ok) return result;
  }
  for (const auto &[id, plan] : state.army_exit_plans) {
    if (id != plan.fact_id)
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    auto result = record(id, OwnedFieldFact::Choice);
    if (!result.ok) return result;
    for (const auto &step : plan.steps) {
      result = record(step.fact_id, OwnedFieldFact::Step);
      if (!result.ok) return result;
    }
    if (plan.ready_source) {
      result = source(*plan.ready_source, OwnedFieldFact::Ready);
      if (!result.ok) return result;
    }
  }
  for (const auto &[id, fact] : state.council_facts) {
    if (id != fact.fact_id)
      return common_failure("FieldBattleFactPayloadIdentityMismatch", id);
    auto result = record(id, OwnedFieldFact::Council);
    if (!result.ok) return result;
  }
  return common_success();
}
} // namespace
bool owns_field_battle_fact_kind(std::string_view kind) noexcept {
  return std::find(owned_field_fact_names.begin(), owned_field_fact_names.end(),
                   kind) != owned_field_fact_names.end();
}
Status validate_field_battle_fact_headers(const CommonStore &common,
                                          const FieldBattleWorldState &state) {
  return validate_field_battle_fact_headers(common, &state);
}
Status validate_field_battle_fact_headers(const CommonStore &common,
                                          const FieldBattleWorldState *state) {
  std::set<std::string_view> seen;
  if (state) {
    auto result = visit_owned_field_fact_headers(
        *state, seen, [&](const StableId &id, std::string_view kind) -> Status {
          const auto *header = common.state().facts.find(id);
          if (!header || header->id != id || header->domain != "military" ||
              header->kind != kind)
            return common_failure("FieldBattleOriginalFactHeaderRequired", id);
          return common_success();
        });
    if (!result.ok) return result;
  }
  // 未选根同样反扫；不能用错误 domain 或 null root 隐藏真实本域残留头。
  for (const auto &[id, header] : common.state().facts.rows())
    if (owns_field_battle_fact_kind(header.kind) &&
        (header.id != id || header.domain != "military" ||
         seen.find(id) == seen.end()))
      return common_failure("FieldBattleFactPayloadCoverageMismatch", id);
  return common_success();
}
Status emit_field_battle_fact_payloads_fields(ProjectionWriter &sink,
                                             const FieldBattleWorldState &state) {
  std::set<std::string_view> seen;
  auto result = visit_owned_field_fact_headers(
      state, seen, [&](const StableId &id, std::string_view kind) {
        return sink.object(id, [&](auto &record) {
          record.text("kind", kind);
          return record.text("domain", "military");
        });
      });
  return result.ok ? sink.status() : result;
}
} // namespace hanmo::resident
