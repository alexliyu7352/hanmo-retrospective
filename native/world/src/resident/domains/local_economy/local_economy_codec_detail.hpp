// 冷边界和二进制检查点共享逐字段访问；所有枚举显式命名，未知字段、类型和枚举立即拒绝。
#pragma once
#include "local_economy_model.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <string_view>
#include <type_traits>
namespace hanmo::resident::dynamic_economy_codec {
template<class T> struct EnumNames;
template<class T> struct IsVector : std::false_type {};
template<class T> struct IsVector<std::vector<T>> : std::true_type { using Value=T; };
template<class T> struct IsOptional : std::false_type {};
template<class T> struct IsOptional<std::optional<T>> : std::true_type { using Value=T; };
template<class T> struct IsTable : std::false_type {};
template<class T> struct IsTable<OrderedTable<T>> : std::true_type { using Value=T; };
template<> struct EnumNames<SubjectKind> { inline static constexpr std::pair<SubjectKind,const char*> names[]={{SubjectKind::actor,"actor"},{SubjectKind::organization,"organization"},{SubjectKind::cohort,"cohort"}};static constexpr const auto& rows(){return names;} };
template<> struct EnumNames<ScopeRef::Kind> { inline static constexpr std::pair<ScopeRef::Kind,const char*> names[]={{ScopeRef::Kind::site,"site"},{ScopeRef::Kind::region,"region"},{ScopeRef::Kind::force,"force"},{ScopeRef::Kind::account,"account"},{ScopeRef::Kind::activity,"activity"}};static constexpr const auto& rows(){return names;} };
template<> struct EnumNames<PopulationStatus> { inline static constexpr std::pair<PopulationStatus,const char*> names[]={{PopulationStatus::civilian,"civilian"},{PopulationStatus::training,"training"},{PopulationStatus::trained_reserve,"trained_reserve"},{PopulationStatus::serving,"serving"},{PopulationStatus::wounded,"wounded"},{PopulationStatus::captive,"captive"},{PopulationStatus::displaced,"displaced"}};static constexpr const auto& rows(){return names;} };
template<> struct EnumNames<ObligationKind> { inline static constexpr std::pair<ObligationKind,const char*> names[]={{ObligationKind::ration,"ration"},{ObligationKind::wage,"wage"},{ObligationKind::contract_payment,"contract_payment"}};static constexpr const auto& rows(){return names;} };
template<> struct EnumNames<ProvisionOwner> { inline static constexpr std::pair<ProvisionOwner,const char*> names[]={{ProvisionOwner::macro,"macro"},{ProvisionOwner::estate,"estate"},{ProvisionOwner::military,"military"}};static constexpr const auto& rows(){return names;} };
// 遍历 SubjectRef 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,SourceRef& v) { a("domain",v.domain);a("kind",v.kind);a("id",v.id); }
template<class A> void record(A& a,NamedBirthPopulationTransition& v) { a("id",v.id);a("birth_fact_ref",v.birth_fact_ref);a("child_actor_id",v.child_actor_id);a("occurred_at",v.occurred_at); }
template<class A> void record(A& a,SubjectRef& v) { a("kind",v.kind);a("id",v.id); }
// 遍历 ScopeRef 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ScopeRef& v) { a("kind",v.kind);a("id",v.id); }
// 遍历 HouseholdGroup 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,HouseholdGroup& v) { a("id",v.id);a("wallet_id",v.wallet_id);a("inventory_id",v.inventory_id); }
// 遍历 PopulationCohort 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,PopulationCohort& v) { a("id",v.id);a("site_id",v.site_id);a("household_group_id",v.household_group_id);a("status",v.status);a("headcount",v.headcount);a("labor_capacity",v.labor_capacity);a("private_wallet_id",v.private_wallet_id);a("private_inventory_id",v.private_inventory_id);a("affiliation_id",v.affiliation_id);a("assignment_id",v.assignment_id); }
// 遍历 LaborAllocation 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,LaborAllocation& v) { a("id",v.id);a("cohort_id",v.cohort_id);a("activity_or_production_id",v.activity_or_production_id);a("workers",v.workers);a("from_tick",v.from_tick);a("through_tick",v.through_tick);a("commitment_id",v.commitment_id); }
// 遍历 PopulationTransfer 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,PopulationTransfer& v) { a("id",v.id);a("from_cohort_id",v.from_cohort_id);a("to_cohort_id",v.to_cohort_id);a("count",v.count);a("labor_eligible_count",v.labor_eligible_count);a("reason",v.reason);a("source_fact_id",v.source_fact_id);a("effective_tick",v.effective_tick); }
// 军事人口死亡保留具体成员和真实伤亡来源，仅进入显式 v2 布局。
template<class A> void record(A& a,PopulationLoss& v) { a("id",v.id);a("cohort_id",v.cohort_id);a("source_fact_id",v.source_fact_id);a("casualty_fact_id",v.casualty_fact_id);a("count",v.count);a("labor_eligible_count",v.labor_eligible_count);a("effective_tick",v.effective_tick);a("member_ids",v.member_ids); }
// 遍历 RecipeQuantity 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,RecipeQuantity& v) { a("item_id",v.item_id);a("numerator",v.numerator);a("denominator",v.denominator); }
// 遍历 ProductionRecipe 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ProductionRecipe& v) { a("id",v.id);a("period_ticks",v.period_ticks);a("labor_per_batch",v.labor_per_batch);a("region_factor_bp",v.region_factor_bp);a("season_factor_bp",v.season_factor_bp);a("inputs",v.inputs);a("outputs",v.outputs); }
// 遍历 ProductionUnit 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ProductionUnit& v) { a("id",v.id);a("site_id",v.site_id);a("owner",v.owner);a("input_inventory_ids",v.input_inventory_ids);a("output_inventory_id",v.output_inventory_id);a("labor_allocation_ids",v.labor_allocation_ids);a("capacity",v.capacity);a("recipe_id",v.recipe_id);a("next_due_tick",v.next_due_tick);a("last_settlement_id",v.last_settlement_id);a("labor_remainder",v.labor_remainder);a("factor_remainder",v.factor_remainder);a("input_remainders",v.input_remainders);a("output_remainders",v.output_remainders); }
// 遍历 RecurringObligation 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,RecurringObligation& v) { a("id",v.id);a("principal",v.principal);a("beneficiary",v.beneficiary);a("kind",v.kind);a("source_wallet_or_inventory",v.source_wallet_or_inventory);a("destination",v.destination);a("rate_profile",v.rate_profile);a("authority_ref",v.authority_ref);a("item_id",v.item_id);a("period_ticks",v.period_ticks);a("next_due_tick",v.next_due_tick);a("last_receipt_id",v.last_receipt_id);a("active",v.active); }
// 遍历 ObligationRate 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ObligationRate& v) { a("id",v.id);a("numerator",v.numerator);a("denominator",v.denominator);a("per_capita",v.per_capita); }
// 遍历 ArrearRecord 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ArrearRecord& v) { a("id",v.id);a("obligation_id",v.obligation_id);a("due_tick",v.due_tick);a("original_due",v.original_due);a("paid_amount",v.paid_amount); }
// 遍历 ProvisionBinding 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ProvisionBinding& v) { a("id",v.id);a("subject",v.subject);a("from_tick",v.from_tick);a("through_tick",v.through_tick);a("payer_inventory",v.payer_inventory);a("obligation_id",v.obligation_id);a("policy_owner",v.policy_owner);if(a.military_adapter)a("military_source_id",v.military_source_id); }
// 遍历 ProductionBinding 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ProductionBinding& v) { a("unit_id",v.unit_id);a("policy_owner",v.policy_owner); }
// 遍历 PaymentReceipt 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,PaymentReceipt& v) { a("id",v.id);a("arrear_id",v.arrear_id);a("obligation_id",v.obligation_id);a("from_wallet_id",v.from_wallet_id);a("to_wallet_id",v.to_wallet_id);a("tick",v.tick);a("due_tick",v.due_tick);a("amount",v.amount);a("authority_ref",v.authority_ref); }
// 遍历 RationReceipt 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,RationReceipt& v) { a("id",v.id);a("obligation_id",v.obligation_id);a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("beneficiary",v.beneficiary);a("tick",v.tick);a("due_tick",v.due_tick);a("required",v.required);a("consumed",v.consumed);a("shortage",v.shortage); }
// 遍历 ProductionQuantity 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ProductionQuantity& v) { a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("quantity",v.quantity); }
// 遍历 ProductionReceipt 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,ProductionReceipt& v) { a("id",v.id);a("unit_id",v.unit_id);a("recipe_id",v.recipe_id);a("tick",v.tick);a("due_tick",v.due_tick);a("allocated_workers",v.allocated_workers);a("effective_workers",v.effective_workers);a("batches",v.batches);a("inputs",v.inputs);a("outputs",v.outputs);a("unrealized_output",v.unrealized_output);a("cause_fact_ids",v.cause_fact_ids);if(a.production_management)a("production_context",v.production_context); }
// 遍历 LocalSupplySource 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,LocalSupplySource& v) { a("source_key",v.source_key);a("organization_id",v.organization_id);a("site_id",v.site_id);a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("target_amount",v.target_amount);a("public_report",v.public_report); }
// 遍历 LocalEconomyCatalog 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,LocalEconomyCatalog& v) { a("enabled",v.enabled);a("households",v.households);a("cohorts",v.cohorts);a("labor_allocations",v.labor_allocations);a("recipes",v.recipes);a("production_units",v.production_units);a("rates",v.rates);a("obligations",v.obligations);a("provision_bindings",v.provision_bindings);a("production_bindings",v.production_bindings);a("supply_sources",v.supply_sources);a("named_actors_excluded",v.named_actors_excluded);if(a.production_management)a("production_management",v.production_management);if(a.household_provisioning)a("household_provisioning",v.household_provisioning); }
// 遍历 LocalEconomyState 的完整持久字段；相同定义供目录、冷状态与检查点使用。
template<class A> void record(A& a,LocalSupplyRegistration& v) { a("id",v.id);a("source_key",v.source_key);a("organization_id",v.organization_id);a("site_id",v.site_id);a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("tick",v.tick); }
// 供应快照是观察当时的不可变历史值，库存仍以共同容器为唯一真值。
template<class A> void record(A& a,SupplyObservation& v) {a("source_key",v.source_key);a("organization_id",v.organization_id);a("site_id",v.site_id);a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("source_fact_id",v.source_fact_id);a("target_amount",v.target_amount);a("physical_amount",v.physical_amount);a("source_revision",v.source_revision);}
template<class A> void record(A& a,LocalSupplyObservation& v) {a("id",v.id);a("observation",v.observation);a("tick",v.tick);a("publicly_visible",v.publicly_visible);}
template<class A> void record(A& a,LocalEconomyState& v) { a("last_step_tick",v.last_step_tick);a("households",v.households);a("cohorts",v.cohorts);a("labor_allocations",v.labor_allocations);a("production_units",v.production_units);a("obligations",v.obligations);a("provision_bindings",v.provision_bindings);a("production_bindings",v.production_bindings);a("arrears",v.arrears);a("payments",v.payments);a("rations",v.rations);a("production_receipts",v.production_receipts);a("population_transfers",v.population_transfers);a("obligation_remainders",v.obligation_remainders);a("supply_registrations",v.supply_registrations);a("supply_observations",v.supply_observations);if(a.military_adapter)a("population_losses",v.population_losses);if(a.named_membership){a("membership_wage_bindings",v.membership_wage_bindings);a("membership_terminal_fractions",v.membership_terminal_fractions);}if(a.production_management)a("production_management",v.production_management);if(a.household_provisioning)a("household_provisioning",v.household_provisioning);if(a.born_population)a("named_birth_population_transitions",v.named_birth_population_transitions); }

// MembershipWageBinding逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,MembershipWageBinding& v) { a("id",v.id);a("member_episode_id",v.member_episode_id);a("obligation_id",v.obligation_id);a("offer_id",v.offer_id);a("decision_id",v.decision_id);a("terms_id",v.terms_id);a("organization_id",v.organization_id);a("actor_id",v.actor_id);a("payer_wallet_id",v.payer_wallet_id);a("payee_wallet_id",v.payee_wallet_id);a("grant_id",v.grant_id);a("office_id",v.office_id);a("issuer_actor_id",v.issuer_actor_id);a("authority_epoch",v.authority_epoch);a("joined_tick",v.joined_tick);a("first_due_tick",v.first_due_tick);a("credited_periods",v.credited_periods);a("last_credited_due_tick",v.last_credited_due_tick);a("frozen_numerator",v.frozen_numerator);a("frozen_denominator",v.frozen_denominator);a("period_ticks",v.period_ticks);a("amount_limit",v.amount_limit);a("terminal_fraction_policy_id",v.terminal_fraction_policy_id);a("ended_tick",v.ended_tick);a("terminal_fact_id",v.terminal_fact_id); }
// MembershipTerminalFractionReceipt逐字段严格保存来源；缺字段或未知字段都拒绝。
template<class A> void record(A& a,MembershipTerminalFractionReceipt& v) { a("id",v.id);a("wage_binding_id",v.wage_binding_id);a("member_episode_id",v.member_episode_id);a("obligation_id",v.obligation_id);a("departure_fact_id",v.departure_fact_id);a("arrear_id",v.arrear_id);a("tick",v.tick);a("original_numerator",v.original_numerator);a("denominator",v.denominator);a("compensation_numerator",v.compensation_numerator);a("integer_due",v.integer_due); }


// 生产管理仅在显式v1能力访问；不把未知字段带回旧布局。
template<> struct EnumNames<ProductionAssetKind> { inline static constexpr std::pair<ProductionAssetKind,const char*> names[]={{ProductionAssetKind::land,"land"},{ProductionAssetKind::facility,"facility"}};static constexpr const auto& rows(){return names;} };
template<class A> void record(A& a,ActingAuthority& v){a("organization_id",v.organization_id);a("office_id",v.office_id);a("authority_epoch",v.authority_epoch);a("actor_id",v.actor_id);}
template<class A> void record(A& a,ProductionAsset& v){a("id",v.id);a("site_id",v.site_id);a("source_key",v.source_key);a("owner",v.owner);a("kind",v.kind);a("capacity_units",v.capacity_units);}
template<class A> void record(A& a,ProductionAssetUse& v){a("asset_id",v.asset_id);a("units",v.units);a("batches_per_unit",v.batches_per_unit);}
template<class A> void record(A& a,ProductionSeasonProfile& v){a("id",v.id);a("origin_tick",v.origin_tick);a("phase_ticks",v.phase_ticks);a("factors_bp",v.factors_bp);}
template<class A> void record(A& a,ProductionCapacityPlan& v){a("unit_id",v.unit_id);a("season_profile_id",v.season_profile_id);a("asset_uses",v.asset_uses);}
template<class A> void record(A& a,ProductionManagementCatalog& v){a("version",v.version);a("assets",v.assets);a("seasons",v.seasons);a("plans",v.plans);}
template<class A> void record(A& a,ProductionContext& v){a("capacity",v.capacity);a("region_factor_bp",v.region_factor_bp);a("season_factor_bp",v.season_factor_bp);a("season_phase",v.season_phase);a("season_profile_id",v.season_profile_id);a("asset_ids",v.asset_ids);}
template<class A> void record(A& a,ProductionLaborTarget& v){a("allocation_id",v.allocation_id);a("workers",v.workers);}
template<class A> void record(A& a,ProductionConfigurationRecord& v){a("id",v.id);a("unit_id",v.unit_id);a("actor_id",v.actor_id);a("requested_tick",v.requested_tick);a("effective_tick",v.effective_tick);a("first_due_tick",v.first_due_tick);a("authority",v.authority);a("targets",v.targets);a("prior_workers",v.prior_workers);a("reservation_allocation_ids",v.reservation_allocation_ids);}
template<class A> void record(A& a,ProductionConfigurationResult& v){a("id",v.id);a("configuration_id",v.configuration_id);a("kind",v.kind);a("tick",v.tick);}
template<class A> void record(A& a,ProductionManagementState& v){a("version",v.version);a("configurations",v.configurations);a("results",v.results);a("pending_by_unit",v.pending_by_unit);}

// 家庭自养逐字段保留已发生来源与期限终态；不包含重复口粮账。
template<class A> void record(A& a,HouseholdProvisionAuthorization& v){a("id",v.id);a("household_group_id",v.household_group_id);a("cohort_id",v.cohort_id);a("obligation_id",v.obligation_id);a("provision_binding_id",v.provision_binding_id);a("inventory_id",v.inventory_id);a("item_id",v.item_id);a("rate_profile_id",v.rate_profile_id);}
template<class A> void record(A& a,HouseholdProvisionCatalog& v){a("version",v.version);a("authorizations",v.authorizations);}
template<class A> void record(A& a,HouseholdProvisionRegistration& v){a("id",v.id);a("authorization_id",v.authorization_id);a("tick",v.tick);}
template<class A> void record(A& a,HouseholdProvisionClosure& v){a("id",v.id);a("registration_id",v.registration_id);a("obligation_id",v.obligation_id);a("tick",v.tick);}
template<class A> void record(A& a,HouseholdProvisionState& v){a("version",v.version);a("registrations",v.registrations);a("closures",v.closures);}

// 编码仅构造临时边界对象，运行状态不持有 BoundaryValue。
template<class T> BoundaryValue encode(const T& value,bool military_adapter=false,bool named_membership=false,bool production_management=false,bool household_provisioning=false,bool born_population=false);
template<class T> void decode(const BoundaryValue&,T&,std::string_view,bool military_adapter=false,bool named_membership=false,bool production_management=false,bool household_provisioning=false,bool born_population=false);
template<class T> Status checkpoint(CodecVisitor&,std::string_view,T&,bool military_adapter=false,bool named_membership=false,bool production_management=false,bool household_provisioning=false,bool born_population=false);
template<class T> Status project(ProjectionWriter&,std::string_view,const T&,bool military_adapter=false,bool named_membership=false,bool production_management=false,bool household_provisioning=false,bool born_population=false);
struct Projector {
    ProjectionWriter& writer;bool military_adapter=false;bool named_membership=false;bool production_management=false;bool household_provisioning=false;bool born_population=false;
    template<class T> void operator()(const char* key,const T&v){project(writer,key,v,military_adapter,named_membership,production_management,household_provisioning,born_population);}
    template<class T>void authored_optional(const char* key,const std::optional<T>& value){(*this)(key,value);}
};
template<class T>Status emit_fields(ProjectionWriter&w,const T&v,bool military_adapter=false,bool named_membership=false,bool production_management=false,bool household_provisioning=false,bool born_population=false){Projector fields{w,military_adapter,named_membership,production_management,household_provisioning,born_population};record(fields,const_cast<T&>(v));return w.status();}
template<class T>inline constexpr bool json_scalar=std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_enum_v<T>;
template<class T>Status json_value(ActorFoundationJsonVisitor&,const BoundaryValue&,T&,std::string_view,bool,bool,bool,bool,bool);
template<class T>Status json_field(ActorFoundationJsonVisitor&,const BoundaryValue&,std::string_view,T&,std::string_view,bool,bool,bool,bool,bool);
inline Status checked(Status value){if(!value.ok)throw boundary::DecodeFailure{value.error};return value;}
inline StableId index_path(std::string_view path,std::size_t index){char digits[3*sizeof(index)+1];auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+index%10);index/=10;}while(index);return checkpoint_decode_boundary_join({path,"/",std::string_view(start,std::size_t(end-start))});}
struct Decoder {
    ActorFoundationJsonVisitor& archive;const BoundaryValue& input;std::string_view path;
    bool military_adapter=false,named_membership=false,production_management=false,household_provisioning=false,born_population=false;
    template<class T>void operator()(const char* key,T& value){checked(json_field(archive,input,key,value,path,military_adapter,named_membership,production_management,household_provisioning,born_population));}
    template<class T>void authored_optional(const char* key,std::optional<T>& value){if(!input.find(key)){value.reset();return;}(*this)(key,value);}
    void finish(){for(const auto& row:boundary::object(input,path))if(!archive.has_visited_field(row.first))boundary::fail_parts("UnsupportedResidentField",{path,"/",row.first});checked(archive.finish());}
};
struct Checkpointer {
    CodecVisitor& codec;bool military_adapter=false;bool named_membership=false;bool production_management=false;bool household_provisioning=false;bool born_population=false;
    template<class T> void operator()(const char* key,T& v) { checkpoint(codec,key,v,military_adapter,named_membership,production_management,household_provisioning,born_population); }
    template<class T>void authored_optional(const char* key,std::optional<T>& value){(*this)(key,value);}
};
// Only the new exact-valued row uses this fallible transfer. Numeric copies
// remain tracked; failure discards the whole unpublished decode, never retries a row.
template<class Archive>Status born_population_moment(Archive& archive,WorldMoment& moment){
 try{return exact_fields::moment(archive,moment);}
 catch(exact_fields::CodecFailure& error){return archive.reject(error.error.original.code,error.error.original.path);}
}
struct NamedBirthPopulationDecodedRowTransfer {
 template<class Destination>Status operator()(Destination& destination,std::pair<StableId,NamedBirthPopulationTransition>& row)const{
  destination.emplace_back(std::move(row));return common_success();
 }
};
// 缺失字段不补跑初始化，重复表键不覆盖既有记录。
template<class T>Status project(ProjectionWriter&w,std::string_view name,const T&value,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning,bool born_population){
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return w.field(name,value);
    else if constexpr(std::is_same_v<T,Owner>)return emit_common_owner(w,name,value);
    else if constexpr(std::is_same_v<T,WorldMoment>)return emit_projection_record<ActorFoundationProjectionArchive>(w,name,value,[](auto& archive,auto& moment){return born_population_moment(archive,moment);});
    else if constexpr(std::is_enum_v<T>){for(const auto&p:EnumNames<T>::rows())if(p.first==value)return w.field(name,StableId{p.second});return w.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return w.sequence(name,value,[&](auto&x,const auto&v){return project(x,"",v,military_adapter,named_membership,production_management,household_provisioning,born_population);});
    else if constexpr(IsOptional<T>::value){if(value)return project(w,name,*value,military_adapter,named_membership,production_management,household_provisioning,born_population);return w.null(name);}
    else if constexpr(IsTable<T>::value)return w.keyed_dictionary(name,{},[&](auto&x){for(const auto&p:value.rows()){project(x,p.first,p.second,military_adapter,named_membership,production_management,household_provisioning,born_population);if(!x.status().ok)break;}return x.status();});
    else return w.object(name,[&](auto&x){return emit_fields(x,value,military_adapter,named_membership,production_management,household_provisioning,born_population);});
}
template<class T> BoundaryValue encode(const T&value,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning,bool born_population){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=project(writer,"",value,military_adapter,named_membership,production_management,household_provisioning,born_population);if(status.ok)status=writer.finish();if(!status.ok)throw boundary::DecodeFailure{status.error};return out;}
// This schema adapter owns only original field names, enum spelling and error
// precedence. The canonical archive owns every text/vector/optional/table allocation.
template<class T>Status json_value(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,T& value,std::string_view path,bool military,bool named,bool management,bool household,bool born_population){
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(input,{path});return archive.field("",value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(input,path);return archive.field("",value);}
 else if constexpr(std::is_same_v<T,WorldMoment>)return born_population_moment(archive,value);
  else if constexpr(std::is_same_v<T,Owner>){
  auto original=[&](const BoundaryValue& node)->Outcome<T>{try{return {true,boundary::decode_owner(node,checkpoint_decode_boundary_text(path)),{}};}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}};
  auto emit_original=[](ProjectionWriter& writer,const T& row){return emit_common_owner(writer,"",row);};
  if(ScopedCheckpointJsonDecode::current())return archive.foreign_record(value,[&](const BoundaryValue& node,ScopedCheckpointJsonDecode& budget)->Outcome<T>{if(&budget!=ScopedCheckpointJsonDecode::current())budget.fail("CheckpointCandidateMemoryJobMismatch","local_economy_original");budget.template default_values<T>();return original(node);},emit_original);
  return archive.foreign_record(value,original,emit_original);
 }
 else if constexpr(std::is_enum_v<T>){StableId name;boundary::string_ref(input,{path});checked(archive.field("",name));for(const auto& row:EnumNames<T>::rows())if(name==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",path);}
 else if constexpr(IsOptional<T>::value)return archive.root_optional(value,path,[&](auto& child,auto& row){return json_value(child,input,row,path,military,named,management,household,born_population);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(input,path);std::size_t index=0;return archive.root_sequence(value,path,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(path,position),military,named,management,household,born_population);},json_scalar<typename IsVector<T>::Value>);}
 else{checked(archive.defer_duplicate_field_check_to_finish());Decoder fields{archive,input,path,military,named,management,household,born_population};record(fields,value);fields.finish();return archive.status();}
}
template<class T>Status json_field(ActorFoundationJsonVisitor& archive,const BoundaryValue& input,std::string_view name,T& value,std::string_view path,bool military,bool named,bool management,bool household,bool born_population){
 const auto& node=boundary::field(input,name,path);const auto child_path=checkpoint_decode_boundary_join({path,"/",name});
 if constexpr(std::is_same_v<T,StableId>){boundary::string_ref(node,{child_path});return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,Integer>){boundary::integer(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_same_v<T,bool>){boundary::boolean(node,child_path);return archive.field(name,value);}
 else if constexpr(std::is_enum_v<T>){StableId text;boundary::string_ref(node,{child_path});checked(archive.field(name,text));for(const auto& row:EnumNames<T>::rows())if(text==row.second){value=row.first;return archive.status();}boundary::fail("InvalidDynamicEnum",child_path);}
 else if constexpr(IsVector<T>::value){const auto& rows=boundary::array(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;return archive.sequence(name,value,[&](auto& child,auto& row){const auto position=index++;return json_value(child,rows[position],row,index_path(child_path,position),military,named,management,household,born_population);},json_scalar<typename IsVector<T>::Value>);}
 else if constexpr(IsOptional<T>::value)return archive.optional(name,value,[&](auto& child,auto& row){return json_value(child,node,row,child_path,military,named,management,household,born_population);},json_scalar<typename IsOptional<T>::Value>);
 else if constexpr(IsTable<T>::value){const auto& rows=boundary::object(node,child_path);if(rows.size()>CodecVisitor::max_items)boundary::fail("CheckpointCollectionTooLarge",child_path);std::size_t index=0;auto read=[&](auto& child,auto& row){const auto position=index++;auto result=json_value(child,rows[position].second,row,checkpoint_decode_boundary_join({child_path,"/",rows[position].first}),military,named,management,household,born_population);for(std::size_t prior=0;prior<position;++prior)if(rows[prior].first==rows[position].first)boundary::fail_parts("DuplicateDynamicRecord",{child_path,"/",rows[position].first});return result;};if constexpr(std::is_same_v<typename IsTable<T>::Value,NamedBirthPopulationTransition>)return archive.table(name,value,read,NamedBirthPopulationDecodedRowTransfer{});else return archive.table(name,value,read,json_scalar<typename IsTable<T>::Value>);}
 else return archive.object(name,[&](auto& child){return json_value(child,node,value,child_path,military,named,management,household,born_population);});
}
template<class T>void decode(const BoundaryValue& input,T& value,std::string_view path,bool military,bool named,bool management,bool household,bool born_population){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(json_value(archive,input,value,path,military,named,management,household,born_population));checked(archive.finish());}
template<class T>T decode_root(const BoundaryValue& input,std::string_view path,bool military=false,bool named=false,bool management=false,bool household=false,bool born_population=false){ActorFoundationJsonVisitor archive(input,json_scalar<T>);checked(archive.template prepare_default_values<T>());T value;checked(json_value(archive,input,value,path,military,named,management,household,born_population));checked(archive.finish());return value;}
// 字符串枚举固定协议语义，恢复时不接受 C++ ordinal 巧合。
template<class T> Status checkpoint(CodecVisitor& c,std::string_view name,T& value,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning,bool born_population) {
    if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);
    else if constexpr(std::is_same_v<T,Owner>)return c.object(name,[&](CodecVisitor& x){return common_checkpoint::owner(x,value);});
    else if constexpr(std::is_same_v<T,WorldMoment>)return c.object(name,[&](auto& x){return born_population_moment(x,value);});
    else if constexpr(std::is_enum_v<T>) {StableId named;for(const auto& p:EnumNames<T>::rows())if(p.first==value)named=p.second;c.field(name,named);if(!c.status().ok)return c.status();for(const auto& p:EnumNames<T>::rows())if(p.second==named){if(c.reading())value=p.first;return c.status();}return c.reject("InvalidDynamicEnum",name);}
    else if constexpr(IsVector<T>::value)return c.sequence(name,value,[&](auto& x,auto& v){return checkpoint(x,"item",v,military_adapter,named_membership,production_management,household_provisioning,born_population);});
    else if constexpr(IsOptional<T>::value)return c.optional(name,value,[&](auto& x,auto& v){return checkpoint(x,"value",v,military_adapter,named_membership,production_management,household_provisioning,born_population);});
    else if constexpr(IsTable<T>::value){auto visit=[&](auto& x,auto& v){return checkpoint(x,"record",v,military_adapter,named_membership,production_management,household_provisioning,born_population);};if constexpr(std::is_same_v<typename IsTable<T>::Value,NamedBirthPopulationTransition>)return common_checkpoint::table(c,name,value,visit,NamedBirthPopulationDecodedRowTransfer{});else return common_checkpoint::table(c,name,value,visit);}
    else return c.object(name,[&](CodecVisitor& x){Checkpointer a{x,military_adapter,named_membership,production_management,household_provisioning,born_population};record(a,value);return x.status();});
}
} // 私有访问器不承担规则结算，任何错误只影响未发布的冷候选。
