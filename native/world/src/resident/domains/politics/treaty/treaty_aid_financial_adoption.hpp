// 原条约援助的 S6 调度与 S5 财务同候选编排；没有替代双方资源接受的输入。
#pragma once
#include "treaty_aid_schedule_sources.hpp"
#include "resident/domains/local_economy/material_aid/material_aid.hpp"
namespace hanmo::resident {
struct TreatyAidFinancialAdoptionContext {
 const TreatyAidScheduleSourceContext& schedules;
 const TreatyAidPerformanceReadContext& performance;
 const MaterialAidWriteContext& finance;
};
// 每个源必须是 S5 原 resources reader 验过的同版双方接受。这里只接受源引用，
// 不接受自由钱包、库、预算或 amount，也不从允许容器列表选择默认项。
Outcome<std::vector<SourceRef>> accept_treaty_aid_schedules_in_candidate(CommonStore&,
 OrderedTable<TreatyAidSchedule>&,TreatyMaterialAidState&,
 const TreatyAidFinancialAdoptionContext&,const SourceRef& actual_activation,
 const std::vector<MomentOwnerSource>& actual_resource_acceptances);
// 原 S5 成立之后立即采用 S6 唯一 cursor。外层同一 candidate 还必须写真实
// promise_episode_binding；没有该原承诺源/全体 term 采用，不能发布完整 activation。
Outcome<SourceRef> establish_and_adopt_treaty_aid_occurrence_in_candidate(CommonStore&,
 OrderedTable<TreatyAidSchedule>&,OrderedTable<TreatyAidOccurrenceBinding>&,
 TreatyMaterialAidState&,const TreatyAidFinancialAdoptionContext&,
 const StableId& schedule_id,Integer occurrence_ordinal);
// 由真实 termination 枚举全部原 aid schedules；只释放未成立 holds，并保存原
// schedule termination_ref，不移动 cursor。返回实际 S5 释放源供原总处分行消费。
Outcome<std::vector<SourceRef>> release_treaty_aid_schedules_in_candidate(CommonStore&,
 OrderedTable<TreatyAidSchedule>&,TreatyMaterialAidState&,
 const TreatyAidFinancialAdoptionContext&,const SourceRef& actual_termination);
// 这只是两域援助原源全覆盖；不替代承诺 episode、通知、条约或全部 term 冷验证。
Status validate_treaty_aid_financial_adoption(const TreatyAidFinancialAdoptionContext&);
}
