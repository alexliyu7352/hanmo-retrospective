// 一套字段描述同时写原JSON、bounded shape与成对checkpoint；买方/卖方/工时绑定均有限typed分支。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "household_purchase_contract.hpp"
#include "market_service_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/travel/mobile_inventory_adoption.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
#include <array>
#include <cstdint>
#include <type_traits>
namespace hanmo::resident::household_codec {
template<class T>struct EnumNames;
template<>struct EnumNames<HouseholdServiceEventKind>{inline static constexpr std::pair<HouseholdServiceEventKind,const char*> names[]={{HouseholdServiceEventKind::accepted,"accepted"},{HouseholdServiceEventKind::resumed,"resumed"},{HouseholdServiceEventKind::suspended,"suspended"},{HouseholdServiceEventKind::completed,"completed"},{HouseholdServiceEventKind::terminated,"terminated"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<MarketStaffingRule>{inline static constexpr std::pair<MarketStaffingRule,const char*> names[]={{MarketStaffingRule::one_qualified_worker_per_service_slot,"one_qualified_worker_per_service_slot"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PublicMarketLaborReleaseReason>{inline static constexpr std::pair<PublicMarketLaborReleaseReason,const char*> names[]={{PublicMarketLaborReleaseReason::completed,"completed"},{PublicMarketLaborReleaseReason::mutually_cancelled,"mutually_cancelled"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<PublicMarketStaffingEventKind>{inline static constexpr std::pair<PublicMarketStaffingEventKind,const char*> names[]={{PublicMarketStaffingEventKind::accepted,"accepted"},{PublicMarketStaffingEventKind::qualification_changed,"qualification_changed"},{PublicMarketStaffingEventKind::stop_at_boundary,"stop_at_boundary"},{PublicMarketStaffingEventKind::completed,"completed"}};static constexpr const auto& rows(){return names;}};
template<class A,class T>auto record(A& a,T& v)->decltype(public_market_record(a,v)){public_market_record(a,v);}
template<class T>struct VariantNames;
template<class T>struct IsVariant:std::false_type{};
template<class...T>struct IsVariant<std::variant<T...>>:std::true_type{};
template<std::size_t I=0,class A,class T>Status select_variant(A& archive,T& value,std::size_t index){if constexpr(I<std::variant_size_v<T>){if(index==I){auto prepared=archive.template prepare_default_values<std::variant_alternative_t<I,T>>();if(!prepared.ok)return prepared;value.template emplace<I>();return archive.status();}return select_variant<I+1>(archive,value,index);}else return archive.reject("UnsupportedHouseholdVariant");}
template<class T>struct IsVector:std::false_type{};
template<class T>struct IsVector<std::vector<T>>:std::true_type{using Value=T;};
template<class T>struct IsOptional:std::false_type{};
template<class T>struct IsOptional<std::optional<T>>:std::true_type{using Value=T;};
template<class T>struct IsTable:std::false_type{};
template<class T>struct IsTable<OrderedTable<T>>:std::true_type{using Value=T;};
template<>struct VariantNames<HouseholdBuyer>{inline static constexpr std::array<std::string_view,2> values={"named","cohort"};static constexpr const auto& names(){return values;}};
template<>struct VariantNames<HouseholdRetailSeller>{inline static constexpr std::array<std::string_view,2> values={"public_market","private_shop"};static constexpr const auto& names(){return values;}};
template<>struct VariantNames<WorkExecutionBinding>{inline static constexpr std::array<std::string_view,2> values={"actor_action","owner_period"};static constexpr const auto& names(){return values;}};
template<>struct EnumNames<HouseholdPurchasePhase>{inline static constexpr std::pair<HouseholdPurchasePhase,const char*> names[]={{HouseholdPurchasePhase::planned,"planned"},{HouseholdPurchasePhase::servicing,"servicing"},{HouseholdPurchasePhase::awaiting_dispatch,"awaiting_dispatch"},{HouseholdPurchasePhase::in_transit,"in_transit"},{HouseholdPurchasePhase::blocked,"blocked"},{HouseholdPurchasePhase::received,"received"},{HouseholdPurchasePhase::settling,"settling"},{HouseholdPurchasePhase::cancelled,"cancelled"},{HouseholdPurchasePhase::lost,"lost"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkPeriodOwner>{inline static constexpr std::pair<WorkPeriodOwner,const char*> names[]={{WorkPeriodOwner::local_economy,"local_economy"},{WorkPeriodOwner::estate,"estate"},{WorkPeriodOwner::organization,"organization"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<WorkPeriodDisposition>{inline static constexpr std::pair<WorkPeriodDisposition,const char*> names[]={{WorkPeriodDisposition::provided,"provided"},{WorkPeriodDisposition::interrupted,"interrupted"},{WorkPeriodDisposition::no_work,"no_work"}};static constexpr const auto& rows(){return names;}};
template<>struct EnumNames<ResourceKey::Kind>{inline static constexpr std::pair<ResourceKey::Kind,const char*> names[]={{ResourceKey::Kind::actor,"actor"},{ResourceKey::Kind::wallet,"wallet"},{ResourceKey::Kind::inventory,"inventory"},{ResourceKey::Kind::action,"action"}};static constexpr const auto& rows(){return names;}};
template<class A>void record(A& a,SourceRef& v){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,WorldMoment& v){a("half_day_tick",v.half_day_tick);a("sub_half_day",v.sub_half_day);}
template<class A>void record(A& a,MomentSpan& v){a("from",v.from);a("to",v.to);}
template<class A>void record(A& a,ResourceKey& v){a("kind",v.kind);a("id",v.id);}
template<class A>void record(A& a,ReadVersion& v){a("resource",v.resource);a("version",v.version);}
template<class A>void record(A& a,NamedHouseholdBuyer& v){a("actor_id",v.actor_id);a("accepted_service_ref",v.accepted_service_ref);}
template<class A>void record(A& a,CohortHouseholdBuyer& v){a("cohort_id",v.cohort_id);a("population_selection_ref",v.population_selection_ref);a("labor_allocation_id",v.labor_allocation_id);a("workers",v.workers);a("accepted_household_policy_ref",v.accepted_household_policy_ref);}
template<class A>void record(A& a,PublicHouseholdRetailSeller& v){a("market_shop_id",v.market_shop_id);}
template<class A>void record(A& a,PrivateHouseholdRetailSeller& v){a("shop_id",v.shop_id);a("operation_id",v.operation_id);}
template<class A>void record(A& a,HouseholdItemCap& v){a("item_id",v.item_id);a("quantity_cap",v.quantity_cap);}
template<class A>void record(A& a,HouseholdProvisionControlProof& v){a("actor_id",v.actor_id);a("household_group_id",v.household_group_id);a("fact_id",v.fact_id);a("source_role_id",v.source_role_id);a("operation",v.operation);a("source_revision",v.source_revision);a("effective_from_tick",v.effective_from_tick);a("through_tick",v.through_tick);}
template<class A>void record(A& a,HouseholdPurchasePolicy& v){a("id",v.id);a("household_id",v.household_id);a("controller_actor_id",v.controller_actor_id);a("revision",v.revision);a("accepted_tick",v.accepted_tick);a("valid_from_tick",v.valid_from_tick);a("valid_until_tick",v.valid_until_tick);a("controller_authority_ref",v.controller_authority_ref);a("allowed_items",v.allowed_items);a("merchandise_cap_qian",v.merchandise_cap_qian);a("transport_cap_qian",v.transport_cap_qian);a("total_cap_qian",v.total_cap_qian);a("allowed_market_refs",v.allowed_market_refs);a("allowed_destination_inventories",v.allowed_destination_inventories);a("acceptance_source_ref",v.acceptance_source_ref);a("allow_cohort_buyer",v.allow_cohort_buyer);a("closed_source_ref",v.closed_source_ref);}
template<class A>void record(A& a,HouseholdMobileCustodyAdoption& v){a("plan_source",v.plan_source);a("actual_view",v.actual_view);}
template<class A>void record(A& a,HouseholdPurchasePlan& v){a("plan_id",v.plan_id);a("household_id",v.household_id);a("need_or_policy_ref",v.need_or_policy_ref);a("controller_authority_ref",v.controller_authority_ref);a("allowed_items",v.allowed_items);a("spend_cap_qian",v.spend_cap_qian);a("known_market_ref",v.known_market_ref);a("destination_inventory",v.destination_inventory);a("execution_choice",v.execution_choice);a("approach_travel_ref",v.approach_travel_ref);a("latest_start_tick",v.latest_start_tick);a("delivery_deadline",v.delivery_deadline);a("terms_version",v.terms_version);a("accepted_policy_ref",v.accepted_policy_ref);a("accepted_policy_revision",v.accepted_policy_revision);if(a.selected_version==3)a("mobile_custody_adoption",v.mobile_custody_adoption);else if(v.mobile_custody_adoption)a.reject("HouseholdMobileCustodyRequiresVersion3");}
template<class A>void record(A& a,ActorActionWorkBinding& v){a("action_id",v.action_id);a("action_kind",v.action_kind);a("source_owner_ref",v.source_owner_ref);}
template<class A>void record(A& a,OwnerPeriodWorkBinding& v){a("domain",v.domain);a("activity_kind",v.activity_kind);a("activity_id",v.activity_id);a("registered_period_profile",v.registered_period_profile);}
template<class A>void record(A& a,WorkPeriodRef& v){a("binding",v.binding);a("period_ordinal",v.period_ordinal);a("from_tick",v.from_tick);a("to_tick",v.to_tick);a("source_schema_version",v.source_schema_version);}
template<class A>void record(A& a,ActualActorWorkRole& v){a("actor_id",v.actor_id);a("role",v.role);}
template<class A>void record(A& a,WorkPeriodView& v){a("ref",v.ref);a("actual_actor_roles",v.actual_actor_roles);a("actual_labor_allocations",v.actual_labor_allocations);a("facility_claim_refs",v.facility_claim_refs);a("work_unit_id",v.work_unit_id);a("units_before",v.units_before);a("units_added",v.units_added);a("units_after",v.units_after);a("resource_effect_refs",v.resource_effect_refs);a("exertion_source_refs",v.exertion_source_refs);a("disposition",v.disposition);a("reason",v.reason);}
template<class A>void record(A& a,FacilitySlotRef& v){a("provider_domain",v.provider_domain);a("facility_id",v.facility_id);a("slot_kind",v.slot_kind);}
template<class A>void record(A& a,FacilityClaimReceiptRef& v){a("slot_ref",v.slot_ref);a("claim_set_revision",v.claim_set_revision);}
template<class A>void record(A& a,HouseholdMarketServiceDefinition& v){a("id",v.id);a("market_ref",v.market_ref);a("place_id",v.place_id);a("custody_inventory_id",v.custody_inventory_id);a("service_slot",v.service_slot);a("custody_terms_ref",v.custody_terms_ref);a("service_usage_right_ref",v.service_usage_right_ref);a("handling_rule_ref",v.handling_rule_ref);}
template<class A>void record(A& a,HouseholdReserveRule& v){a("id",v.id);a("household_id",v.household_id);a("item_id",v.item_id);a("destination_inventory_id",v.destination_inventory_id);a("minimum_stock",v.minimum_stock);a("minimum_cash_reserve",v.minimum_cash_reserve);}
template<class A>void record(A& a,HouseholdPurchaseCatalog& v){a("duration_definition_id",v.duration_definition_id);a("continuation_rule_id",v.continuation_rule_id);a("market_services",v.market_services);a("market_service_definitions",v.market_service_definitions);a("household_reserve_rules",v.household_reserve_rules);}
template<class A>void record(A& a,HouseholdPurchaseAcceptedTerms& v){a("source",v.source);a("plan",v.plan);a("seller",v.seller);a("item_id",v.item_id);a("quantity",v.quantity);a("quoted_total_qian",v.quoted_total_qian);a("accepted_at",v.accepted_at);a("presence_source_ref",v.presence_source_ref);a("accepted_versions",v.accepted_versions);}
template<class A>void record(A& a,HouseholdPurchase& v){a("purchase_id",v.purchase_id);a("plan_ref",v.plan_ref);a("accepted_terms_ref",v.accepted_terms_ref);a("executor",v.executor);a("execution_binding",v.execution_binding);a("service_claim_ref",v.service_claim_ref);a("retail_receipt_ref",v.retail_receipt_ref);a("holding_inventory_ref",v.holding_inventory_ref);a("delivery_binding",v.delivery_binding);a("state",v.state);a("phase_revision",v.phase_revision);a("last_consumed_period_ref",v.last_consumed_period_ref);a("terminal_result_ref",v.terminal_result_ref);}
template<class A>void record(A& a,HouseholdPurchaseServiceResult& v){a("id",v.id);a("purchase_id",v.purchase_id);a("serviced_window",v.serviced_window);a("coverage_source",v.coverage_source);a("close_receipt",v.close_receipt);a("period_ref",v.period_ref);a("completed_at",v.completed_at);}
template<class A>void record(A& a,HouseholdPurchaseHandover& v){a("id",v.id);a("purchase_id",v.purchase_id);a("actual_delivery_source",v.actual_delivery_source);a("occurred_at",v.occurred_at);a("from_inventory_id",v.from_inventory_id);a("to_inventory_id",v.to_inventory_id);a("actual_lots",v.actual_lots);}
template<class A>void record(A& a,HouseholdPurchasePhaseChange& v){a("id",v.id);a("purchase_id",v.purchase_id);a("before",v.before);a("after",v.after);a("revision_before",v.revision_before);a("revision_after",v.revision_after);a("actual_cause",v.actual_cause);a("occurred_at",v.occurred_at);}
template<class A>void record(A& a,HouseholdServiceSegment& v){a("purchase_id",v.purchase_id);a("window",v.window);a("actual_coverage_source",v.actual_coverage_source);a("labor_allocation_refs",v.labor_allocation_refs);a("exertion_source_refs",v.exertion_source_refs);}
struct HouseholdOriginalPayloadView {OrderedTable<HouseholdPurchasePolicy>& policies;OrderedTable<HouseholdPurchasePlan>& plans;OrderedTable<HouseholdPurchaseAcceptedTerms>& accepted_terms;OrderedTable<HouseholdPurchase>& purchases;OrderedTable<HouseholdPurchaseServiceResult>& service_results;OrderedTable<WorkPeriodView>& work_periods;OrderedTable<HouseholdPurchaseHandover>& handovers;OrderedTable<HouseholdPurchasePhaseChange>& phase_changes;std::vector<HouseholdServiceSegment>& service_segments;};
template<class A>void record(A& a,HouseholdOriginalPayloadView& v){a("policies",v.policies);a("plans",v.plans);a("accepted_terms",v.accepted_terms);a("purchases",v.purchases);a("service_results",v.service_results);a("work_periods",v.work_periods);a("handovers",v.handovers);a("phase_changes",v.phase_changes);a("service_segments",v.service_segments);}
template<class A>void record(A& a,HouseholdPurchaseState& v){HouseholdOriginalPayloadView original{v.policies,v.plans,v.accepted_terms,v.purchases,v.service_results,v.work_periods,v.handovers,v.phase_changes,v.service_segments};a("original_payload",original);a("named_service_events",v.named_service_events);a("named_service_claim_episodes",v.named_service_claim_episodes);a("market_service_definitions",v.market_service_definitions);a("market_service_sources",v.market_service_sources);a("public_staffing_offers",v.public_staffing_offers);a("public_staffing_consents",v.public_staffing_consents);a("public_staffing_bindings",v.public_staffing_bindings);a("public_staffing_events",v.public_staffing_events);a("public_staffing_release_consents",v.public_staffing_release_consents);}
template<class A>void record(A& a,CargoSelection& v){a("inventory_id",v.inventory_id);a("lot_id",v.lot_id);a("quantity",v.quantity);}
template<class A>void record(A& a,VerifiedTradeLot& v){a("original_lot_id",v.original_lot_id);a("destination_lot_id",v.destination_lot_id);a("quantity",v.quantity);}
template<class A>void record(A& a,PrivateShopRetailLot& v){a("purchase_fact_id",v.purchase_fact_id);a("lot_id",v.lot_id);a("quantity",v.quantity);a("cost",v.cost);}
template<class A>void record(A& a,HouseholdRetailTerms& v){a("purchase_id",v.purchase_id);a("household_group_id",v.household_group_id);a("executor",v.executor);a("seller_ref",v.seller_ref);a("item_id",v.item_id);a("buyer_wallet_id",v.buyer_wallet_id);a("seller_wallet_id",v.seller_wallet_id);a("source_inventory_id",v.source_inventory_id);a("custody_inventory_id",v.custody_inventory_id);a("destination_inventory_id",v.destination_inventory_id);a("place_id",v.place_id);a("buyer",v.buyer);a("seller",v.seller);a("quantity",v.quantity);a("total",v.total);a("market_revision",v.market_revision);a("market_stock",v.market_stock);a("base_price",v.base_price);a("midpoint",v.midpoint);a("unit_price",v.unit_price);a("quoted_at",v.quoted_at);a("selected_cargo",v.selected_cargo);a("accepted_terms_ref",v.accepted_terms_ref);a("custody_terms_ref",v.custody_terms_ref);a("terms_version",v.terms_version);}
template<class A>void record(A& a,HouseholdRetailSettlement& v){a("id",v.id);a("terms",v.terms);a("settled_at",v.settled_at);a("market_version_after",v.market_version_after);a("cost_of_goods",v.cost_of_goods);a("lots",v.lots);a("consumed_lots",v.consumed_lots);a("payer_balance_before",v.payer_balance_before);a("payer_balance_after",v.payer_balance_after);a("payee_balance_before",v.payee_balance_before);a("payee_balance_after",v.payee_balance_after);a("payment_ref",v.payment_ref);a("transfer_refs",v.transfer_refs);}
template<class A>void record(A& a,HouseholdRetailState& v){a("version",v.version);a("settlements",v.settlements);}
template<class A>void record(A& a,HouseholdRetailCatalog& v){a("version",v.version);a("premises_ids",v.premises_ids);a("public_market_shop_ids",v.public_market_shop_ids);}
template<class T> BoundaryValue encode(const T& value,Integer selected_version=2);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&,Integer selected_version=2);
template<class T> void decode(const BoundaryValue&,T&,std::string_view,Integer selected_version=2);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&,Integer selected_version=2);
struct Projector {ProjectionWriter& writer;Integer selected_version=2;template<class T>void operator()(const char* key,const T& v){project(writer,key,v,selected_version);}void reject(std::string_view code){writer.reject(code);}};
// 无符号字段仍是原数字标量；序列和可选值不得误建对象外壳。
template <class T>
inline constexpr bool json_scalar =
    std::is_same_v<T, StableId> || std::is_same_v<T, Integer> ||
    std::is_same_v<T, std::uint64_t> || std::is_same_v<T, bool> ||
    std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view,Integer);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view,Integer);
inline Status checked(Status status){if(!status.ok)throw boundary::DecodeFailure{status.error};return status;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder {
 ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;Integer selected_version=2;
 template<class T>void operator()(const char* key,T& value){checked(json_field(archive,input,key,value,path,selected_version));}
 void reject(std::string_view code){boundary::fail(code,path);}
 void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.finish());}
};
struct Checkpointer {
    CodecVisitor& codec;Integer selected_version=2;
    template<class T> void operator()(const char* key,T& v) { checkpoint(codec,key,v,selected_version); }
    void reject(std::string_view code){codec.reject(code);}
};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template <class T>
Status project(ProjectionWriter &w, std::string_view name, const T &value,
               Integer selected_version) {
  if constexpr (std::is_same_v<T, StableId> || std::is_same_v<T, Integer> ||
                std::is_same_v<T, bool>)
    return w.field(name, value);
  else if constexpr (std::is_same_v<T, std::uint64_t>)
    return w.unsigned_field(name, value);
  else if constexpr (std::is_same_v<T, MobileInventoryAdoptionView>)
    return emit_mobile_inventory_adoption(w, name, value);
  else if constexpr (std::is_same_v<T, Owner>)
    return emit_common_owner(w, name, value);
  else if constexpr (std::is_same_v<T, Location>)
    return emit_common_location(w, name, value);
  else if constexpr (std::is_same_v<T, CanonicalFraction>) {
    auto encoded = encode_canonical_fraction(value);
    if (!encoded.ok)
      return w.reject("InvalidEconomicExactFraction", name);
    return w.object(name, [&](auto &x) {
      x.field("numerator", encoded.value.numerator);
      return x.field("denominator", encoded.value.denominator);
    });
  } else if constexpr (IsVariant<T>::value && !IsMarketWireVariant<T>::value)
    return w.object(name, [&](auto &x) {
      x.field("kind", StableId{VariantNames<T>::names().at(value.index())});
      return std::visit(
          [&](const auto &v) {
            Projector fields{x, selected_version};
            record(fields, const_cast<std::decay_t<decltype(v)> &>(v));
            return x.status();
          },
          value);
    });
  else if constexpr (IsMarketWireVariant<T>::value)
    return w.object(name, [&](auto &x) {
      x.field("tag", static_cast<Integer>(value.index()));
      return std::visit(
          [&](const auto &v) {
            return project(x, "payload", v, selected_version);
          },
          value);
    });
  else if constexpr (std::is_enum_v<T>) {
    for (const auto &p : EnumNames<T>::rows())
      if (p.first == value)
        return w.field(name, StableId{p.second});
    return w.reject("InvalidDynamicEnum", name);
  } else if constexpr (IsVector<T>::value)
    return w.sequence(name, value, [&](auto &x, const auto &v) {
      return project(x, "", v, selected_version);
    });
  else if constexpr (IsOptional<T>::value) {
    if (value)
      return project(w, name, *value, selected_version);
    return w.null(name);
  } else if constexpr (IsTable<T>::value)
    return w.keyed_dictionary(name, {}, [&](auto &x) {
      for (const auto &p : value.rows()) {
        project(x, p.first, p.second, selected_version);
        if (!x.status().ok)
          break;
      }
      return x.status();
    });
  else
    return w.object(name, [&](auto &x) {
      Projector fields{x, selected_version};
      record(fields, const_cast<T &>(value));
      return x.status();
    });
}
template<class T> BoundaryValue encode(const T& value,Integer selected_version){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value,selected_version);if(status.ok)status=writer.finish();if(!status.ok)throw [&]{boundary::DecodeFailure failure{status.error.original.code,status.error.original.path};failure.error=status.error;return failure;}();return out;}
// 原数字类型门先验字段；无符号值不经有符号窄转，失败保留原路径。
template <class T>
Status json_value(ActorFoundationJsonVisitor &archive,
                  const BoundaryValue &input, T &value, std::string_view path,
                  Integer selected_version) {
  if constexpr (std::is_same_v<T, StableId>) {
    boundary::string_ref(input, {path});
    return archive.field("", value);
  } else if constexpr (std::is_same_v<T, Integer>) {
    boundary::integer(input, path);
    return archive.field("", value);
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    boundary::unsigned_integer(input, path);
    return archive.unsigned_field("", value);
  } else if constexpr (std::is_same_v<T, bool>) {
    boundary::boolean(input, path);
    return archive.field("", value);
  } else if constexpr (std::is_same_v<T, MobileInventoryAdoptionView>) {
    auto emit_original = [selected_version](ProjectionWriter &writer,
                                            const T &row) {
      return project(writer, "", row, selected_version);
    };
    if (ScopedCheckpointJsonDecode::current())
      return archive.foreign_record(
          value,
          [](const BoundaryValue &node, ScopedCheckpointJsonDecode &budget) {
            return decode_mobile_inventory_adoption(node, budget);
          },
          emit_original);
    return archive.foreign_record(
        value,
        [](const BoundaryValue &node) {
          return decode_mobile_inventory_adoption(node);
        },
        emit_original);
  } else if constexpr (std::is_same_v<T, Owner> ||
                       std::is_same_v<T, Location>) {
    auto original = [&](const BoundaryValue &node) -> Outcome<T> {
      try {
        if constexpr (std::is_same_v<T, Owner>)
          return {true,
                  boundary::decode_owner(node,
                                         checkpoint_decode_boundary_text(path)),
                  {}};
        else
          return {true,
                  boundary::decode_location(
                      node, checkpoint_decode_boundary_text(path)),
                  {}};
      } catch (const boundary::DecodeFailure &error) {
        return {false, {}, error.error};
      }
    };
    auto emit_original = [selected_version](ProjectionWriter &writer,
                                            const T &row) {
      return project(writer, "", row, selected_version);
    };
    if (ScopedCheckpointJsonDecode::current())
      return archive.foreign_record(
          value,
          [&](const BoundaryValue &node,
              ScopedCheckpointJsonDecode &budget) -> Outcome<T> {
            if (&budget != ScopedCheckpointJsonDecode::current())
              budget.fail("CheckpointCandidateMemoryJobMismatch",
                          "household_original");
            budget.template default_values<T>();
            return original(node);
          },
          emit_original);
    return archive.foreign_record(value, original, emit_original);
  } else if constexpr (std::is_same_v<T, CanonicalFraction>) {
    for (const auto &field : boundary::object(input, path))
      if (field.first != "numerator" && field.first != "denominator")
        boundary::fail_parts("UnsupportedResidentField",
                             {path, "/", field.first});
    checked(archive.defer_duplicate_field_check_to_finish());
    EncodedFraction fields;
    checked(json_field(archive, input, "numerator", fields.numerator, path,
                       selected_version));
    checked(json_field(archive, input, "denominator", fields.denominator, path,
                       selected_version));
    auto decoded = decode_canonical_fraction(fields);
    if (!decoded.ok)
      throw boundary::DecodeFailure{decoded.error};
    value = std::move(decoded.value);
    return archive.finish();
  } else if constexpr (IsVariant<T>::value && !IsMarketWireVariant<T>::value) {
    const auto &kind = boundary::string_ref(
        boundary::field(input, "kind", path), {path, "/kind"});
    const auto &names = VariantNames<T>::names();
    auto found = std::find(names.begin(), names.end(), kind);
    if (found == names.end())
      boundary::fail("UnsupportedHouseholdVariant", path);
    checked(archive.defer_duplicate_field_check_to_finish());
    StableId decoded_kind;
    checked(archive.field("kind", decoded_kind));
    checked(select_variant(archive, value,
                           static_cast<std::size_t>(found - names.begin())));
    std::visit(
        [&](auto &row) {
          Decoder fields{archive, input, path, selected_version};
          record(fields, row);
          fields.finish();
        },
        value);
    return archive.status();
  } else if constexpr (IsMarketWireVariant<T>::value) {
    for (const auto &row : boundary::object(input, path))
      if (row.first != "tag" && row.first != "payload")
        boundary::fail_parts("UnsupportedResidentField",
                             {path, "/", row.first});
    auto tag =
        boundary::integer(boundary::field(input, "tag", path),
                          checkpoint_decode_boundary_join({path, "/tag"}));
    if (tag < 0 || static_cast<std::size_t>(tag) >= std::variant_size_v<T>)
      boundary::fail("UnsupportedMarketWireVariant", path);
    checked(archive.defer_duplicate_field_check_to_finish());
    checked(archive.field("tag", tag));
    checked(select_market_wire_variant(archive, value,
                                       static_cast<std::size_t>(tag)));
    auto status = std::visit(
        [&](auto &row) {
          return json_field(archive, input, "payload", row, path,
                            selected_version);
        },
        value);
    checked(status);
    return archive.finish();
  } else if constexpr (std::is_enum_v<T>) {
    StableId name;
    boundary::string_ref(input, {path});
    checked(archive.field("", name));
    for (const auto &row : EnumNames<T>::rows())
      if (name == row.second) {
        value = row.first;
        return archive.status();
      }
    boundary::fail("InvalidDynamicEnum", path);
  } else {
    checked(archive.defer_duplicate_field_check_to_finish());
    Decoder fields{archive, input, path, selected_version};
    record(fields, value);
    fields.finish();
    return archive.status();
  }
}
// 按原字段名读取标量或容器；缺失、重复和错误类型仍由原访客拒绝。
template <class T>
Status json_field(ActorFoundationJsonVisitor &archive,
                  const BoundaryValue &input, std::string_view name, T &value,
                  std::string_view path, Integer selected_version) {
  const auto &node = boundary::field(input, name, path);
  const auto child_path = checkpoint_decode_boundary_join({path, "/", name});
  if constexpr (std::is_same_v<T, StableId>) {
    boundary::string_ref(node, {child_path});
    return archive.field(name, value);
  } else if constexpr (std::is_same_v<T, Integer>) {
    boundary::integer(node, child_path);
    return archive.field(name, value);
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    boundary::unsigned_integer(node, child_path);
    return archive.unsigned_field(name, value);
  } else if constexpr (std::is_same_v<T, bool>) {
    boundary::boolean(node, child_path);
    return archive.field(name, value);
  } else if constexpr (std::is_enum_v<T>) {
    StableId text;
    boundary::string_ref(node, {child_path});
    checked(archive.field(name, text));
    for (const auto &row : EnumNames<T>::rows())
      if (text == row.second) {
        value = row.first;
        return archive.status();
      }
    boundary::fail("InvalidDynamicEnum", child_path);
  } else if constexpr (IsVector<T>::value) {
    const auto &rows = boundary::array(node, child_path);
    if (rows.size() > CodecVisitor::max_items)
      boundary::fail("CheckpointCollectionTooLarge", child_path);
    std::size_t index = 0;
    return archive.sequence(
        name, value,
        [&](auto &child, auto &row) {
          const auto position = index++;
          return json_value(child, rows[position], row,
                            index_path(child_path, position), selected_version);
        },
        json_scalar<typename IsVector<T>::Value>);
  } else if constexpr (IsOptional<T>::value)
    return archive.optional(
        name, value,
        [&](auto &child, auto &row) {
          return json_value(child, node, row, child_path, selected_version);
        },
        json_scalar<typename IsOptional<T>::Value>);
  else if constexpr (IsTable<T>::value) {
    const auto &rows = boundary::object(node, child_path);
    if (rows.size() > CodecVisitor::max_items)
      boundary::fail("CheckpointCollectionTooLarge", child_path);
    std::size_t index = 0;
    auto read = [&](auto &child, auto &row) {
      const auto position = index++;
      auto result = json_value(child, rows[position].second, row,
                               checkpoint_decode_boundary_join(
                                   {child_path, "/", rows[position].first}),
                               selected_version);
      for (std::size_t prior = 0; prior < position; ++prior)
        if (rows[prior].first == rows[position].first)
          boundary::fail_parts("DuplicateDynamicRecord",
                               {child_path, "/", rows[position].first});
      return result;
    };
    using Row = typename IsTable<T>::Value;
    if constexpr (std::is_same_v<Row, HouseholdPurchasePlan> ||
                  std::is_same_v<Row, HouseholdPurchaseAcceptedTerms> ||
                  std::is_same_v<Row, HouseholdPurchaseServiceResult> ||
                  std::is_same_v<Row, HouseholdPurchaseHandover> ||
                  std::is_same_v<Row, HouseholdPurchasePhaseChange> ||
                  std::is_same_v<Row, HouseholdServiceEvent2> ||
                  std::is_same_v<Row, PublicMarketStaffingOffer> ||
                  std::is_same_v<Row, PublicMarketStaffingConsent> ||
                  std::is_same_v<Row, PublicMarketStaffingEvent> ||
                  std::is_same_v<Row, PublicMarketStaffingReleaseConsent> ||
                  std::is_same_v<Row, HouseholdRetailSettlement>)
      return archive.table(
          name, value, read,
          [](auto &destination, auto &source) -> Status {
            destination.emplace_back(std::move(source));
            return common_success();
          },
          json_scalar<Row>);
    else
      return archive.table(name, value, read, json_scalar<Row>);
  } else
    return archive.object(name, [&](auto &child) {
      return json_value(child, node, value, child_path, selected_version);
    });
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path,Integer selected_version){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path,selected_version));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path,Integer selected_version=2){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path,selected_version));checked(archive.finish());return value;}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template <class T>
Status checkpoint(CodecVisitor &c, std::string_view name, T &value,
                  Integer selected_version) {
  if constexpr (std::is_same_v<T, StableId> || std::is_same_v<T, Integer> ||
                std::is_same_v<T, bool>)
    return c.field(name, value);
  else if constexpr (std::is_same_v<T, std::uint64_t>)
    return c.unsigned_field(name, value);
  else if constexpr (std::is_same_v<T, MobileInventoryAdoptionView>)
    return visit_mobile_inventory_adoption(c, name, value);
  else if constexpr (std::is_same_v<T, Owner>)
    return c.object(name, [&](CodecVisitor &x) {
      return common_checkpoint::owner(x, value);
    });
  else if constexpr (std::is_same_v<T, Location>)
    return c.object(name, [&](CodecVisitor &x) {
      return common_checkpoint::location(x, value);
    });
  else if constexpr (std::is_same_v<T, CanonicalFraction>)
    return c.object(name, [&](CodecVisitor &x) {

      EncodedFraction fields;
      if (!x.reading()) {
        auto encoded = encode_canonical_fraction(value);
        if (!encoded.ok)
          return x.reject("InvalidEconomicExactFraction");
        fields = std::move(encoded.value);
      }
      x.field("numerator", fields.numerator);
      x.field("denominator", fields.denominator);
      if (x.reading() && x.status().ok) {
        auto decoded = decode_canonical_fraction(fields);
        if (!decoded.ok)
          return x.reject("InvalidEconomicExactFraction");
        value = decoded.value;
      }
      return x.status();
    });
  else if constexpr (IsVariant<T>::value && !IsMarketWireVariant<T>::value)
    return c.object(name, [&](auto &x) {

      const auto &names = VariantNames<T>::names();
      StableId kind;
      if (!x.reading())
        kind = names.at(value.index());
      x.field("kind", kind);
      if (!x.status().ok)
        return x.status();
      auto found = std::find(names.begin(), names.end(), kind);
      if (found == names.end())
        return x.reject("UnsupportedHouseholdVariant");
      if (x.reading()) {
        auto selected = select_variant(
            x, value, static_cast<std::size_t>(found - names.begin()));
        if (!selected.ok)
          return selected;
      }
      return std::visit(
          [&](auto &v) {
            Checkpointer fields{x, selected_version};
            record(fields, v);
            return x.status();
          },
          value);
    });
  else if constexpr (IsMarketWireVariant<T>::value)
    return c.object(name, [&](auto &x) {
      Integer tag = x.reading() ? 0 : static_cast<Integer>(value.index());
      x.field("tag", tag);
      if (!x.status().ok)
        return x.status();
      if (tag < 0 || tag >= static_cast<Integer>(std::variant_size_v<T>))
        return x.reject("UnsupportedMarketWireVariant");
      if (x.reading()) {
        auto selected =
            select_market_wire_variant(x, value, static_cast<std::size_t>(tag));
        if (!selected.ok)
          return selected;
      }
      return std::visit(
          [&](auto &v) {
            return checkpoint(x, "payload", v, selected_version);
          },
          value);
    });
  else if constexpr (std::is_enum_v<T>) {

    StableId named;
    for (const auto &p : EnumNames<T>::rows())
      if (p.first == value)
        named = p.second;
    c.field(name, named);
    if (!c.status().ok)
      return c.status();
    for (const auto &p : EnumNames<T>::rows())
      if (p.second == named) {
        if (c.reading())
          value = p.first;
        return c.status();
      }
    return c.reject("InvalidDynamicEnum", name);
  } else if constexpr (IsVector<T>::value)
    return c.sequence(name, value, [&](auto &x, auto &v) {
      return checkpoint(x, "item", v, selected_version);
    });
  else if constexpr (IsOptional<T>::value)
    return c.optional(name, value, [&](auto &x, auto &v) {
      return checkpoint(x, "value", v, selected_version);
    });
  else if constexpr (IsTable<T>::value)
    return common_checkpoint::table(c, name, value, [&](auto &x, auto &v) {
      return checkpoint(x, "record", v, selected_version);
    });
  else
    return c.object(name, [&](CodecVisitor &x) {
      Checkpointer a{x, selected_version};
      record(a, value);
      return x.status();
    });
}
}
namespace hanmo::resident {
BoundaryValue encode_household_purchases(const HouseholdPurchaseState& v,Integer selected_version){if(selected_version!=2&&selected_version!=3)boundary::fail("UnsupportedHouseholdPurchasesVersion","/household_purchases");return household_codec::encode(v,selected_version);}
Outcome<HouseholdPurchaseState> decode_household_purchases(const BoundaryValue& v,Integer selected_version){try{if(selected_version!=2&&selected_version!=3)boundary::fail("UnsupportedHouseholdPurchasesVersion","/household_purchases");auto out=household_codec::decode_root<HouseholdPurchaseState>(v,"/household_purchases",selected_version);return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_household_purchases(CodecVisitor& c,HouseholdPurchaseState& v,Integer selected_version){if(selected_version!=2&&selected_version!=3)return c.reject("UnsupportedHouseholdPurchasesVersion");household_codec::Checkpointer fields{c,selected_version};household_codec::record(fields,v);return c.status();}
Status emit_household_purchases(ProjectionWriter& w,const HouseholdPurchaseState& v,Integer selected_version){if(selected_version!=2&&selected_version!=3)return w.reject("UnsupportedHouseholdPurchasesVersion");return household_codec::project(w,"",v,selected_version);}
BoundaryValue encode_household_purchase_catalog(const HouseholdPurchaseCatalog& v){return household_codec::encode(v);}
Outcome<HouseholdPurchaseCatalog> decode_household_purchase_catalog(const BoundaryValue& v){try{auto out=household_codec::decode_root<HouseholdPurchaseCatalog>(v,"/household_purchase_catalog");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_household_purchase_catalog(CodecVisitor& c,HouseholdPurchaseCatalog& v){household_codec::Checkpointer fields{c};household_codec::record(fields,v);return c.status();}
Status emit_household_purchase_catalog(ProjectionWriter& w,const HouseholdPurchaseCatalog& v){return household_codec::project(w,"",v);}
BoundaryValue encode_household_retail_state(const HouseholdRetailState& v){return household_codec::encode(v);}
Outcome<HouseholdRetailState> decode_household_retail_state(const BoundaryValue& v){try{auto out=household_codec::decode_root<HouseholdRetailState>(v,"/household_retail_state");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_household_retail_checkpoint(CodecVisitor& c,HouseholdRetailState& v){household_codec::Checkpointer fields{c};household_codec::record(fields,v);return c.status();}
Status emit_household_retail_state(ProjectionWriter& w,const HouseholdRetailState& v){return household_codec::project(w,"",v);}
BoundaryValue encode_household_retail_catalog(const HouseholdRetailCatalog& v){return household_codec::encode(v);}
Outcome<HouseholdRetailCatalog> decode_household_retail_catalog(const BoundaryValue& v){try{auto out=household_codec::decode_root<HouseholdRetailCatalog>(v,"/household_retail_catalog");return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_household_retail_catalog(CodecVisitor& c,HouseholdRetailCatalog& v){household_codec::Checkpointer fields{c};household_codec::record(fields,v);return c.status();}
Status emit_household_retail_catalog(ProjectionWriter& w,const HouseholdRetailCatalog& v){return household_codec::project(w,"",v);}
}
