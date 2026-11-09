// Original military march cost: one Common debit, no invented merchant or S5 payable.
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/domains/travel/group_journey_funding.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
// First-positive source is adopted at the trusted selected span.from before movement.
// The S1 reader proves this exact selected positive span; whole-candidate validation
// requires its real subsequent activity coverage before publication.
struct MilitaryMarchFirstPositivePrefix {MomentSpan applied_span;SourceRef original_prefix;Integer prefix_schema=1;};
struct MilitaryMarchZeroContact {SourceRef actual_contact;Integer contact_schema=1;};
struct ActualMilitaryMarchExpense {
 SourceRef actual_unit_cost;Integer source_schema=1;StableId payer_wallet_id,original_cost_rule_ref;
 Integer amount=0;WorldMoment occurred_at;std::variant<MilitaryMarchFirstPositivePrefix,MilitaryMarchZeroContact> actual_start;
};
struct MilitaryMarchWalletDebit {
 SourceRef actual_unit_cost;Integer source_schema=1;StableId payer_wallet_id;
 Integer amount=0,balance_before=0,balance_after=0;WorldMoment occurred_at;
};
class MilitaryMarchExpenseSourceRead:public CurrentMomentRead {
public:virtual ~MilitaryMarchExpenseSourceRead()=default;
 // S1/S2 resolves the actual first prefix/zero-contact and accepted military order,
 // including army, command, campaign, formation, lawful wallet and original cost rule.
 virtual Outcome<ActualMilitaryMarchExpense> read_actual_military_march_expense(const SourceRef&,Integer,const MilitaryMarchExpenseAuthority&)const=0;
 // Borrow the already recorded original MovementExpenseSource, if it exists.
 virtual Outcome<std::optional<MilitaryMarchWalletDebit>> read_recorded_military_march_debit(const SourceRef&,Integer)const=0;
};
Outcome<MilitaryMarchWalletDebit> apply_military_march_wallet_debit_in_candidate(CommonStore&,const MilitaryMarchExpenseSourceRead&,const SourceRef& actual_unit_cost,Integer source_schema,const MilitaryMarchExpenseAuthority&);
}
