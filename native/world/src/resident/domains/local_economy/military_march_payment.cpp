#include "military_march_payment.hpp"
namespace hanmo::resident {
Outcome<MilitaryMarchWalletDebit> apply_military_march_wallet_debit_in_candidate(CommonStore&common,const MilitaryMarchExpenseSourceRead&sources,const SourceRef&ref,Integer schema,const MilitaryMarchExpenseAuthority&authority){
 const auto fail=[](const char*code,const StableId&id)->Outcome<MilitaryMarchWalletDebit>{return {false,{},common_failure(code,id).error};};
 if(!valid_source_ref(ref)||schema<1||!valid_source_ref(authority.accepted_order_source)||authority.army_id.empty()||authority.campaign_id.empty()||authority.payer_wallet_id.empty())return fail("InvalidMilitaryMarchExpenseAuthority",ref.id);
 auto read=sources.read_actual_military_march_expense(ref,schema,authority);if(!read.ok)return {false,{},read.error};const auto&actual=read.value;
 if(!same_source_ref(ref,actual.actual_unit_cost)||schema!=actual.source_schema||actual.payer_wallet_id!=authority.payer_wallet_id||actual.original_cost_rule_ref.empty()||actual.amount<0)return fail("MilitaryMarchExpenseOriginalSourceMismatch",ref.id);
 auto now=sources.read_current_moment();if(!now.ok)return {false,{},now.error};auto clock=compare_world_moments(actual.occurred_at,now.value);if(!clock.ok)return {false,{},clock.error};if(clock.value>0)return fail("MilitaryMarchExpenseFromFuture",ref.id);
 auto old=sources.read_recorded_military_march_debit(ref,schema);if(!old.ok)return {false,{},old.error};
 if(old.value){const auto&r=*old.value;auto at=compare_world_moments(r.occurred_at,actual.occurred_at);if(!at.ok)return {false,{},at.error};if(!same_source_ref(r.actual_unit_cost,ref)||r.source_schema!=schema||r.payer_wallet_id!=actual.payer_wallet_id||r.amount!=actual.amount||r.balance_before<r.amount||r.balance_after!=r.balance_before-r.amount||at.value)return fail("MilitaryMarchExpenseReplayMismatch",ref.id);return {true,r,{}};}
 if(const auto*prefix=std::get_if<MilitaryMarchFirstPositivePrefix>(&actual.actual_start)){auto from=compare_world_moments(prefix->applied_span.from,actual.occurred_at),start=compare_world_moments(prefix->applied_span.from,now.value),length=compare_world_moments(prefix->applied_span.from,prefix->applied_span.to);if(!from.ok)return {false,{},from.error};if(!start.ok)return {false,{},start.error};if(!length.ok)return {false,{},length.error};if(!valid_source_ref(prefix->original_prefix)||prefix->prefix_schema<1||from.value||start.value||length.value>=0)return fail("MilitaryMarchExpenseFirstPrefixMismatch",ref.id);}else{const auto&contact=std::get<MilitaryMarchZeroContact>(actual.actual_start);if(!valid_source_ref(contact.actual_contact)||contact.contact_schema<1||clock.value)return fail("MilitaryMarchExpenseActualContactMismatch",ref.id);}
 const auto*wallet=common.wallet(actual.payer_wallet_id);if(!wallet||wallet->balance<actual.amount)return fail("MilitaryMarchExpenseInsufficientFunds",actual.payer_wallet_id);
 MilitaryMarchWalletDebit result{ref,schema,actual.payer_wallet_id,actual.amount,wallet->balance,wallet->balance-actual.amount,actual.occurred_at};
 if(actual.amount){ResourceRecipe recipe;recipe.expected_versions={common.read_version({ResourceKey::Kind::wallet,actual.payer_wallet_id})};recipe.operations={WalletDelta{actual.payer_wallet_id,-actual.amount,true}};auto applied=common.apply_resources(recipe);if(!applied.ok)return {false,{},applied.error};}
 // S1 appends this actual result to its sole MovementExpenseSource in the same
 // outer candidate. It owns replay and source history; no second money table is made.
 return {true,std::move(result),{}};
}
}
