// 原军事占领的同候选组合入口；捕获、资产应用、pending登记和各域暂停不得分次提交。
#pragma once
#include "resident/domains/military/occupation_model.hpp"
#include "politics_takeover_catalog.hpp"
#include "resident/domains/military/occupation_reservations.hpp"
#include "politics_takeover_original_access.hpp"
namespace hanmo::resident {
// 回调只处理实际已登记责任的执行侧。欠额、受益人和资产产权不由政治域重复修改。
using PoliticalTakeoverPendingApplication=std::function<Status(
    const PoliticalTakeoverRegistrationSource&,const std::vector<PoliticalResponsibilityExecution>&)>;
struct PoliticalTakeoverApplicationContext {
    PoliticalTakeoverOriginalContext original;
    const CommonCatalog& common_catalog;
    const OrganizationCatalog& organization_catalog;
    const PoliticsCatalog& politics_catalog;
    const PoliticalTakeoverCatalog& catalog;
    PoliticsState& politics;
    OccupationReservationRead reservations;
    const OrganizationAuthorityClock* actual_clock=nullptr;
    PoliticalTakeoverProductionWriteContext* production=nullptr;
};
struct PoliticalTakeoverApplication {
    OccupationEffect occupation_effect;
    std::optional<StableId> registration_fact_id;
};
// 调用者必须在错误时丢弃全部候选，包含Common、Military、经济/任务与本责任账。
Outcome<PoliticalTakeoverApplication> apply_political_military_takeover_in_transaction(
    CommonStore&,MilitaryState&,PoliticalTakeoverState&,const PoliticalTakeoverApplicationContext&,
    const StableId& occupation_fact_id,const PoliticalTakeoverPendingApplication&);
}
