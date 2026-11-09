#include "execution_phase.hpp"
#include "resident/domains/organization/institutional_obligation_source.hpp"
namespace hanmo::resident {
Status validate_taken_obligation_accrual(const EconomicTakeoverReadContext& x,
    const RecurringObligation& obligation,Integer amount,Integer due_tick){
    if(!x.organizations||!x.organization_catalog||obligation.principal.kind!=SubjectKind::organization)
        return common_failure("EconomicAccrualSourceRequired",obligation.id);
    const bool membership=x.economy.membership_wage_bindings.find(obligation.authority_ref)!=nullptr;
    const bool allowed=membership
        ?membership_wage_source_authorized(x.common,*x.organizations,*x.organization_catalog,x.economy,
            obligation.authority_ref,obligation.id,obligation.principal.id,
            obligation.source_wallet_or_inventory,amount,due_tick)
        :institutional_issuer_obligation_accrual_allowed(x.common,*x.organizations,*x.organization_catalog,x.economy,
            obligation.authority_ref,obligation.id,obligation.principal.id,
            obligation.source_wallet_or_inventory,amount,due_tick);
    return allowed?common_success():common_failure("InstitutionalAccrualSourceDenied",obligation.id);
}
}
