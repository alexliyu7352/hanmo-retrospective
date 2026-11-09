// 只验中央已有Common子事务中的政治薄叶，不重复旧任免/财政矩阵或冒完整Director运行。
#define main political_component_matrix_not_run
#include "politics_test.cpp"
#undef main
#include "resident/kernel/politics_fiscal_integration.hpp"
int main(){
    Fixture f;SocialState social;PoliticsWriteContext context{f.common,*f.cc,f.politics,f.pc,f.org,f.oc,f.economy,social,f.activities};
    PoliticsCommand command=f.appoint();const auto quote=need(preview_political_business(context.read(),"lord",command));
    const auto original_politics=f.politics;const auto original_org=f.org;const auto original_common=encode_common_state(f.common.state());
    auto mark=need(f.common.begin_savepoint());auto offer=need(apply_political_business(context,"lord",command,quote));
    check(!offer.appointment_id.empty()&&f.politics.appointments.size()==1,"politics business runs inside existing common transaction");
    need(f.common.rollback_savepoint(mark));f.politics=original_politics;f.org=original_org;
    check(compare_boundary(original_common,encode_common_state(f.common.state())).exact(),"outer rollback restores common facts and allocation counter");
    // 在已分配appointment ID之后故意耗尽事实ID，验失败仍由唯一外层还原。
    f.common.state().next_instance_id=INT64_MAX-1;const auto counter=f.common.state().next_instance_id;mark=need(f.common.begin_savepoint());
    auto failed=apply_political_business(context,"lord",command,quote);check(!failed.ok,"partial political allocation fails explicitly");need(f.common.rollback_savepoint(mark));f.politics=original_politics;f.org=original_org;
    check(f.common.state().next_instance_id==counter&&f.common.state().facts.empty()&&f.politics.appointments.empty(),"outer rollback removes failed political allocation");
    f.common.state().next_instance_id=1;
    auto* grant=f.oc.grants.find("lord.grant");grant->allowed_command_kinds.push_back("political_set_tax");
    ActorMutation move;move.actor_id="lord";move.location=SettlementLocation{"county","county.seat"};need(f.common.apply_actor(move));
    PoliticalFiscalState fiscal;PoliticalFiscalCatalog fiscal_catalog{true};PoliticalFiscalWriteContext fiscal_context{f.common,*f.cc,f.politics,fiscal,fiscal_catalog,f.org,f.oc,f.activities};
    PoliticalSetTaxCommand tax{{"realm","lord.office",0,"lord"},"county","public.granary",0,0,FiscalTaxMode::normal};
    const auto tax_quote=need(preview_political_fiscal_business(fiscal_context.read(),"lord",tax));const auto before=encode_common_state(f.common.state());const auto old_fiscal=fiscal;
    mark=need(f.common.begin_savepoint());auto policy=need(apply_political_fiscal_business(fiscal_context,"lord",tax,tax_quote));
    check(!policy.policy_fact_id.empty()&&fiscal.policies.size()==1,"fiscal business runs inside existing common transaction");need(f.common.rollback_savepoint(mark));fiscal=old_fiscal;
    check(compare_boundary(before,encode_common_state(f.common.state())).exact()&&fiscal.policies.empty(),"outer rollback removes policy without moving funds");
    mark=need(f.common.begin_savepoint());policy=need(apply_political_fiscal_business(fiscal_context,"lord",tax,tax_quote));need(f.common.commit_savepoint(mark));
    check(fiscal.policies.size()==1&&f.common.state().facts.find(policy.policy_fact_id),"outer transaction commits one real policy source");
    std::cout<<"PASS caller-owned politics/fiscal transaction: existing savepoint, exact rollback, allocation failure rollback and committed policy; no Director/full-world claim\n";
}
