// 经济明细只投影本人或有权查阅的当前地方结果；不承担公开消息传播或资产写入。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "../local_economy_model.hpp"
#include <variant>
#include <map>
#include <tuple>
namespace hanmo::resident {
struct LocalProductionLaborView { StableId allocation_id,cohort_id;Integer workers=0,maximum_workers=0; };
struct LocalProductionView {
    StableId unit_id,site_id,recipe_id,output_inventory_id,last_receipt_id;
    Owner owner;
    std::optional<Owner> execution_owner;
    StableId configuration_block_code;
    Integer capacity=0,next_due_tick=0,allocated_workers=0;
    std::optional<ProductionReceipt> last_result;
    std::optional<ProductionContext> context;
    std::optional<ProductionConfigurationRecord> pending_configuration;
    std::optional<ActingAuthority> configuration_authority;
    std::vector<LocalProductionLaborView> labor;
    bool may_configure=false,history_available=false;
};
struct LocalPopulationView {
    StableId cohort_id,site_id,household_group_id;
    PopulationStatus status=PopulationStatus::civilian;
    Integer headcount=0,labor_capacity=0;
};
// 原欠据页面只附派生责任身份，不重复保存或对外计算另一份工资金额。
struct CohortWageNeedReference { StableId need_id,source_fact_id,status;Integer revision=0; };
struct LocalArrearView {
    StableId arrear_id,obligation_id,principal_id,beneficiary_id;
    ObligationKind kind=ObligationKind::wage;
    Integer due_tick=0,original_due=0,paid_amount=0,outstanding=0;
    bool obligation_active=false;
    std::optional<CohortWageNeedReference> wage_need;
    SubjectKind beneficiary_kind=SubjectKind::actor;
};
struct LocalRationView { RationReceipt result; };
using LocalEconomyRow=std::variant<LocalProductionView,LocalPopulationView,LocalArrearView,LocalRationView>;
struct LocalEconomyCursor {
    WorldIdentity identity;
    StableId observer_actor_id,place_id;
    std::optional<ActingAuthority> authority;
    Integer offset=0;
};
struct LocalEconomyQuery {
    StableId observer_actor_id,place_id;
    std::optional<ActingAuthority> authority;
    std::optional<LocalEconomyCursor> cursor;
};
struct LocalEconomySource { enum class Kind { production,population,arrear,ration };Kind kind=Kind::production;StableId id; };
using LocalEconomyAuthorityKey=std::tuple<StableId,StableId,Integer,StableId>;
struct OrganizationState;
struct EconomicTakeoverReadContext;
// 候选边界只保存已授权来源的ID，不复制金额或持久化世界真值。
struct LocalEconomyIndex {
    CheckpointMemoryCharge text_memory; // 先于字段构造，后于所有字段销毁。
    LocalEconomyIndex()=default;LocalEconomyIndex(const LocalEconomyIndex&)=default;LocalEconomyIndex(LocalEconomyIndex&&)noexcept=default;
    LocalEconomyIndex& operator=(LocalEconomyIndex value)noexcept{this->~LocalEconomyIndex();new(this)LocalEconomyIndex(std::move(value));return *this;}
    WorldIdentity identity;StableId observer_actor_id,place_id;
    CheckpointVector<LocalEconomySource> personal_sources;
    CheckpointMap<LocalEconomyAuthorityKey,CheckpointVector<LocalEconomySource>> authority_sources;
    CheckpointVector<ActingAuthority> authorities;
    CheckpointMap<StableId,CohortWageNeedReference> cohort_wage_need_by_arrear;
    // Only lawful, continuously owned production sources. Derived IDs, never a second receipt ledger.
    CheckpointMap<StableId,CheckpointVector<StableId>> production_receipts_by_unit;
    // 仅当前明确获准经营者的派生身份；原产权与历史知情不据此改写。
    CheckpointMap<StableId,Owner> production_execution_owners;
    CheckpointMap<StableId,StableId> production_configuration_blocks;
    // 只关联已知单元当前原因和原合法历史收据，不扩大分页候选序列。
    CheckpointMap<StableId,CheckpointVector<StableId>> production_disruption_causes_by_unit;
    CheckpointMap<StableId,StableId> production_disruption_by_receipt;
};
LocalEconomyIndex build_local_economy_index(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationState&,const OrganizationRead&);
LocalEconomyIndex build_local_economy_index(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationState&,const OrganizationRead&,const EconomicTakeoverReadContext*);
struct LocalEconomyPage {
    WorldIdentity identity;
    StableId observer_actor_id,place_id;
    Integer cursor=0,scanned=0;
    std::optional<LocalEconomyCursor> next_cursor;
    std::vector<LocalEconomyRow> rows;
    std::vector<ActingAuthority> authorities;
};
inline constexpr Integer local_economy_page_limit=32,local_economy_scan_limit=128;
Outcome<LocalEconomyPage> query_local_economy(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const LocalEconomyIndex&,const LocalEconomyQuery&);
BoundaryValue encode_local_economy_page(const LocalEconomyPage&);
Outcome<LocalEconomyCursor> decode_local_economy_cursor(const BoundaryValue&);
// 观察者只能由当前世界调用方提供，参数中任何actor覆盖字段都拒绝。
Outcome<LocalEconomyQuery> decode_local_economy_query(const BoundaryValue&,const StableId& observer_actor_id);
}
