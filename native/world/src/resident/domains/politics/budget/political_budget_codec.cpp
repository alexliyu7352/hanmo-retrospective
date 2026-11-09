#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_budget_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
namespace hanmo::resident {
namespace {
template <class C> Status source(C &c, SourceRef &s) {
  c.field("domain", s.domain);
  c.field("kind", s.kind);
  c.field("id", s.id);
  if (c.status().ok && !valid_source_ref(s))
    return c.reject("InvalidBudgetSourceRef");
  return c.status();
}
template <class C> Status identity(C &c, WorldIdentity &v) {
  StableId hex;
  if (!c.reading()) {
    constexpr char digits[] = "0123456789abcdef";
    for (auto b : v.world_id) {
      hex.push_back(digits[b >> 4]);
      hex.push_back(digits[b & 15]);
    }
  }
  c.field("world_id", hex);
  if (!c.status().ok)
    return c.status();
  if (hex.size() != 2 * v.world_id.size())
    return c.reject("InvalidBudgetWorldId");
  if (c.reading())
    for (std::size_t i = 0; i < v.world_id.size(); ++i) {
      auto digit = [](char x) {
        return x >= '0' && x <= '9'   ? x - '0'
               : x >= 'a' && x <= 'f' ? x - 'a' + 10
                                      : -1;
      };
      const auto hi = digit(hex[2 * i]), lo = digit(hex[2 * i + 1]);
      if (hi < 0 || lo < 0)
        return c.reject("InvalidBudgetWorldId");
      v.world_id[i] = static_cast<std::uint8_t>(16 * hi + lo);
    }
  c.field("execution_epoch", v.execution_epoch);
  c.field("revision", v.revision);
  return c.field("tick", v.tick);
}
template <class C> Status unit(C &c, BudgetUnit &u) {
  Integer tag = static_cast<Integer>(u.index());
  c.field("kind", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 1)
    return c.reject("InvalidBudgetUnit");
  if (c.reading())
    u = tag == 0 ? BudgetUnit{BudgetMoneyQian{}}
                 : BudgetUnit{BudgetItemQuantity{}};
  if (tag == 1)
    return c.field("item_id", std::get<BudgetItemQuantity>(u).item_id);
  return c.status();
}
template <class C>
Status purpose(C &c, BudgetExecutionPurpose &p, Integer version) {
  Integer tag = static_cast<Integer>(p);
  c.field("value", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > (version == 1 ? 5 : 6))
    return c.reject("InvalidBudgetPurpose", std::to_string(tag));
  auto valid = validate_budget_execution_purpose(
      static_cast<BudgetExecutionPurpose>(tag), version);
  if (!valid.ok)
    return valid;
  if (c.reading())
    p = static_cast<BudgetExecutionPurpose>(tag);
  return c.status();
}
template <class C>
Status record(C &c, BudgetAuthorizationSource &s, Integer version) {
  c.object("identity", [&](auto &x) { return identity(x, s.identity); });
  c.field("budget_id", s.budget_id);
  c.field("budget_revision", s.budget_revision);
  c.field("policy_id", s.policy_id);
  c.field("policy_revision", s.policy_revision);
  c.field("principal_org_id", s.principal_org_id);
  c.field("issuer_actor_id", s.issuer_actor_id);
  c.object("period", [&](auto &x) {
    x.field("period_from_tick", s.period.period_from_tick);
    return x.field("period_until_tick", s.period.period_until_tick);
  });
  c.object("scope", [&](auto &x) {
    Integer tag = static_cast<Integer>(s.scope.kind);
    x.field("kind", tag);
    if (tag < 0 || tag > 4)
      return x.reject("InvalidBudgetScope");
    if (x.reading())
      s.scope.kind = static_cast<ScopeRef::Kind>(tag);
    return x.field("id", s.scope.id);
  });
  c.sequence("container_refs", s.container_refs,
             [](auto &x, auto &id) { return x.field("value", id); });
  c.sequence("purposes", s.purposes,
             [&](auto &x, auto &p) { return purpose(x, p, version); });
  c.sequence("total_caps", s.total_caps, [](auto &x, auto &limit) {
    x.object("unit", [&](auto &y) { return unit(y, limit.unit); });
    x.field("cap", limit.cap);
    x.field("civil_floor", limit.civil_floor);
    x.field("garrison_floor", limit.garrison_floor);
    return x.sequence("container_refs", limit.container_refs,
                      [](auto &y, auto &id) { return y.field("value", id); });
  });
  c.sequence("category_caps", s.category_caps, [&](auto &x, auto &limit) {
    x.object("purpose",
             [&](auto &y) { return purpose(y, limit.purpose, version); });
    x.object("unit", [&](auto &y) { return unit(y, limit.unit); });
    return x.field("cap", limit.cap);
  });
  c.optional("parent_budget_ref", s.parent_budget_ref, [](auto &x, auto &ref) {
    x.field("authorization_id", ref.authorization_id);
    x.field("authorization_revision", ref.authorization_revision);
    return x.object("approval_source",
                    [&](auto &y) { return source(y, ref.approval_source); });
  });
  c.object("authority_basis_ref", [&](auto &x) {
    x.object("issued_source", [&](auto &y) {
      return source(y, s.authority_basis_ref.issued_source);
    });
    return x.field("source_schema", s.authority_basis_ref.source_schema);
  });
  c.object("approval_source",
           [&](auto &x) { return source(x, s.approval_source); });
  c.object("approved_at",
           [&](auto &x) { return exact_fields::moment(x, s.approved_at); });
  Integer decision = static_cast<Integer>(s.decision);
  c.field("decision", decision);
  if (decision < 0 || decision > 3)
    return c.reject("InvalidBudgetDecisionKind");
  if (c.reading())
    s.decision = static_cast<PublicBudgetDecisionKind>(decision);
  c.field("closed", s.closed);
  return c.status();
}
// 按物理四根借读原history，过滤器只持迭代位置，不复制记录或新建可变预算。
struct BudgetDecisionRange {
  const std::vector<BudgetAuthorizationSource> &rows;
  PublicBudgetDecisionKind kind;
  struct Iterator {
    const BudgetDecisionRange *range;
    std::size_t index;
    void advance() {
      while (index < range->rows.size() &&
             range->rows[index].decision != range->kind)
        ++index;
    }
    const BudgetAuthorizationSource &operator*() const {
      return range->rows[index];
    }
    Iterator &operator++() {
      ++index;
      advance();
      return *this;
    }
    bool operator!=(const Iterator &other) const {
      return index != other.index;
    }
  };
  Iterator begin() const {
    Iterator i{this, 0};
    i.advance();
    return i;
  }
  Iterator end() const { return {this, rows.size()}; }
  std::size_t size() const {
    std::size_t count = 0;
    for (const auto &row : rows)
      if (row.decision == kind)
        ++count;
    return count;
  }
};
template <class C>
Status emit_groups(C &c, PoliticalBudgetState &state, Integer version) {
  if ((version != 1 && version != 2) || state.schema_version != version)
    return c.reject("UnsupportedPoliticalBudgetsSchema",
                    std::to_string(state.schema_version));
  if (version == 2 &&
      state.authorization_history.size() > political_budget_history_limit)
    return c.reject("CheckpointCollectionTooLarge",
                    "political_budgets/authorization_history");
  Integer schema = version;
  c.field("schema_version", schema);
  if (schema != version)
    return c.reject("UnsupportedPoliticalBudgetsSchema",
                    std::to_string(schema));
  const char *names[] = {"authorizations", "delegations", "amendments",
                         "closures"};
  for (Integer tag = 0; tag < 4; ++tag) {
    const auto kind = static_cast<PublicBudgetDecisionKind>(tag);
    BudgetDecisionRange rows{state.authorization_history, kind};
    c.write_sequence(
        names[tag], rows, [&](auto &x, const BudgetAuthorizationSource &row) {
          return record(x, const_cast<BudgetAuthorizationSource &>(row),
                        version);
        });
    if (!c.status().ok)
      return c.status();
  }
  return c.status();
}
template <class C>
Status read_groups(C &c, PoliticalBudgetState &state, Integer version) {
  if (version != 1 && version != 2)
    return c.reject("UnsupportedPoliticalBudgetsSchema",
                    std::to_string(version));
  Integer schema = 0;
  c.field("schema_version", schema);
  if (schema != version)
    return c.reject("UnsupportedPoliticalBudgetsSchema",
                    std::to_string(schema));
  const char *names[] = {"authorizations", "delegations", "amendments",
                         "closures"};
  PoliticalBudgetState candidate;
  candidate.schema_version = version;
  for (Integer tag = 0; tag < 4; ++tag) {
    if (version == 1) {
      // 保旧 /1 四物理根逐根限制；新 owning 收拢只属于明选 /2。
      std::vector<BudgetAuthorizationSource> rows;
      c.sequence(names[tag], rows, [&](auto &x, auto &row) {
        auto valid = record(x, row, 1);
        if (valid.ok && static_cast<Integer>(row.decision) != tag)
          return x.reject("BudgetDecisionRootMismatch");
        return valid;
      });
      if (!c.status().ok)
        return c.status();
      for (auto &row : rows)
        candidate.authorization_history.push_back(std::move(row));
      continue;
    }
    c.append_sequence(
        names[tag], candidate.authorization_history,
        [&](auto &x, auto &row) {
          auto valid = record(x, row, version);
          if (valid.ok && static_cast<Integer>(row.decision) != tag)
            return x.reject("BudgetDecisionRootMismatch");
          return valid;
        },
        [](auto &destination, BudgetAuthorizationSource &row) -> Status {
          destination.emplace_back(std::move(row));
          return common_success();
        });
    if (!c.status().ok)
      return c.status();
  }
  state = std::move(candidate);
  return c.status();
}
} // namespace
Status visit_political_budgets(CodecVisitor &c, PoliticalBudgetState &state) {
  return visit_political_budgets(c, state, 1);
}
Status visit_political_budgets(CodecVisitor &c, PoliticalBudgetState &state,
                               Integer version) {
  if (c.reading())
    return read_groups(c, state, version);
  return emit_groups(c, state, version);
}
Status emit_political_budgets_projection(ProjectionWriter &sink,
                                         std::string_view name,
                                         const PoliticalBudgetState &state) {
  return emit_political_budgets_projection(sink, name, state, 1);
}
Status emit_political_budgets_projection(ProjectionWriter &sink,
                                         std::string_view name,
                                         const PoliticalBudgetState &state,
                                         Integer version) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, state,
      [&](auto &c, auto &value) { return emit_groups(c, value, version); });
}
Outcome<BoundaryValue>
encode_political_budgets(const PoliticalBudgetState &state) {
  return encode_political_budgets(state, 1);
}
Outcome<BoundaryValue>
encode_political_budgets(const PoliticalBudgetState &state, Integer version) {
  BoundaryValue result;
  BoundaryProjectionWriter sink(result);
  auto valid = emit_political_budgets_projection(sink, "", state, version);
  if (valid.ok)
    valid = sink.finish();
  return {valid.ok, std::move(result), valid.error};
}
Outcome<PoliticalBudgetState>
decode_political_budgets(const BoundaryValue &input) {
  return decode_political_budgets(input, 1);
}
Outcome<PoliticalBudgetState>
decode_political_budgets(const BoundaryValue &input, Integer version) {
  PoliticalBudgetState result;
  ActorFoundationJsonVisitor reader(input);
  auto valid = read_groups(reader, result, version);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(result), valid.error};
}
} // namespace hanmo::resident
