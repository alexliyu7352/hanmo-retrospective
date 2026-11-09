// S1运输费用唯一联合；预算、私约、自资及家庭许可均由原经济owner验证并实际记账。
#pragma once
#include "../../model/dynamic_contract.hpp"
#include "../../kernel/world_timeline.hpp"
namespace hanmo::resident {
// 引用真实公共预算执行，不能只拿钱包ID充当支出权。
struct PublicBudgetExpenseAuthority { StableId execution_ref,payer_wallet_id; };
// 多个旅程共用原委任的毛支出额度，退款不重开额度。
struct PrivateMandateExpenseAuthority { StableId mandate_id; Integer accepted_terms_revision=0,authority_epoch=0; StableId payer_wallet_id; };
// 自费条款由真实当事人接受并绑定用途；不是推定的私人余额许可。
struct SelfFundedExpenseAuthority { StableId funding_terms_id,principal_actor_id,payer_wallet_id; };
// 家庭自治只引用已接受购置政策与原采购，不伪造领队actor或私约。
struct HouseholdPurchaseExpenseAuthority { StableId household_id,accepted_policy_ref; Integer policy_revision=0; StableId purchase_plan_ref; std::optional<StableId> purchase_ref; StableId phase; };
using TransportExpenseAuthority=std::variant<PublicBudgetExpenseAuthority,PrivateMandateExpenseAuthority,SelfFundedExpenseAuthority,HouseholdPurchaseExpenseAuthority>;
// 运输签发人联合只容真实actor权源或已接受的有限家庭采购源。
struct ActorGroupInitiator { StableId initiator_actor_id,authority_ref; };
struct HouseholdPolicyGroupInitiator { StableId accepted_household_policy_ref; Integer policy_revision=0; StableId purchase_plan_ref; std::optional<StableId> purchase_ref; StableId phase; };
using GroupInitiator=std::variant<ActorGroupInitiator,HouseholdPolicyGroupInitiator>;
} // 这里不保存任何预算余额或替领域生成付款源。
