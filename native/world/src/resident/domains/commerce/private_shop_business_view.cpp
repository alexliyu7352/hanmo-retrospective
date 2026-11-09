// 每页至多十六项；本人账与当前会面分流，远方经营者、陌生订单和店主余额不向店员泄露。
#include "private_shop_business_view.hpp"
#include "private_shop_codec.hpp"
#include "resident/domains/local_economy/economic_payload_guard.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V = BoundaryValue;
using A = V::Array;
using D = V::Dict;
using boundary::obj;
constexpr Integer limit = 16;
V identity(const WorldIdentity &s) {
  StableId id;
  constexpr char hex[] = "0123456789abcdef";
  for (auto b : s.world_id) {
    id += hex[b >> 4];
    id += hex[b & 15];
  }
  return obj({{"world_id", V{id}},
              {"execution_epoch", V{s.execution_epoch}},
              {"revision", V{s.revision}},
              {"tick", V{s.tick}}});
}
// 所有身份字段及原命令回显必须一致，不能把另一候选的报价挂到今天的按钮。
bool same(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
Outcome<V> action(const PrivateShopCommandContext &x, const StableId &actor,
                  const PrivateShopCommand &command, const StableId &text) {
  auto q = preview_private_shop_command(x, actor, command);
  const auto kind = private_shop_command_kind(command);
  auto params = encode_private_shop_command(command);
  if (q.ok &&
      (!same(q.value.source_identity, x.shops.common.state().identity) ||
       q.value.actor_id != actor ||
       private_shop_command_kind(q.value.command) != kind ||
       !compare_boundary(params, encode_private_shop_command(q.value.command))
            .exact()))
    return {
        false, {}, common_failure("PrivateShopPreviewSourceMismatch").error};
  return {true,
          obj({{"command", V{kind}},
               {"params", params},
               {"text", V{text}},
               {"available", V{q.ok}},
               {"code", V{q.ok ? StableId{} : q.error.original.code}},
               {"quote", q.ok ? encode_private_shop_quote(q.value) : V{}}}),
          {}};
}
// 当前provider身份仅取实际已开启会面；不靠作者列表假造NPC同意或反转会话。
const InteractionSession *session(const PrivateShopCommandContext &x,
                                  const StableId &actor) {
  for (const auto &s : x.meeting.state.interactions)
    if (s.actor_id == actor)
      return &s;
  return nullptr;
}
V source(const PrivateShopSource &s) {
  if (!s.accounts_proven)
    return obj(
        {{"shop_id", V{s.shop_id}},
         {"premises_id", V{s.premises_id}},
         {"place_id", V{s.place_id}},
         {"opened_tick", V{s.opened_tick}},
         {"observation", V{"firsthand_documents"}},
         {"unavailable_code", V{"PrivateShopLocalObservationRequired"}}});
  return obj({{"observation", V{"current_local"}},
              {"shop_id", V{s.shop_id}},
              {"premises_id", V{s.premises_id}},
              {"place_id", V{s.place_id}},
              {"status", V{s.status}},
              {"markup_basis_points", V{s.markup_basis_points}},
              {"operating_days", V{s.operating_days}},
              {"completed_orders", V{s.completed_orders}},
              {"net_income", V{s.net_income}},
              {"unclosed_rent_paid", V{s.unclosed_rent_paid}},
              {"unclosed_wage_paid", V{s.unclosed_wage_paid}},
              {"milestone_reached", V{s.milestone_reached}},
              {"unavailable_code", V{s.unavailable_code}}});
}
} // namespace
// 查询同样拒绝重复键和隐含Actor参数；观察者来自World当前受控本人。
Outcome<PrivateShopBusinessQuery>
decode_private_shop_business_query(const BoundaryValue &raw) {
  try {
    using namespace boundary;
    auto valid = validate_economic_payload_fields(raw);
    if (!valid.ok)
      return {false, {}, valid.error};
    fields(raw, {},
           {"tab", "shop_id", "cursor", "fund_amount", "restock_quantity",
            "contract_id"},
           "private_shop_business");
    PrivateShopBusinessQuery q;
    if (raw.find("tab"))
      q.tab = get_string(raw, "tab", "query");
    if (raw.find("shop_id"))
      q.shop_id = get_string(raw, "shop_id", "query");
    if (raw.find("contract_id"))
      q.contract_id = get_string(raw, "contract_id", "query");
    if (raw.find("cursor"))
      q.cursor = get_int(raw, "cursor", "query");
    if (raw.find("fund_amount"))
      q.fund_amount = get_int(raw, "fund_amount", "query");
    if (raw.find("restock_quantity"))
      q.restock_quantity = get_int(raw, "restock_quantity", "query");
    if (q.cursor < 0 || q.cursor > INT64_MAX - limit || q.fund_amount <= 0 ||
        q.restock_quantity <= 0 || q.restock_quantity > 500 ||
        q.shop_id.size() > 256 || q.contract_id.size() > 256 ||
        (q.tab == "staff_ledger" ? q.contract_id.empty()
                                 : !q.contract_id.empty()))
      fail("PrivateShopReadArguments", "query");
    if (q.tab != "shops" && q.tab != "premises" && q.tab != "staff" &&
        q.tab != "staff_offers" && q.tab != "shop" && q.tab != "meetings" &&
        q.tab != "employees" && q.tab != "orders" && q.tab != "ledger" &&
        q.tab != "staff_ledger")
      fail("UnknownPrivateShopPage", "tab");
    return {true, std::move(q), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<BoundaryValue>
query_private_shop_business(const PrivateShopCommandContext &x,
                            const StableId &actor,
                            const PrivateShopBusinessQuery &query) {
  auto fail = [](const StableId &code) -> Outcome<V> {
    return {false, {}, common_failure(code, "private_shops").error};
  };
  const auto &c = x.shops.common;
  if (x.shops.phase || !validate_private_shop_committed_read3(x.shops).ok)
    return fail("PrivateShopReadRequiresCommittedClock");
  if (actor.empty() || actor != c.state().metadata.player_actor_id ||
      !c.actor(actor))
    return fail("PrivateShopObserverRequired");
  if (query.cursor < 0 || query.cursor > INT64_MAX - limit ||
      query.fund_amount <= 0 || query.restock_quantity <= 0 ||
      query.restock_quantity > 500 || query.shop_id.size() > 256 ||
      query.contract_id.size() > 256 ||
      (query.tab == "staff_ledger" ? query.contract_id.empty()
                                   : !query.contract_id.empty()))
    return fail("PrivateShopReadArguments");
  auto valid = validate_private_shops(x.shops);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto location = c.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  const auto *place = std::get_if<SettlementLocation>(&location.value);
  const auto *meeting = session(x, actor);
  const auto *selected =
      query.shop_id.empty() ? nullptr : x.shops.state.shops.find(query.shop_id);
  const bool selected_local =
      selected && place && place->place_id == selected->terms.place_id;
  A rows;
  bool more = false;
  Integer seen = 0;
  auto take = [&]() {
    const auto i = seen++;
    if (i < query.cursor)
      return false;
    if (i >= query.cursor + limit) {
      more = true;
      return false;
    }
    return true;
  };
  auto append_action = [&](A &to, const PrivateShopCommand &command,
                           const StableId &label) -> Status {
    auto a = action(x, actor, command, label);
    if (!a.ok)
      return {false, {}, a.error};
    to.push_back(std::move(a.value));
    return common_success();
  };
  if (query.tab == "shops") {
    auto page = query_private_shops(x.shops, actor, query.cursor, limit);
    if (!page.ok)
      return {false, {}, page.error};
    for (const auto &row : page.value.shops) {
      auto value = source(row.source);
      auto &fields = *value.as<D>();
      if (row.source.accounts_proven) {
        fields.push_back({"cash", V{row.cash}});
        fields.push_back({"spendable", V{row.spendable}});
        fields.push_back({"inventory_weight", V{row.inventory_weight}});
        fields.push_back({"inventory_capacity", V{row.inventory_capacity}});
        fields.push_back({"active_action_id", V{row.active_action_id}});
        fields.push_back({"remaining_ticks", V{row.remaining_ticks}});
      }
      rows.push_back(std::move(value));
    }
    more = page.value.has_more;
  } else if (query.tab == "premises") {
    for (const auto &row : x.shops.catalog.premises.rows())
      if (place && row.second.place_id == place->place_id && take()) {
        A actions;
        valid = append_action(actions, PrivateShopOpen{row.first},
                              "当面核对租约并开设");
        if (!valid.ok)
          return {false, {}, valid.error};
        rows.push_back(obj({{"id", V{row.first}},
                            {"opening_fee", V{row.second.opening_fee}},
                            {"initial_capital", V{row.second.initial_capital}},
                            {"daily_rent", V{row.second.daily_rent}},
                            {"actions", V{std::move(actions)}}}));
      }
  } else if (query.tab == "staff") {
    PrivateShopStaffAccess read(x.shops);
    for (const auto &row : x.shops.state.staff.contracts.rows())
      if (row.second.terms.worker_actor_id == actor && take()) {
        const auto *shop = x.shops.state.shops.find(row.second.terms.shop_id);
        if (!shop)
          return fail("PrivateShopStaffSourceMismatch");
        if (!place || place->place_id != shop->terms.place_id) {
          rows.push_back(
              obj({{"id", V{row.first}},
                   {"shop_id", V{shop->id}},
                   {"employer_actor_id", V{row.second.terms.employer_actor_id}},
                   {"accepted_tick", V{row.second.accepted_tick}},
                   {"through_tick", V{row.second.through_tick}},
                   {"wage_numerator", V{row.second.terms.wage_numerator}},
                   {"wage_denominator", V{row.second.terms.wage_denominator}},
                   {"observation", V{"firsthand_documents"}},
                   {"actions", V{A{}}}}));
          continue;
        }
        auto s = read.private_shop_staff_source(row.first);
        if (!s.ok)
          return {false, {}, s.error};
        if (!s.value)
          return fail("PrivateShopStaffSourceMismatch");
        A actions;
        if (s.value->status == "active") {
          valid =
              append_action(actions,
                            PrivateShopOperate{s.value->terms.shop_id,
                                               s.value->terms.service_item_id},
                            "按原合同到岗营业");
          if (!valid.ok)
            return {false, {}, valid.error};
          valid = append_action(actions,
                                PrivateShopStaffDecision{row.first, "leave"},
                                "本人离职并结清已赚工钱");
          if (!valid.ok)
            return {false, {}, valid.error};
        }
        rows.push_back(
            obj({{"id", V{row.first}},
                 {"shop_id", V{s.value->terms.shop_id}},
                 {"status", V{s.value->status == "worker_dead" ||
                                      s.value->status == "employer_dead"
                                  ? StableId{"terminated"}
                                  : s.value->status}},
                 {"through_tick", V{s.value->through_tick}},
                 {"served_ticks", V{s.value->served_ticks}},
                 {"earned_cash", V{s.value->earned_cash}},
                 {"paid_cash", V{s.value->paid_cash}},
                 {"reserved_cash", V{s.value->reserved_cash}},
                 {"actions", V{std::move(actions)}}}));
      }
  } else if (query.tab == "staff_offers") {
    if (meeting)
      for (const auto &policy : x.shops.catalog.staff.policies.rows())
        if (policy.second.worker_actor_id == actor)
          for (const auto &shop : x.shops.state.shops.rows())
            if (place && place->place_id == shop.second.terms.place_id &&
                shop.second.terms.premises_id == policy.second.premises_id &&
                shop.second.status != "closed" &&
                shop.second.terms.proprietor_actor_id ==
                    meeting->context.provider_actor_id &&
                take()) {
              A actions;
              valid = append_action(
                  actions, PrivateShopStaffAccept{shop.first, policy.first},
                  "本人接受当面店员约定");
              if (!valid.ok)
                return {false, {}, valid.error};
              rows.push_back(obj({{"id", V{policy.first}},
                                  {"shop_id", V{shop.first}},
                                  {"actions", V{std::move(actions)}}}));
            }
  } else if (query.tab == "shop") {
    const auto *shop = x.shops.state.shops.find(query.shop_id);
    if (!shop || shop->terms.proprietor_actor_id != actor)
      return fail("PrivateShopProprietorRequired");
    PrivateShopAccess read(x.shops);
    auto s = read.private_shop_source(actor, shop->id);
    if (!s.ok)
      return {false, {}, s.error};
    if (!s.value)
      return fail("PrivateShopSourceChanged");
    A actions;
    auto add = [&](const PrivateShopCommand &cmd,
                   const StableId &text) -> bool {
      valid = append_action(actions, cmd, text);
      return valid.ok;
    };
    if (s.value->accounts_proven && shop->status != "closed") {
      if (!add(PrivateShopFund{shop->id, query.fund_amount}, "追加本人资金") ||
          !add(PrivateShopSetMarkup{shop->id, 0}, "平价，每日最多三件") ||
          !add(PrivateShopSetMarkup{shop->id, 500}, "加价5%，每日最多两件") ||
          !add(PrivateShopSetMarkup{shop->id, 1000}, "加价10%，每日最多一件") ||
          !add(PrivateShopClose{shop->id}, "清理全部责任并关店"))
        return {false, {}, valid.error};
      for (const auto &operation : x.shops.state.operations.rows())
        if (operation.second.shop_id == shop->id &&
            operation.second.actor_id == actor &&
            operation.second.result_fact_id.empty())
          if (!add(PrivateShopCancelOperation{operation.first},
                   "中断本次营业，结清实际费用"))
            return {false, {}, valid.error};
    }
    rows.push_back(obj({{"id", V{shop->id}},
                        {"source", source(*s.value)},
                        {"actions", V{std::move(actions)}}}));
    if (s.value->accounts_proven && x.shops.markets &&
        shop->status != "closed") {
      const auto *market =
          x.shops.markets->shops.find(shop->terms.market_shop_id);
      for (const auto &item : x.shops.markets->tradeable_items)
        if (take()) {
          A buttons;
          valid = append_action(buttons, PrivateShopOperate{shop->id, item},
                                "本人到岗经营此货");
          if (!valid.ok)
            return {false, {}, valid.error};
          if (market && meeting &&
              std::find(market->representative_roles.begin(),
                        market->representative_roles.end(),
                        meeting->context.role_id) !=
                  market->representative_roles.end()) {
            valid = append_action(buttons,
                                  PrivateShopRestock{shop->id, item,
                                                     query.restock_quantity,
                                                     meeting->context},
                                  "按原市场报价采购入店");
            if (!valid.ok)
              return {false, {}, valid.error};
          }
          auto stock = c.count(shop->inventory_id, item,
                               {{Owner::Kind::actor, actor, {}}}, {});
          if (!stock.ok)
            return {false, {}, stock.error};
          rows.push_back(obj({{"id", V{item}},
                              {"stock_quantity", V{stock.value}},
                              {"actions", V{std::move(buttons)}}}));
        }
    }
  } else if (query.tab == "meetings") {
    const auto *shop = x.shops.state.shops.find(query.shop_id);
    if (!shop || shop->terms.proprietor_actor_id != actor)
      return fail("PrivateShopProprietorRequired");
    if (selected_local && meeting) {
      for (const auto &policy : x.shops.catalog.staff.policies.rows())
        if (policy.second.premises_id == shop->terms.premises_id &&
            policy.second.worker_actor_id ==
                meeting->context.provider_actor_id &&
            take()) {
          A actions;
          valid =
              append_action(actions, PrivateShopEmploy{shop->id, policy.first},
                            "请求本人明确答复雇用");
          if (!valid.ok)
            return {false, {}, valid.error};
          rows.push_back(obj(
              {{"id", V{policy.first}}, {"actions", V{std::move(actions)}}}));
        }
      for (const auto &policy : x.shops.catalog.orders.policies.rows())
        if (policy.second.premises_id == shop->terms.premises_id &&
            policy.second.client_actor_id ==
                meeting->context.provider_actor_id &&
            take()) {
          A actions;
          valid = append_action(actions,
                                PrivateShopRequestOrder{shop->id, policy.first},
                                "记录客户当面订单");
          if (!valid.ok)
            return {false, {}, valid.error};
          rows.push_back(obj(
              {{"id", V{policy.first}}, {"actions", V{std::move(actions)}}}));
        }
    }
  } else if (query.tab == "employees") {
    const auto *shop = x.shops.state.shops.find(query.shop_id);
    if (!shop || shop->terms.proprietor_actor_id != actor)
      return fail("PrivateShopProprietorRequired");
    PrivateShopStaffAccess read(x.shops);
    for (const auto &row : x.shops.state.staff.contracts.rows())
      if (row.second.terms.shop_id == shop->id && take()) {
        if (!selected_local) {
          rows.push_back(
              obj({{"id", V{row.first}},
                   {"shop_id", V{shop->id}},
                   {"worker_actor_id", V{row.second.terms.worker_actor_id}},
                   {"accepted_tick", V{row.second.accepted_tick}},
                   {"through_tick", V{row.second.through_tick}},
                   {"wage_numerator", V{row.second.terms.wage_numerator}},
                   {"wage_denominator", V{row.second.terms.wage_denominator}},
                   {"observation", V{"firsthand_documents"}},
                   {"actions", V{A{}}}}));
          continue;
        }
        auto s = read.private_shop_staff_source(row.first);
        if (!s.ok)
          return {false, {}, s.error};
        if (!s.value)
          return fail("PrivateShopStaffSourceMismatch");
        A actions;
        if (s.value->status == "active") {
          valid = append_action(actions,
                                PrivateShopStaffDecision{row.first, "dismiss"},
                                "终止店员约定并结清已赚工钱");
          if (!valid.ok)
            return {false, {}, valid.error};
        }
        rows.push_back(
            obj({{"id", V{row.first}},
                 {"worker_actor_id", V{s.value->terms.worker_actor_id}},
                 {"status", V{s.value->status == "worker_dead" ||
                                      s.value->status == "employer_dead"
                                  ? StableId{"terminated"}
                                  : s.value->status}},
                 {"through_tick", V{s.value->through_tick}},
                 {"served_ticks", V{s.value->served_ticks}},
                 {"earned_cash", V{s.value->earned_cash}},
                 {"paid_cash", V{s.value->paid_cash}},
                 {"reserved_cash", V{s.value->reserved_cash}},
                 {"actions", V{std::move(actions)}}}));
      }
  }
  // 工资明细只给原雇约双方；店员不能借这个页读取店金、采购或其他人的合同。
  else if (query.tab == "staff_ledger") {
    const auto *contract =
        x.shops.state.staff.contracts.find(query.contract_id);
    if (!contract ||
        (actor != contract->terms.employer_actor_id &&
         actor != contract->terms.worker_actor_id) ||
        (!query.shop_id.empty() && query.shop_id != contract->terms.shop_id))
      return fail("PrivateShopStaffPartyRequired");
    const auto *shop = x.shops.state.shops.find(contract->terms.shop_id);
    if (!shop)
      return fail("PrivateShopStaffSourceMismatch");
    const bool local = place && place->place_id == shop->terms.place_id;
    const auto &facts = c.state().facts.order();
    for (auto i = facts.rbegin(); i != facts.rend(); ++i) {
      // 本人已作的离职/辞退始终为已知原事实；对方后来死亡、到期或决定不凭此变成送达消息。
      if (const auto *ended = x.shops.state.staff.decisions.find(*i)) {
        if (ended->contract_id == contract->id &&
            private_shop_history_is_firsthand(x.shops.state, actor, *i) &&
            take())
          rows.push_back(obj({{"id", V{ended->id}},
                              {"record_kind", V{"staff_decision"}},
                              {"contract_id", V{contract->id}},
                              {"shop_id", V{contract->terms.shop_id}},
                              {"tick", V{ended->tick}},
                              {"actor_id", V{ended->actor_id}},
                              {"decision", V{ended->decision}}}));
        continue;
      }
      const auto *service = x.shops.state.staff.services.find(*i);
      if (!service || service->contract_id != contract->id ||
          (!local &&
           !private_shop_history_is_firsthand(x.shops.state, actor, *i)))
        continue;
      const auto *due = x.shops.economy.arrears.find(service->arrear_id);
      const auto *paid = x.shops.economy.payments.find(service->payment_id);
      if (!due || !paid || paid->id != service->payment_id ||
          paid->amount != service->amount || paid->arrear_id != due->id ||
          due->obligation_id != contract->obligation_id ||
          paid->obligation_id != contract->obligation_id)
        return fail("PrivateShopStaffWageReceiptMismatch");
      if (take())
        rows.push_back(obj({{"id", V{service->id}},
                            {"record_kind", V{"staff_wage"}},
                            {"contract_id", V{contract->id}},
                            {"shop_id", V{contract->terms.shop_id}},
                            {"tick", V{service->tick}},
                            {"operation_id", V{service->operation_id}},
                            {"attendance_index", V{service->attendance_index}},
                            {"closing_fraction", V{service->closing_fraction}},
                            {"earned_cash", V{service->amount}},
                            {"paid_cash", V{paid->amount}},
                            {"arrear_id", V{due->id}},
                            {"payment_id", V{paid->id}}}));
    }
  }
  // 账目按真实Common事实逆序逐页，只列本人已发生结果；购入成本与投入本金不在此重复算利润。
  else if (query.tab == "ledger") {
    const auto *shop = x.shops.state.shops.find(query.shop_id);
    if (!shop || shop->terms.proprietor_actor_id != actor)
      return fail("PrivateShopProprietorRequired");
    const auto &facts = c.state().facts.order();
    for (auto i = facts.rbegin(); i != facts.rend(); ++i) {
      const auto &id = *i;
      if (!selected_local &&
          !private_shop_history_is_firsthand(x.shops.state, actor, id))
        continue;
      V row;
      if (id == shop->id)
        row = obj({{"id", V{id}},
                   {"record_kind", V{"opening"}},
                   {"tick", V{shop->opened_tick}},
                   {"opening_fee", V{shop->terms.opening_fee}},
                   {"initial_capital", V{shop->terms.initial_capital}}});
      else if (const auto *v = x.shops.state.capital.find(id)) {
        if (v->shop_id != shop->id)
          continue;
        row = obj({{"id", V{id}},
                   {"record_kind", V{"capital"}},
                   {"tick", V{v->tick}},
                   {"amount", V{v->amount}},
                   {"from_wallet_id", V{v->from_wallet_id}},
                   {"to_wallet_id", V{v->to_wallet_id}}});
      } else if (const auto *v = x.shops.state.markups.find(id)) {
        if (v->shop_id != shop->id)
          continue;
        row = obj({{"id", V{id}},
                   {"record_kind", V{"markup"}},
                   {"tick", V{v->tick}},
                   {"before_basis_points", V{v->before_basis_points}},
                   {"after_basis_points", V{v->after_basis_points}}});
      } else if (const auto *v = x.shops.state.purchases.find(id)) {
        if (v->shop_id != shop->id)
          continue;
        row = obj({{"id", V{id}},
                   {"record_kind", V{"stock"}},
                   {"tick", V{v->tick}},
                   {"item_id", V{v->item_id}},
                   {"quantity", V{v->quantity}},
                   {"purchase_cash", V{v->purchase_cash}},
                   {"trade_fact_id", V{v->trade_fact_id}}});
        if (selected_local) {
          auto cost = derive_private_shop_purchase_cost_view(x.shops.state, *v);
          if (!cost.ok)
            return {false, {}, cost.error};
          auto &fields = *row.as<D>();
          fields.insert(
              fields.end(),
              {{"cost_observed_tick", V{c.tick()}},
               {"sold_quantity", V{cost.value.sold_quantity}},
               {"recognized_cost", V{cost.value.recognized_cost}},
               {"unrecognized_quantity", V{cost.value.unrecognized_quantity}},
               {"unrecognized_cost", V{cost.value.unrecognized_cost}}});
        }
      } else if (const auto *v = x.shops.state.operating_results.find(id)) {
        if (v->shop_id != shop->id)
          continue;
        const auto *operation = x.shops.state.operations.find(v->operation_id);
        if (!operation)
          return fail("PrivateShopActionMismatch");
        row = obj({{"id", V{id}},
                   {"record_kind", V{"retail"}},
                   {"tick", V{v->tick}},
                   {"completed", V{v->completed}},
                   {"reason", V{v->reason}},
                   {"item_id", V{v->item_id}},
                   {"quantity", V{v->quantity}},
                   {"income", V{v->income}},
                   {"cost_of_goods", V{v->cost_of_goods}},
                   {"rent_paid", V{v->rent_paid}},
                   {"wage_paid", V{v->wage_paid}},
                   {"net_income", V{v->net_income}},
                   {"operation_id", V{v->operation_id}},
                   {"rent_payment_id", V{operation->rent_payment_id}}});
      } else if (const auto *v = x.shops.state.order_settlements.find(id)) {
        const auto *order = x.shops.state.orders.orders.find(v->order_id);
        if (!order)
          return fail("PrivateShopOrderSettlementRequired");
        if (order->terms.shop_id != shop->id)
          continue;
        Integer net = 0;
        if (__builtin_sub_overflow(v->income, v->cost_of_goods, &net))
          return fail("PrivateShopNetIncomeOverflow");
        row = obj({{"id", V{id}},
                   {"record_kind", V{"order"}},
                   {"tick", V{v->tick}},
                   {"outcome", V{v->outcome}},
                   {"order_id", V{v->order_id}},
                   {"task_id", V{v->task_id}},
                   {"receipt_fact_id", V{v->receipt_fact_id}},
                   {"task_settlement_id", V{v->task_settlement_id}},
                   {"income", V{v->income}},
                   {"cost_of_goods", V{v->cost_of_goods}},
                   {"net_income", V{net}}});
      } else if (const auto *v = x.shops.state.closings.find(id)) {
        if (v->shop_id != shop->id)
          continue;
        Integer quantity = 0;
        for (const auto &lot : v->returned_lots)
          if (__builtin_add_overflow(quantity, lot.quantity, &quantity))
            return fail("PrivateShopStockOverflow");
        row = obj({{"id", V{id}},
                   {"record_kind", V{"closing"}},
                   {"tick", V{v->tick}},
                   {"returned_cash", V{v->returned_cash}},
                   {"returned_quantity", V{quantity}}});
      } else
        continue;
      if (take())
        rows.push_back(std::move(row));
    }
  } else if (query.tab == "orders") {
    const auto *shop = x.shops.state.shops.find(query.shop_id);
    if (!shop || shop->terms.proprietor_actor_id != actor)
      return fail("PrivateShopProprietorRequired");
    for (const auto &row : x.shops.state.orders.orders.rows())
      if (row.second.terms.shop_id == shop->id && take()) {
        if (!selected_local) {
          rows.push_back(
              obj({{"id", V{row.first}},
                   {"order_id", V{row.first}},
                   {"item_id", V{row.second.terms.item_id}},
                   {"quantity", V{row.second.terms.quantity}},
                   {"contract_price", V{row.second.terms.contract_price}},
                   {"valid_through_tick", V{row.second.valid_through_tick}},
                   {"observation", V{"firsthand_documents"}}}));
          continue;
        }
        const PrivateShopOrderSettlement *execution = nullptr;
        for (const auto &e : x.shops.state.order_settlements.rows())
          if (e.second.order_id == row.first)
            execution = &e.second;
        rows.push_back(obj(
            {{"id", V{row.first}},
             {"order_id", V{row.first}},
             {"task_id", V{row.second.task_id}},
             {"status", V{execution ? execution->outcome : row.second.status}},
             {"item_id", V{row.second.terms.item_id}},
             {"quantity", V{row.second.terms.quantity}},
             {"contract_price", V{row.second.terms.contract_price}},
             {"valid_through_tick", V{row.second.valid_through_tick}},
             {"income", V{execution ? execution->income : 0}},
             {"cost_of_goods", V{execution ? execution->cost_of_goods : 0}}}));
      }
  } else
    return fail("UnknownPrivateShopPage");
  if (query.cursor > seen && query.tab != "shops")
    return fail("PrivateShopReadCursor");
  D page = {{"ok", V{true}},
            {"source_identity", identity(c.state().identity)},
            {"observer_actor_id", V{actor}},
            {"tab", V{query.tab}},
            {"shop_id", V{query.shop_id}},
            {"contract_id", V{query.contract_id}},
            {"place_id", V{place ? place->place_id : StableId{}}},
            {"rows", V{std::move(rows)}}};
  if (selected && selected->terms.proprietor_actor_id == actor)
    page.push_back({"observation", V{selected_local ? "current_local"
                                                    : "firsthand_documents"}});
  if (more) {
    Integer next = 0;
    if (__builtin_add_overflow(query.cursor, limit, &next))
      return fail("PrivateShopReadCursor");
    page.push_back({"next_cursor", V{next}});
  }
  return {true, V{std::move(page)}, {}};
}
} // namespace hanmo::resident
