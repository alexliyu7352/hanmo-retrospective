// 财政明细只读唯一税债和实际实付；合法源序列在候选边界派生，不按查询扫描后过滤全世界。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/views/local_economy_view.hpp"
namespace hanmo::resident {
// 只保存同源事项身份与状态；数量仍读取该行原财政欠据，不复制付款余额。
struct FiscalNeedReference {StableId need_id,source_fact_id,status;Integer revision=0;};
struct FiscalAssessmentRow {
    StableId assessment_id,period_id,source_id,site_id,jurisdiction_id,organization_id,grain_item_id,policy_fact_id;
    Integer due_tick=0,cash_due=0,cash_paid=0,cash_outstanding=0,grain_due=0,grain_paid=0,grain_outstanding=0;
    Integer produced_grain=0,civilian_grain_reserve=0;
    FiscalTaxMode mode=FiscalTaxMode::normal;
    std::optional<FiscalNeedReference> cash_need,grain_need;
};
struct FiscalSettlementIndex {
    CheckpointMemoryCharge text_memory; // 先于字段构造，后于所有字段销毁。
    FiscalSettlementIndex()=default;FiscalSettlementIndex(const FiscalSettlementIndex&)=default;FiscalSettlementIndex(FiscalSettlementIndex&&)noexcept=default;
    FiscalSettlementIndex& operator=(FiscalSettlementIndex value)noexcept{this->~FiscalSettlementIndex();new(this)FiscalSettlementIndex(std::move(value));return *this;}
    WorldIdentity identity;StableId observer_actor_id,place_id;
    CheckpointMap<LocalEconomyAuthorityKey,CheckpointVector<StableId>> sources;
    CheckpointVector<ActingAuthority> authorities;
    CheckpointMap<StableId,FiscalNeedReference> need_by_arrear;
};
struct FiscalSettlementPage {
    WorldIdentity identity;StableId observer_actor_id,place_id;
    Integer cursor=0,scanned=0;
    std::optional<LocalEconomyCursor> next_cursor;
    std::vector<FiscalAssessmentRow> rows;
    std::vector<ActingAuthority> authorities;
};
// builder只供最终候选身份就位时调用，派生ID索引不进入存档。
FiscalSettlementIndex build_fiscal_settlement_index(const CommonStore&,const FiscalSettlementState&,const OrganizationState&,const OrganizationRead&);
Outcome<FiscalSettlementPage> query_fiscal_settlement(const CommonStore&,const FiscalSettlementState&,const FiscalSettlementCatalog&,const OrganizationRead&,const FiscalSettlementIndex&,const LocalEconomyQuery&);
BoundaryValue encode_fiscal_settlement_page(const FiscalSettlementPage&);
}
