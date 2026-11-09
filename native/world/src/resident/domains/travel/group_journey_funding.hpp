// S1 dispatches original commercial payment and original military march debit.
// The military branch is only a reference to the actual accepted S2 order
// embedded in this journey's immutable opening source.
#pragma once
#include "transport_authority.hpp"
namespace hanmo::resident {
struct MilitaryMarchExpenseAuthority {
    SourceRef accepted_order_source;
    StableId army_id,campaign_id,payer_wallet_id;
};
using GroupJourneyFunding=std::variant<TransportExpenseAuthority,MilitaryMarchExpenseAuthority>;
const TransportExpenseAuthority* read_commercial_group_funding(const GroupJourneyFunding&) noexcept;
bool same_transport_expense_authority(const GroupJourneyFunding&,const TransportExpenseAuthority&);
}
