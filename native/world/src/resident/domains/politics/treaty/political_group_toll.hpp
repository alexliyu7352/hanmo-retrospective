// 真Decision的收费投影；预采用与稳定历史分口，费用仍由S5原钱账唯一写入。
#pragma once
#include "political_group_passage_entry.hpp"
#include "resident/domains/travel/group_journey_funding.hpp"
namespace hanmo::resident {
// 仅S1原enter_boundaries同候选，在实际from和原manifest/Decision已写后使用。
// 不要求尚未发生的正片/crossing倒证；发布前仍必须走下面稳定历史口。
Outcome<ActualTransportExpense> read_group_political_toll_at_adoption(
 const GroupPoliticalPassageEntryContext&,const PoliticalPassageHistoryRead&,
 const SourceRef& actual_decision,const GroupMovementJourney&,const WorldMoment& actual_entry_at);
// 稳定历史必须已有真实crossing及正activity，不能因source存在就允许独立收费。
Outcome<ActualTransportExpense> read_original_group_political_toll(
 const GroupPoliticalPassageEntryContext&,const PoliticalPassageHistoryRead&,const SourceRef&,Integer schema);
// 仅原S1尚未append的本次crossing草稿核；实际付款闭包继续由S5/原S1整体验证。
Status validate_pending_group_political_crossing(const GroupPoliticalPassageEntryContext&,
 const PoliticalPassageHistoryRead&,const EconomicTransportExpenseState&,const BoundaryCrossingReceipt&);
// 稳定counterpart逐原crossing检查唯一收费绑定；S5完整钱腿门另行反验。
Status validate_original_group_political_crossing(const GroupPoliticalPassageEntryContext&,
 const PoliticalPassageHistoryRead&,const EconomicTransportExpenseState&,const BoundaryCrossingReceipt&);
// S5冷验借用同World实际Decision/运动源；不会从今日规则重报价或生成费用source。
class NativeHistoricalGroupPoliticalTollRead final:public TransportExpenseSourceRead {
 const GroupPoliticalPassageEntryContext& x_;const PoliticalPassageHistoryRead& history_;
public:
 NativeHistoricalGroupPoliticalTollRead(const GroupPoliticalPassageEntryContext& x,const PoliticalPassageHistoryRead& h):x_(x),history_(h){}
 Outcome<ActualTransportExpense> read_actual_transport_expense(const SourceRef&,Integer)const override;
 Outcome<WorldMoment> current_transport_expense_moment()const override;
 Status validate_transport_expense_authority(const SourceRef&,Integer,const TransportExpenseAuthority&)const override;
};
}
