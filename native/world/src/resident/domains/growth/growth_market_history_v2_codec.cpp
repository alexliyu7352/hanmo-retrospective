// History2三出口共用字段表；Common原型只借唯一codec，不复制Boundary或另建库存快照。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_market_history_v2_codec.hpp"
#include "growth_market_history_v2_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/actor_foundation/character_origin_codec_detail.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
#include <limits>
#include <type_traits>
namespace hanmo::resident {
namespace market_v2_codec {
template <class T> struct Vector : std::false_type {};
template <class T> struct Vector<std::vector<T>> : std::true_type {
  using Value = T;
};
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {};
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
template <class T> struct Table : std::false_type {};
template <class T> struct Table<OrderedTable<T>> : std::true_type {
  using Value = T;
};
template <class T>
inline constexpr bool scalar =
    std::is_same_v<T, StableId> || std::is_same_v<T, Integer> ||
    std::is_same_v<T, std::uint32_t>;
template <class T>
inline constexpr bool shared =
    std::is_same_v<T, Owner> || std::is_same_v<T, ReservationRef> ||
    std::is_same_v<T, LotView> || std::is_same_v<T, InventoryView>;
template <class T>
Status emit(ProjectionWriter &, std::string_view, const T &,
            const CommonResourceCodecContext &);
template <class T>
Status binary(CodecVisitor &, std::string_view, T &,
              const CommonResourceCodecContext &);
template <class T>
Status json_value(ActorFoundationJsonVisitor &, const BoundaryValue &, T &,
                  const CommonResourceCodecContext &);
template <class T>
Status json_field(ActorFoundationJsonVisitor &, const BoundaryValue &,
                  std::string_view, T &, const CommonResourceCodecContext &);
// 唯一JSON访客拥有访问键和解码预算；这里不复制key集合或候选值。
struct DecodeFields {
  ActorFoundationJsonVisitor &archive;
  const BoundaryValue &input;
  const CommonResourceCodecContext &layout;
  bool reading() const { return true; }
  Status status() const { return archive.status(); }
  void reject(std::string_view code, std::string_view path) {
    archive.reject(code, path);
  }
  template <class T> void operator()(const char *name, T &value) {
    json_field(archive, input, name, value, layout);
  }
  void order(FieldOrder &value) {
    if (status().ok)
      value = boundary::order(input);
  }
};
// JSON导出只借原字段，FieldOrder仍由原ProjectionWriter处理。
struct ProjectFields {
  ProjectionWriter &writer;
  const CommonResourceCodecContext &layout;
  Status result = common_success();
  bool reading() const { return false; }
  Status status() const { return result.ok ? writer.status() : result; }
  void reject(std::string_view code, std::string_view path) {
    if (result.ok)
      result = writer.reject(code, path);
  }
  template <class T> void operator()(const char *name, T &value) {
    if (status().ok)
      result = emit(writer, name, value, layout);
  }
  void order(FieldOrder &value) {
    if (status().ok)
      result = writer.order_fields(value);
  }
};
// binary保声明序、原可选和同job容器分配，不借JSON做中转。
struct BinaryFields {
  CodecVisitor &archive;
  const CommonResourceCodecContext &layout;
  Status result = common_success();
  bool reading() const { return archive.reading(); }
  Status status() const { return result.ok ? archive.status() : result; }
  void reject(std::string_view code, std::string_view path) {
    if (result.ok)
      result = archive.reject(code, path);
  }
  template <class T> void operator()(const char *name, T &value) {
    if (status().ok)
      result = binary(archive, name, value, layout);
  }
  void order(FieldOrder &value) {
    if (status().ok)
      result = common_checkpoint::strings(archive, "field_order", value);
  }
};
// 先预留实际alternative默认拥有值，再在原variant就地构造。
template <std::size_t I = 0, class C, class T>
bool construct(C &archive, T &value, Integer tag) {
  if constexpr (I < std::variant_size_v<T>) {
    if (tag == static_cast<Integer>(I)) {
      if (prepare_checkpoint_default_values<std::variant_alternative_t<I, T>>(
              archive)
              .ok)
        value.template emplace<I>();
      return true;
    }
    return construct<I + 1>(archive, value, tag);
  }
  return false;
}
// 递归只查原输入对象重键，不能由foreign Common解码吞掉重复来源字段。
void unique(const BoundaryValue &input, std::size_t depth = 0) {
  if (depth >= CodecVisitor::max_depth)
    boundary::fail("CheckpointTooDeep", "growth_market_history");
  if (const auto *rows = input.as<BoundaryValue::Dict>()) {
    if (rows->size() > CodecVisitor::max_items)
      boundary::fail("CheckpointCollectionTooLarge", "growth_market_history");
    for (std::size_t i = 0; i < rows->size(); ++i) {
      for (std::size_t j = 0; j < i; ++j)
        if ((*rows)[j].first == (*rows)[i].first)
          boundary::fail("DuplicateGrowthMarketField", (*rows)[i].first);
      unique((*rows)[i].second, depth + 1);
    }
  } else if (const auto *rows = input.as<BoundaryValue::Array>()) {
    if (rows->size() > CodecVisitor::max_items)
      boundary::fail("CheckpointCollectionTooLarge", "growth_market_history");
    for (const auto &row : *rows)
      unique(row, depth + 1);
  }
}
// History2始终使用所选Common完整typed预约；不把旧字符串猜成新对象。
void typed_lot(const BoundaryValue &input) {
  if (!boundary::field(input, "reserved_by", "market_lot")
           .as<BoundaryValue::Dict>())
    boundary::fail("TypedReservationRequired", "market_lot/reserved_by");
}
template <class T>
Outcome<T> decode_shared(const BoundaryValue &input,
                         const CommonResourceCodecContext &layout,
                         ScopedCheckpointJsonDecode &budget) {
  if (ScopedCheckpointJsonDecode::current() != &budget)
    return {false,
            {},
            common_failure("CheckpointCandidateMemoryJobMismatch").error};
  try {
    if constexpr (std::is_same_v<T, Owner>)
      return {true, boundary::decode_owner(input, "market_owner"), {}};
    else if constexpr (std::is_same_v<T, ReservationRef>) {
      if (!input.as<BoundaryValue::Dict>())
        boundary::fail("TypedReservationRequired", "market_reservation");
      return {true,
              boundary::decode_reservation_ref(
                  input, "market_reservation",
                  layout.domain_activity_reservation_refs_enabled),
              {}};
    } else if constexpr (std::is_same_v<T, LotView>) {
      typed_lot(input);
      return {true, boundary::decode_lot(input, "market_lot", layout), {}};
    } else {
      for (const auto &row :
           boundary::array(boundary::field(input, "lots", "market_inventory"),
                           "market_inventory/lots"))
        typed_lot(row);
      return decode_common_inventory_frame(input, "market_inventory", layout,
                                           budget);
    }
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
template <class T>
Status emit_shared_fields(ProjectionWriter &writer, const T &value,
                          const CommonResourceCodecContext &layout) {
  if constexpr (std::is_same_v<T, Owner>)
    return emit_common_owner_fields(writer, value);
  else if constexpr (std::is_same_v<T, ReservationRef>)
    return emit_common_reservation_fields(
        writer, value, true, layout.domain_activity_reservation_refs_enabled);
  else if constexpr (std::is_same_v<T, LotView>)
    return emit_common_lot_fields(writer, value, true, layout);
  else
    return emit_common_inventory_frame_fields(writer, value, true, layout);
}
template <class T>
Status emit(ProjectionWriter &writer, std::string_view name, const T &value,
            const CommonResourceCodecContext &layout) {
  if constexpr (std::is_same_v<T, StableId> || std::is_same_v<T, Integer>)
    return writer.field(name, value);
  else if constexpr (std::is_same_v<T, std::uint32_t>)
    return writer.field(name, static_cast<Integer>(value));
  else if constexpr (std::is_same_v<T, Owner>)
    return emit_common_owner(writer, name, value);
  else if constexpr (std::is_same_v<T, ReservationRef>)
    return emit_common_reservation(
        writer, name, value, true,
        layout.domain_activity_reservation_refs_enabled);
  else if constexpr (std::is_same_v<T, LotView>)
    return emit_common_lot(writer, name, value, true, layout);
  else if constexpr (std::is_same_v<T, InventoryView>)
    return emit_common_inventory_frame(writer, name, value, true, layout);
  else if constexpr (std::is_same_v<T, WorldMoment>)
    return emit_projection_record<ActorFoundationProjectionArchive>(
        writer, name, value, [](auto &archive, auto &row) {
          return exact_fields::moment(archive, row);
        });
  else if constexpr (std::is_same_v<T, ActorFoundationRuleRef>)
    return emit_projection_record<ActorFoundationProjectionArchive>(
        writer, name, value, [](auto &archive, auto &row) {
          return character_origin_codec::rule(archive, row);
        });
  else if constexpr (Vector<T>::value)
    return writer.sequence(name, value, [&](auto &out, const auto &row) {
      return emit(out, "", row, layout);
    });
  else if constexpr (Optional<T>::value) {
    if (value)
      return emit(writer, name, *value, layout);
    return writer.null(name);
  } else if constexpr (Table<T>::value)
    return writer.keyed_dictionary(name, {}, [&](auto &out) {
      for (const auto &row : value.rows()) {
        auto result = emit(out, row.first, row.second, layout);
        if (!result.ok)
          return result;
      }
      return out.status();
    });
  else if constexpr (Variant<T>::value) {
    if (value.valueless_by_exception())
      return writer.reject("InvalidMarketHistory2Variant", name);
    return writer.object(name, [&](auto &out) {
      out.field("tag", static_cast<Integer>(value.index()));
      return std::visit(
          [&](const auto &row) { return emit(out, "value", row, layout); },
          value);
    });
  } else
    return writer.object(name, [&](auto &out) {
      ProjectFields archive{out, layout};
      growth_market_v2_fields::fields(archive, const_cast<T &>(value));
      return archive.status();
    });
}
template <class T>
Status json_value(ActorFoundationJsonVisitor &archive,
                  const BoundaryValue &input, T &value,
                  const CommonResourceCodecContext &layout) {
  if constexpr (std::is_same_v<T, StableId> || std::is_same_v<T, Integer>)
    return archive.field("", value);
  else if constexpr (std::is_same_v<T, std::uint32_t>) {
    Integer number = value;
    archive.field("", number);
    if (!archive.status().ok)
      return archive.status();
    if (number < 0 || static_cast<std::uint64_t>(number) >
                          std::numeric_limits<std::uint32_t>::max())
      return archive.reject("MarketHistory2OrdinalOutOfRange");
    value = static_cast<std::uint32_t>(number);
    return archive.status();
  } else if constexpr (shared<T>)
    return archive.foreign_record(
        value,
        [&](const BoundaryValue &raw, ScopedCheckpointJsonDecode &budget) {
          return decode_shared<T>(raw, layout, budget);
        },
        [&](ProjectionWriter &out, const T &row) {
          return emit_shared_fields(out, row, layout);
        });
  else if constexpr (std::is_same_v<T, WorldMoment>)
    return exact_fields::moment(archive, value);
  else if constexpr (std::is_same_v<T, ActorFoundationRuleRef>)
    return character_origin_codec::rule(archive, value);
  else if constexpr (Variant<T>::value) {
    Integer tag = static_cast<Integer>(value.index());
    archive.field("tag", tag);
    if (!archive.status().ok)
      return archive.status();
    if (tag < 0 || !construct(archive, value, tag))
      return archive.reject("InvalidMarketHistory2Variant");
    if (!archive.status().ok)
      return archive.status();
    const auto &raw = boundary::field(input, "value", "market_variant");
    return archive.object("value", [&](auto &child) {
      return std::visit(
          [&](auto &row) { return json_value(child, raw, row, layout); },
          value);
    });
  } else {
    DecodeFields fields{archive, input, layout};
    growth_market_v2_fields::fields(fields, value);
    return fields.status();
  }
}
template <class T>
Status json_field(ActorFoundationJsonVisitor &archive,
                  const BoundaryValue &input, std::string_view name, T &value,
                  const CommonResourceCodecContext &layout) {
  if (!archive.status().ok)
    return archive.status();
  const auto &raw = boundary::field(input, name, "growth_market_history");
  if constexpr (std::is_same_v<T, StableId> || std::is_same_v<T, Integer>)
    return archive.field(name, value);
  else if constexpr (std::is_same_v<T, std::uint32_t>) {
    Integer number = value;
    archive.field(name, number);
    if (!archive.status().ok)
      return archive.status();
    if (number < 0 || static_cast<std::uint64_t>(number) >
                          std::numeric_limits<std::uint32_t>::max())
      return archive.reject("MarketHistory2OrdinalOutOfRange", name);
    value = static_cast<std::uint32_t>(number);
    return archive.status();
  } else if constexpr (Vector<T>::value) {
    const auto &rows = boundary::array(raw, name);
    std::size_t index = 0;
    return archive.sequence(
        name, value,
        [&](auto &child, auto &row) {
          return json_value(child, rows[index++], row, layout);
        },
        scalar<typename Vector<T>::Value>);
  } else if constexpr (Optional<T>::value)
    return archive.optional(
        name, value,
        [&](auto &child, auto &row) {
          return json_value(child, raw, row, layout);
        },
        false);
  else if constexpr (Table<T>::value) {
    static_assert(
        std::is_same_v<typename Table<T>::Value, MarketMovementRecord>);
    const auto &rows = boundary::object(raw, name);
    std::size_t index = 0;
    return archive.table(
        name, value,
        [&](auto &child, auto &row) {
          return json_value(child, rows[index++].second, row, layout);
        },
        [](auto &destination, auto &source) -> Status {
          destination.emplace_back(std::move(source));
          return common_success();
        });
  } else
    return archive.object(name, [&](auto &child) {
      return json_value(child, raw, value, layout);
    });
}
template <class T>
Status binary_value(CodecVisitor &archive, T &value,
                    const CommonResourceCodecContext &layout) {
  if constexpr (scalar<T>) {
    if constexpr (std::is_same_v<T, std::uint32_t>) {
      Integer number = value;
      archive.field("value", number);
      if (!archive.status().ok)
        return archive.status();
      if (number < 0 || static_cast<std::uint64_t>(number) >
                            std::numeric_limits<std::uint32_t>::max())
        return archive.reject("MarketHistory2OrdinalOutOfRange");
      if (archive.reading())
        value = static_cast<std::uint32_t>(number);
      return archive.status();
    } else
      return archive.field("value", value);
  } else if constexpr (std::is_same_v<T, Owner>)
    return common_checkpoint::owner(archive, value);
  else if constexpr (std::is_same_v<T, ReservationRef>) {
    static const StableId key = "action_id";
    return common_checkpoint::reservation(archive, value, key);
  } else if constexpr (std::is_same_v<T, LotView>)
    return common_checkpoint::lot(archive, value);
  else if constexpr (std::is_same_v<T, InventoryView>)
    return common_checkpoint::inventory(archive, value);
  else if constexpr (std::is_same_v<T, WorldMoment>)
    return exact_fields::moment(archive, value);
  else if constexpr (std::is_same_v<T, ActorFoundationRuleRef>)
    return character_origin_codec::rule(archive, value);
  else if constexpr (Variant<T>::value) {
    if (value.valueless_by_exception())
      return archive.reject("InvalidMarketHistory2Variant");
    Integer tag = static_cast<Integer>(value.index());
    archive.field("tag", tag);
    if (!archive.status().ok)
      return archive.status();
    if (tag < 0 || tag >= static_cast<Integer>(std::variant_size_v<T>))
      return archive.reject("InvalidMarketHistory2Variant");
    if (archive.reading())
      construct(archive, value, tag);
    if (!archive.status().ok)
      return archive.status();
    return archive.object("value", [&](auto &child) {
      return std::visit(
          [&](auto &row) { return binary_value(child, row, layout); }, value);
    });
  } else {
    BinaryFields fields{archive, layout};
    growth_market_v2_fields::fields(fields, value);
    return fields.status();
  }
}
template <class T>
Status binary(CodecVisitor &archive, std::string_view name, T &value,
              const CommonResourceCodecContext &layout) {
  if constexpr (std::is_same_v<T, StableId> || std::is_same_v<T, Integer>)
    return archive.field(name, value);
  else if constexpr (std::is_same_v<T, std::uint32_t>) {
    Integer number = value;
    archive.field(name, number);
    if (!archive.status().ok)
      return archive.status();
    if (number < 0 || static_cast<std::uint64_t>(number) >
                          std::numeric_limits<std::uint32_t>::max())
      return archive.reject("MarketHistory2OrdinalOutOfRange", name);
    if (archive.reading())
      value = static_cast<std::uint32_t>(number);
    return archive.status();
  } else if constexpr (Vector<T>::value)
    return archive.sequence(name, value, [&](auto &child, auto &row) {
      return binary_value(child, row, layout);
    });
  else if constexpr (Optional<T>::value)
    return archive.optional(name, value, [&](auto &child, auto &row) {
      return binary_value(child, row, layout);
    });
  else if constexpr (Table<T>::value) {
    static_assert(
        std::is_same_v<typename Table<T>::Value, MarketMovementRecord>);
    return common_checkpoint::table(
        archive, name, value,
        [&](auto &child, auto &row) {
          return binary_value(child, row, layout);
        },
        [](auto &destination, auto &source) -> Status {
          destination.emplace_back(std::move(source));
          return common_success();
        });
  } else
    return archive.object(
        name, [&](auto &child) { return binary_value(child, value, layout); });
}
} // namespace market_v2_codec
Status visit_growth_market_history_v2_checkpoint(
    CodecVisitor &archive, GrowthMarketHistoryState &state,
    const CommonResourceCodecContext &layout) {
  return exact_fields::guarded<Done>([&] {
    if (!(archive.schema_features &
          common_checkpoint::typed_resource_references) ||
        archive.item_instance_refs_enabled !=
            layout.item_instance_refs_enabled ||
        archive.lot_property_refs_enabled != layout.lot_property_refs_enabled ||
        archive.domain_activity_reservation_refs_enabled !=
            layout.domain_activity_reservation_refs_enabled ||
        archive.carrier_location_enabled != layout.carrier_location_enabled)
      exact_fields::take(
          archive.reject("GrowthMarketHistory2CommonLayoutMismatch"));
    market_v2_codec::BinaryFields fields{archive, layout};
    growth_market_v2_fields::fields(fields, state);
    exact_fields::take(fields.status());
    auto valid = validate_growth_market_history_v2_shape(state, layout);
    exact_fields::take(valid.ok ? archive.status()
                                : archive.reject(valid.error.original.code,
                                                 valid.error.original.path));
    return Done{};
  });
}
Status
emit_growth_market_history_v2_fields(ProjectionWriter &writer,
                                     const GrowthMarketHistoryState &state,
                                     const CommonResourceCodecContext &layout) {
  return exact_fields::guarded<Done>([&] {
    exact_fields::take(validate_growth_market_history_v2_shape(state, layout));
    market_v2_codec::ProjectFields fields{writer, layout};
    growth_market_v2_fields::fields(
        fields, const_cast<GrowthMarketHistoryState &>(state));
    exact_fields::take(fields.status());
    return Done{};
  });
}
Outcome<GrowthMarketHistoryState>
decode_growth_market_history_v2(const BoundaryValue &input,
                                const CommonResourceCodecContext &layout,
                                ScopedCheckpointJsonDecode &budget) {
  return exact_fields::guarded<GrowthMarketHistoryState>([&] {
    if (ScopedCheckpointJsonDecode::current() != &budget)
      exact_fields::take(
          common_failure("CheckpointCandidateMemoryJobMismatch"));
    try {
      market_v2_codec::unique(input);
      budget.default_values<GrowthMarketHistoryState>();
      GrowthMarketHistoryState result;
      ActorFoundationJsonVisitor archive(input);
      market_v2_codec::DecodeFields fields{archive, input, layout};
      growth_market_v2_fields::fields(fields, result);
      exact_fields::take(fields.status());
      exact_fields::take(archive.finish());
      exact_fields::take(
          validate_growth_market_history_v2_shape(result, layout));
      return result;
    } catch (const boundary::DecodeFailure &error) {
      throw exact_fields::CodecFailure{error.error};
    }
  });
}
} // namespace hanmo::resident
