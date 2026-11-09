// 边界严格逐字段；取消、雇用和财货报价复用真实本域门，不由按钮推算许可。
#include "private_shop_boundary.hpp"
#include "private_shop_codec.hpp"
#include "resident/domains/local_economy/private_shop_household_custody.hpp"
#include "resident/domains/local_economy/economic_payload_guard.hpp"
namespace hanmo::resident {
// 当前命令只可点名真实选中政策；既有政策摘要仍由同 World 目录拥有。
Outcome<ActorFoundationRuleRef> read_private_shop_explicit_custody_policy(
    const PrivateShopHouseholdCustodyContext &context,
    const PrivateShopOperateWithCustody &command) {
  const auto *policy = context.catalog.policies.find(command.custody_policy_id);
  if (!policy || command.custody_policy_version != 1 ||
      policy->policy_ref.version != command.custody_policy_version ||
      policy->policy_ref.catalog_digest != context.catalog.catalog_digest)
    return {false,
            {},
            common_failure("PrivateShopExplicitCustodyPolicyRequired",
                           command.custody_policy_id)
                .error};
  return {true, policy->policy_ref, {}};
}
Outcome<PrivateShopCommand>
decode_private_shop_command(const StableId &kind, const BoundaryValue &params) {
  try {
    using namespace boundary;
    auto checked = validate_economic_payload_fields(params);
    if (!checked.ok)
      return {false, {}, checked.error};
    auto id = [&](const char *key) {
      auto s = get_string(params, key, "params");
      if (s.empty() || s.size() > 256)
        fail("InvalidPrivateShopCommandId", key);
      return s;
    };
    if (kind == "private_shop_open") {
      fields(params, {"premises_id"}, {}, "params");
      return {true, PrivateShopOpen{id("premises_id")}, {}};
    }
    if (kind == "private_shop_fund") {
      fields(params, {"shop_id", "amount"}, {}, "params");
      auto amount = get_int(params, "amount", "params");
      if (amount <= 0)
        fail("InvalidPrivateShopCapital", "amount");
      return {true, PrivateShopFund{id("shop_id"), amount}, {}};
    }
    if (kind == "private_shop_set_markup") {
      fields(params, {"shop_id", "markup_basis_points"}, {}, "params");
      auto markup = get_int(params, "markup_basis_points", "params");
      if (markup != 0 && markup != 500 && markup != 1000)
        fail("InvalidPrivateShopMarkup", "markup_basis_points");
      return {true, PrivateShopSetMarkup{id("shop_id"), markup}, {}};
    }
    if (kind == "private_shop_restock") {
      fields(params, {"shop_id", "item_id", "quantity", "context"}, {},
             "params");
      auto amount = get_int(params, "quantity", "params");
      if (amount <= 0 || amount > 500)
        fail("InvalidPrivateShopRestockQuantity", "quantity");
      const auto &context = field(params, "context", "params");
      fields(context, {"place_id", "provider_actor_id", "role_id"}, {},
             "params/context");
      InteractionContext source;
      source.place_id = get_string(context, "place_id", "params/context");
      source.provider_actor_id =
          get_string(context, "provider_actor_id", "params/context");
      source.role_id = get_string(context, "role_id", "params/context");
      if (source.place_id.empty() || source.provider_actor_id.empty() ||
          source.role_id.empty())
        fail("InvalidPrivateShopTradeContext", "context");
      return {true,
              PrivateShopRestock{id("shop_id"), id("item_id"), amount, source},
              {}};
    }
    if (kind == "private_shop_operate") {
      if (params.find("version")) {
        fields(params, {"version", "shop_id", "item_id", "custody_policy"}, {},
               "params");
        if (get_int(params, "version", "params") != 2)
          unsupported("UnsupportedPrivateShopOperateCommandVersion", "version");
        const auto &policy = field(params, "custody_policy", "params");
        fields(policy, {"id", "version"}, {}, "params/custody_policy");
        auto policy_id = get_string(policy, "id", "params/custody_policy");
        auto version = get_int(policy, "version", "params/custody_policy");
        if (policy_id.empty() || policy_id.size() > 256 || version != 1)
          fail("InvalidPrivateShopCustodyPolicy", "custody_policy");
        return {true,
                PrivateShopOperateWithCustody{id("shop_id"), id("item_id"),
                                              std::move(policy_id), version},
                {}};
      }
      fields(params, {"shop_id", "item_id"}, {}, "params");
      return {true, PrivateShopOperate{id("shop_id"), id("item_id")}, {}};
    }
    if (kind == "private_shop_cancel_operation") {
      fields(params, {"action_id"}, {}, "params");
      return {true, PrivateShopCancelOperation{id("action_id")}, {}};
    }
    if (kind == "private_shop_employ" || kind == "private_shop_staff_accept") {
      fields(params, {"shop_id", "policy_id"}, {}, "params");
      if (kind == "private_shop_employ")
        return {true, PrivateShopEmploy{id("shop_id"), id("policy_id")}, {}};
      return {true, PrivateShopStaffAccept{id("shop_id"), id("policy_id")}, {}};
    }
    if (kind == "private_shop_staff_decide") {
      fields(params, {"contract_id", "decision"}, {}, "params");
      auto decision = id("decision");
      if (decision != "leave" && decision != "dismiss")
        fail("InvalidPrivateShopStaffDecision", "decision");
      return {true, PrivateShopStaffDecision{id("contract_id"), decision}, {}};
    }
    if (kind == "private_shop_request_order") {
      fields(params, {"shop_id", "policy_id"}, {}, "params");
      return {
          true, PrivateShopRequestOrder{id("shop_id"), id("policy_id")}, {}};
    }
    if (kind == "private_shop_close") {
      fields(params, {"shop_id"}, {}, "params");
      return {true, PrivateShopClose{id("shop_id")}, {}};
    }
    return {false, {}, common_failure("UnknownPrivateShopCommand", kind).error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
StableId private_shop_command_kind(const PrivateShopCommand &c) {
  return std::visit(
      [](const auto &v) -> StableId {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrivateShopOpen>)
          return "private_shop_open";
        else if constexpr (std::is_same_v<T, PrivateShopFund>)
          return "private_shop_fund";
        else if constexpr (std::is_same_v<T, PrivateShopSetMarkup>)
          return "private_shop_set_markup";
        else if constexpr (std::is_same_v<T, PrivateShopRestock>)
          return "private_shop_restock";
        else if constexpr (std::is_same_v<T, PrivateShopOperate> ||
                           std::is_same_v<T, PrivateShopOperateWithCustody>)
          return "private_shop_operate";
        else if constexpr (std::is_same_v<T, PrivateShopCancelOperation>)
          return "private_shop_cancel_operation";
        else if constexpr (std::is_same_v<T, PrivateShopEmploy>)
          return "private_shop_employ";
        else if constexpr (std::is_same_v<T, PrivateShopStaffAccept>)
          return "private_shop_staff_accept";
        else if constexpr (std::is_same_v<T, PrivateShopStaffDecision>)
          return "private_shop_staff_decide";
        else if constexpr (std::is_same_v<T, PrivateShopRequestOrder>)
          return "private_shop_request_order";
        else
          return "private_shop_close";
      },
      c);
}
BoundaryValue encode_private_shop_command(const PrivateShopCommand &c) {
  return std::visit(
      [](const auto &v) -> BoundaryValue {
        using T = std::decay_t<decltype(v)>;
        using V = BoundaryValue;
        using boundary::obj;
        if constexpr (std::is_same_v<T, PrivateShopOpen>)
          return obj({{"premises_id", V{v.premises_id}}});
        else if constexpr (std::is_same_v<T, PrivateShopCancelOperation>)
          return obj({{"action_id", V{v.action_id}}});
        else if constexpr (std::is_same_v<T, PrivateShopStaffDecision>)
          return obj(
              {{"contract_id", V{v.contract_id}}, {"decision", V{v.decision}}});
        else if constexpr (std::is_same_v<T, PrivateShopFund>)
          return obj({{"shop_id", V{v.shop_id}}, {"amount", V{v.amount}}});
        else if constexpr (std::is_same_v<T, PrivateShopSetMarkup>)
          return obj({{"shop_id", V{v.shop_id}},
                      {"markup_basis_points", V{v.markup_basis_points}}});
        else if constexpr (std::is_same_v<T, PrivateShopRestock>)
          return obj({{"shop_id", V{v.shop_id}},
                      {"item_id", V{v.item_id}},
                      {"quantity", V{v.quantity}},
                      {"context", obj({{"place_id", V{v.context.place_id}},
                                       {"provider_actor_id",
                                        V{v.context.provider_actor_id}},
                                       {"role_id", V{v.context.role_id}}})}});
        else if constexpr (std::is_same_v<T, PrivateShopOperateWithCustody>)
          return obj({{"version", V{Integer{2}}},
                      {"shop_id", V{v.shop_id}},
                      {"item_id", V{v.item_id}},
                      {"custody_policy",
                       obj({{"id", V{v.custody_policy_id}},
                            {"version", V{v.custody_policy_version}}})}});
        else if constexpr (std::is_same_v<T, PrivateShopOperate>)
          return obj({{"shop_id", V{v.shop_id}}, {"item_id", V{v.item_id}}});
        else if constexpr (std::is_same_v<T, PrivateShopEmploy> ||
                           std::is_same_v<T, PrivateShopStaffAccept> ||
                           std::is_same_v<T, PrivateShopRequestOrder>)
          return obj(
              {{"shop_id", V{v.shop_id}}, {"policy_id", V{v.policy_id}}});
        else
          return obj({{"shop_id", V{v.shop_id}}});
      },
      c);
}

Outcome<PrivateShopCommandQuote>
preview_private_shop_command(const PrivateShopCommandContext &x,
                             const StableId &actor,
                             const PrivateShopCommand &command) {
  PrivateShopCommandQuote out{
      x.shops.common.state().identity, actor, command, {}};
  auto done = std::visit(
      [&](const auto &v) -> Status {
        using T = std::decay_t<decltype(v)>;
        auto assign = [&](auto result) -> Status {
          if (!result.ok)
            return {false, {}, result.error};
          out.data = std::move(result.value);
          return common_success();
        };
        if constexpr (std::is_same_v<T, PrivateShopOpen>)
          return assign(
              preview_private_shop_opening(x.shops, x.meeting, actor, v));
        else if constexpr (std::is_same_v<T, PrivateShopFund> ||
                           std::is_same_v<T, PrivateShopSetMarkup>)
          return assign(preview_private_shop_management(
              x.shops, actor, PrivateShopManagementCommand{v}));
        else if constexpr (std::is_same_v<T, PrivateShopRestock>)
          return assign(prepare_private_shop_restock(
              x.shops, x.growth, x.social, x.growth_access, actor, v));
        else if constexpr (std::is_same_v<T, PrivateShopOperateWithCustody>) {
          if (!x.private_custody ||
              &x.private_custody->market.operation.shops != &x.shops)
            return common_failure("PrivateShopExplicitCustodyContextRequired");
          auto policy =
              read_private_shop_explicit_custody_policy(*x.private_custody, v);
          if (!policy.ok)
            return {false, {}, policy.error};
          auto valid = preview_private_shop_household_custody_choice(
              *x.private_custody, v.shop_id, policy.value, actor);
          if (!valid.ok)
            return valid;
          return assign(preview_private_shop_operation(
              x.shops, actor, PrivateShopOperate{v.shop_id, v.item_id}));
        } else if constexpr (std::is_same_v<T, PrivateShopOperate>)
          return assign(preview_private_shop_operation(x.shops, actor, v));
        else if constexpr (std::is_same_v<T, PrivateShopCancelOperation>) {
          auto valid = preview_private_shop_cancel_operation(x.shops, actor,
                                                             v.action_id);
          if (valid.ok)
            out.data = PrivateShopCancelQuote{v.action_id};
          return valid;
        } else if constexpr (std::is_same_v<T, PrivateShopEmploy> ||
                             std::is_same_v<T, PrivateShopStaffAccept> ||
                             std::is_same_v<T, PrivateShopStaffDecision>) {
          if (!x.shops.estates || !x.shops.estate_access)
            return common_failure("PrivateShopLaborSourceUnavailable");
          PrivateShopStaffContext staff{x.shops,
                                        x.shops.catalog.staff,
                                        x.shops.state.staff,
                                        *x.shops.estates,
                                        x.shops.estate_employment,
                                        x.shops.estate_access};
          return assign(preview_private_shop_staff(staff, x.meeting, actor,
                                                   PrivateShopStaffCommand{v}));
        } else if constexpr (std::is_same_v<T, PrivateShopRequestOrder>)
          return assign(
              preview_private_shop_order(x.shops, x.meeting, actor, v));
        else
          return assign(preview_private_shop_closing(x.shops, actor, v));
      },
      command);
  return done.ok ? Outcome<PrivateShopCommandQuote>{true, std::move(out), {}}
                 : Outcome<PrivateShopCommandQuote>{false, {}, done.error};
}
BoundaryValue encode_private_shop_quote(const PrivateShopCommandQuote &q) {
  using V = BoundaryValue;
  using boundary::obj;
  auto detail = std::visit(
      [](const auto &value) -> BoundaryValue {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, PrivateShopOpeningQuote>) {
          V out =
              obj({{"duration", V{Integer{0}}},
                   {"terms", dynamic_economy_codec::encode(value.assets.terms)},
                   {"total_cash", V{value.assets.total_private_cash}}});
          if (value.daily_rent) {
            const auto &rent = *value.daily_rent;
            out.as<V::Dict>()->push_back(
                {"daily_rent", obj({{"profile", V{rent.profile}},
                                    {"arrears_policy", V{rent.arrears_policy}},
                                    {"prepaid_from_initial_capital",
                                     V{rent.prepaid_from_initial_capital}},
                                    {"from_tick", V{rent.from_tick}},
                                    {"through_tick", V{rent.through_tick}}})});
          }
          return out;
        } else if constexpr (std::is_same_v<T, PrivateShopManagementQuote>)
          return obj({{"duration", V{Integer{0}}}});
        else if constexpr (std::is_same_v<T, PrivateShopRestockAdmission>) {
          const auto *t = std::get_if<TradeTerms>(&value.trade.data);
          return obj({{"duration", V{value.trade.duration}},
                      {"quantity", V{t ? t->quantity : 0}},
                      {"cash", V{t ? t->total : 0}}});
        } else if constexpr (std::is_same_v<T, PrivateShopOperationQuote>)
          return obj({{"duration", V{value.duration}},
                      {"rent_cash", V{value.rent_cash}},
                      {"wage_cap", V{value.wage_cap}}});
        else if constexpr (std::is_same_v<T, PrivateShopStaffQuote>)
          return obj({{"duration", V{Integer{0}}},
                      {"terms", dynamic_economy_codec::encode(value.terms)}});
        else if constexpr (std::is_same_v<T, PrivateShopOrderQuote>)
          return obj({{"duration", V{Integer{0}}},
                      {"terms", dynamic_economy_codec::encode(value.terms)},
                      {"valid_through_tick", V{value.valid_through_tick}}});
        else if constexpr (std::is_same_v<T, PrivateShopClosingQuote>) {
          V::Array lots;
          for (const auto &lot : value.stock_lots)
            lots.push_back(obj(
                {{"item_id", V{lot.item_id}}, {"quantity", V{lot.quantity}}}));
          return obj({{"duration", V{Integer{0}}},
                      {"returned_cash", V{value.returned_cash}},
                      {"returned_lots", V{std::move(lots)}}});
        } else
          return obj(
              {{"duration", V{Integer{0}}}, {"action_id", V{value.action_id}}});
      },
      q.data);
  return obj({{"ok", V{true}},
              {"kind", V{private_shop_command_kind(q.command)}},
              {"actor_id", V{q.actor_id}},
              {"params", encode_private_shop_command(q.command)},
              {"terms", std::move(detail)}});
}
} // namespace hanmo::resident
