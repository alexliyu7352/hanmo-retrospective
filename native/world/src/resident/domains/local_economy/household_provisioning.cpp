#include "resident/kernel/catalog_section_observation.hpp"
// 家庭私粮授权只接原供养周期；所有真实扣粮仍在local_economy的唯一ration结算点。
#include "household_provisioning.hpp"
#include "local_economy_internal.hpp"
namespace hanmo::resident {
namespace {
using namespace local_economy_detail;
// 原义务的付款源、周期和授权不可由冷态篡改；next_due和last_receipt由原阶段维护。
bool same_obligation(const RecurringObligation& a,const RecurringObligation& b){
    return a.id==b.id&&same_subject(a.principal,b.principal)&&same_subject(a.beneficiary,b.beneficiary)&&a.kind==b.kind&&a.source_wallet_or_inventory==b.source_wallet_or_inventory&&a.destination==b.destination&&a.rate_profile==b.rate_profile&&a.authority_ref==b.authority_ref&&a.item_id==b.item_id&&a.period_ticks==b.period_ticks;
}
// 原供养区间是唯一责任边界，不能在冷态擅自延长私产授权。
bool same_binding(const ProvisionBinding& a,const ProvisionBinding& b){
    return a.id==b.id&&same_subject(a.subject,b.subject)&&a.from_tick==b.from_tick&&a.through_tick==b.through_tick&&a.payer_inventory==b.payer_inventory&&a.obligation_id==b.obligation_id&&a.policy_owner==b.policy_owner&&a.military_source_id==b.military_source_id;
}
// 原人群可因真实转移增减人数，家庭归属与私粮容器仍必须吻合。
bool owns_source(const CommonStore& common,const HouseholdGroup* household,const PopulationCohort* cohort,const HouseholdProvisionAuthorization& a){
    const auto* inventory=common.inventory(a.inventory_id);
    return household&&cohort&&household->id==a.household_group_id&&household->inventory_id==a.inventory_id&&cohort->id==a.cohort_id&&cohort->household_group_id==a.household_group_id&&cohort->private_inventory_id==a.inventory_id&&inventory&&inventory->owner.kind==Owner::Kind::household&&inventory->owner.id==a.household_group_id;
}
// 用真正登记表解析作者授权，作者字符串本身不算已发生事实。
const HouseholdProvisionRegistration* registration_for(const HouseholdProvisionState& state,const StableId& authorization){
    const HouseholdProvisionRegistration* result=nullptr;
    for(const auto& row:state.registrations.rows())if(row.second.authorization_id==authorization){if(result)return nullptr;result=&row.second;}
    return result;
}
}
// 独立模块只接受显式版本和一义务一来源；未知产权、重复授权与不完整周期立即拒绝。
Status validate_household_provision_catalog(const CommonStore& common,const LocalEconomyCatalog& economy,const HouseholdProvisionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 72, "HouseholdProvisionCatalog", "validate_household_provision_catalog", &catalog);

    if(!economy.enabled||catalog.version!=1||catalog.authorizations.empty()||catalog.authorizations.size()>65536)return common_failure("InvalidHouseholdProvisionCatalog");
    std::set<StableId> obligations,bindings;
    for(const auto& row:catalog.authorizations.rows()){
        const auto& a=row.second;const auto* obligation=economy.obligations.find(a.obligation_id);const auto* binding=economy.provision_bindings.find(a.provision_binding_id);const auto* rate=economy.rates.find(a.rate_profile_id);
        if(row.first!=a.id||a.id.empty()||!owns_source(common,economy.households.find(a.household_group_id),economy.cohorts.find(a.cohort_id),a)||!obligation||!binding||!rate||!common.item_definition(a.item_id)||!obligations.insert(a.obligation_id).second||!bindings.insert(a.provision_binding_id).second)return common_failure("InvalidHouseholdProvisionSource",row.first);
        if(obligation->kind!=ObligationKind::ration||obligation->principal.kind!=SubjectKind::cohort||obligation->principal.id!=a.cohort_id||!same_subject(obligation->principal,obligation->beneficiary)||obligation->authority_ref!=a.id||obligation->source_wallet_or_inventory!=a.inventory_id||obligation->item_id!=a.item_id||obligation->rate_profile!=a.rate_profile_id||!obligation->destination.empty()||!obligation->active||obligation->period_ticks<=0||!rate->per_capita||rate->numerator<0||rate->denominator<=0)return common_failure("InvalidHouseholdRationObligation",a.id);
        if(binding->policy_owner!=ProvisionOwner::macro||!binding->military_source_id.empty()||!same_subject(binding->subject,obligation->beneficiary)||binding->obligation_id!=obligation->id||binding->payer_inventory!=a.inventory_id||binding->from_tick<0||binding->through_tick<binding->from_tick||binding->through_tick==INT64_MAX||obligation->next_due_tick<=binding->from_tick||obligation->next_due_tick>binding->through_tick)return common_failure("InvalidHouseholdProvisionInterval",a.id);
    }
    return common_success();
}
// 登记以共同保存点保全：任何失败都不留下半条授权事实或已消耗ID。
Outcome<HouseholdProvisionState> make_initial_household_provision(CommonStore& common,const LocalEconomyCatalog& economy,const HouseholdProvisionCatalog& catalog){
    auto checked=validate_household_provision_catalog(common,economy,catalog);if(!checked.ok)return {false,{},checked.error};
    PhaseSavepoint guard(common);if(!guard.token().ok)return {false,{},guard.token().error};HouseholdProvisionState state;
    for(const auto& row:catalog.authorizations.rows()){
        const auto* obligation=economy.obligations.find(row.second.obligation_id);if(obligation->next_due_tick<=common.tick())return {false,{},common_failure("InvalidHouseholdProvisionInitialTick",row.first).error};
        auto id=common.next_id("household_provision");if(!id.ok)return {false,{},id.error};auto appended=common.append_fact({id.value,"household_provision_registered","local_economy"});if(!appended.ok)return {false,{},appended.error};state.registrations.insert(id.value,{id.value,row.first,common.tick()});
    }
    auto committed=guard.commit();if(!committed.ok)return {false,{},committed.error};return {true,std::move(state),{}};
}
// 每个真实登记必须对应原来源；终止事实只在原区间结束后的边界存在。
Status validate_household_provision_state(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionCatalog& catalog,const HouseholdProvisionState& state){
    auto checked=validate_household_provision_catalog(common,original,catalog);if(!checked.ok)return checked;
    if(state.version!=1||state.registrations.size()!=catalog.authorizations.size())return common_failure("InvalidHouseholdProvisionState");std::set<StableId> registered,closed;
    for(const auto& row:state.registrations.rows()){
        const auto& r=row.second;const auto* a=catalog.authorizations.find(r.authorization_id);const auto* header=common.state().facts.find(row.first);if(row.first!=r.id||!a||!header||header->kind!="household_provision_registered"||header->domain!="local_economy"||r.tick<0||r.tick>economy.last_step_tick||!registered.insert(r.authorization_id).second)return common_failure("InvalidHouseholdProvisionRegistration",row.first);
        const auto* obligation=economy.obligations.find(a->obligation_id);const auto* binding=economy.provision_bindings.find(a->provision_binding_id);if(!owns_source(common,economy.households.find(a->household_group_id),economy.cohorts.find(a->cohort_id),*a)||!obligation||!binding||!same_obligation(*obligation,*original.obligations.find(a->obligation_id))||!same_binding(*binding,*original.provision_bindings.find(a->provision_binding_id))||r.tick>=original.obligations.find(a->obligation_id)->next_due_tick)return common_failure("HouseholdProvisionSourceChanged",row.first);
    }
    // 每条私人收据必须属于原始账期且无缺期；next_due从真实收据数量导出，不能冷态跳期或提前记账。
    for(const auto& row:catalog.authorizations.rows()){
        const auto& a=row.second;const auto* first=original.obligations.find(a.obligation_id);const auto* current=economy.obligations.find(a.obligation_id);const auto* binding=economy.provision_bindings.find(a.provision_binding_id);const auto stop=std::min(economy.last_step_tick,binding->through_tick);Integer periods=stop<first->next_due_tick?0:(stop-first->next_due_tick)/first->period_ticks+1;Integer elapsed=0,expected_next=0;if(!multiply(periods,first->period_ticks,elapsed)||!add(first->next_due_tick,elapsed,expected_next)||current->next_due_tick!=expected_next)return common_failure("HouseholdProvisionPeriodMismatch",a.id);
        std::set<Integer> dates;const RationReceipt* latest=nullptr;
        for(const auto& receipt:economy.rations.rows())if(receipt.second.obligation_id==a.obligation_id){const auto& r=receipt.second;if(r.due_tick<first->next_due_tick||r.due_tick>stop||(r.due_tick-first->next_due_tick)%first->period_ticks||!dates.insert(r.due_tick).second)return common_failure("HouseholdProvisionReceiptPeriod",r.id);if(!latest||r.due_tick>latest->due_tick)latest=&r;}
        if(dates.size()!=static_cast<std::size_t>(periods)||(latest?current->last_receipt_id!=latest->id:!current->last_receipt_id.empty()))return common_failure("HouseholdProvisionMissingReceipt",a.id);
    }
    for(const auto& row:state.closures.rows()){
        const auto& closure=row.second;const auto* r=state.registrations.find(closure.registration_id);const auto* a=r?catalog.authorizations.find(r->authorization_id):nullptr;const auto* binding=a?economy.provision_bindings.find(a->provision_binding_id):nullptr;const auto* header=common.state().facts.find(row.first);
        if(row.first!=closure.id||!a||!binding||closure.obligation_id!=a->obligation_id||closure.tick!=binding->through_tick+1||closure.tick>economy.last_step_tick||!header||header->kind!="household_provision_ended"||header->domain!="local_economy"||!closed.insert(closure.registration_id).second)return common_failure("InvalidHouseholdProvisionClosure",row.first);
    }
    for(const auto& row:state.registrations.rows()){
        const auto* a=catalog.authorizations.find(row.second.authorization_id);const auto* binding=economy.provision_bindings.find(a->provision_binding_id);const auto* obligation=economy.obligations.find(a->obligation_id);const bool ended=economy.last_step_tick>binding->through_tick;
        if(closed.count(row.first)!=static_cast<std::size_t>(ended)||obligation->active==ended)return common_failure("HouseholdProvisionClosureMismatch",row.first);
    }
    return common_success();
}
// 结算当期仍核对原登记和容器；其他群体、期限外或已经关闭的授权都失败。
Status check_household_ration_authority(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& original,const HouseholdProvisionCatalog& catalog,const HouseholdProvisionState& state,const RecurringObligation& obligation,Integer due){
    const auto* a=catalog.authorizations.find(obligation.authority_ref);const auto* r=a?registration_for(state,a->id):nullptr;const auto* binding=a?economy.provision_bindings.find(a->provision_binding_id):nullptr;const auto* source=a?original.obligations.find(a->obligation_id):nullptr;const auto* original_binding=a?original.provision_bindings.find(a->provision_binding_id):nullptr;const auto* header=r?common.state().facts.find(r->id):nullptr;
    if(catalog.version!=1||state.version!=1||!a||!r||!source||!binding||!original_binding||!header||header->kind!="household_provision_registered"||header->domain!="local_economy"||r->tick>=due||!same_obligation(obligation,*source)||!same_binding(*binding,*original_binding)||due<binding->from_tick||due>binding->through_tick||!owns_source(common,economy.households.find(a->household_group_id),economy.cohorts.find(a->cohort_id),*a))return common_failure("HouseholdProvisionAuthorityDenied",obligation.id);
    for(const auto& row:state.closures.rows())if(row.second.registration_id==r->id)return common_failure("HouseholdProvisionAuthorityEnded",obligation.id);
    return common_success();
}
// 结束只记录责任停止；过去的粮食和收据不会退回或重算。
Status close_expired_household_provision(CommonStore& common,LocalEconomyState& economy,const LocalEconomyCatalog& catalog,Integer tick,LocalEconomyEffects& effects){
    if(!catalog.household_provisioning&&!economy.household_provisioning)return common_success();if(!catalog.household_provisioning||!economy.household_provisioning)return common_failure("HouseholdProvisionProfileMismatch");
    auto& state=*economy.household_provisioning;
    for(const auto& row:state.registrations.rows()){
        const auto* a=catalog.household_provisioning->authorizations.find(row.second.authorization_id);const auto* binding=a?economy.provision_bindings.find(a->provision_binding_id):nullptr;auto* obligation=a?economy.obligations.find(a->obligation_id):nullptr;if(!binding||!obligation)return common_failure("HouseholdProvisionSourceChanged",row.first);if(tick<=binding->through_tick)continue;
        bool closed=false;for(const auto& closure:state.closures.rows())closed|=closure.second.registration_id==row.first;if(closed)continue;
        if(obligation->next_due_tick<=binding->through_tick)return common_failure("HouseholdProvisionUnsettledPeriod",obligation->id);
        auto id=common.next_id("household_provision_end");if(!id.ok)return {false,{},id.error};auto appended=common.append_fact({id.value,"household_provision_ended","local_economy"});if(!appended.ok)return appended;state.closures.insert(id.value,{id.value,row.first,obligation->id,binding->through_tick+1});obligation->active=false;effects.fact_ids.push_back(id.value);
    }
    return common_success();
}
// 新模块的劳力必须有当前唯一供养周期覆盖；旧一次足粮不能无限维持未来劳动。
Outcome<std::optional<Integer>> household_provision_workers(const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,const LaborAllocation& allocation,Integer due,std::vector<StableId>& causes){
    bool managed=false;if(catalog.household_provisioning)for(const auto& row:catalog.household_provisioning->authorizations.rows())managed|=row.second.cohort_id==allocation.cohort_id;if(!managed)return {true,std::nullopt,{}};
    const ProvisionBinding* binding=nullptr;for(const auto& row:economy.provision_bindings.rows()){const auto& b=row.second;if(b.subject.kind==SubjectKind::cohort&&b.subject.id==allocation.cohort_id&&b.from_tick<=due&&due<=b.through_tick){if(binding)return {false,{},common_failure("DuplicateProvisionBinding",allocation.cohort_id).error};binding=&b;}}
    if(!binding)return {true,Integer{0},{}};if(binding->policy_owner!=ProvisionOwner::macro)return {true,std::nullopt,{}};
    const auto* obligation=economy.obligations.find(binding->obligation_id);if(!obligation||obligation->period_ticks<=0)return {false,{},common_failure("ProvisionBindingMismatch",binding->id).error};
    const RationReceipt* ration=nullptr;for(const auto& row:economy.rations.rows()){const auto& r=row.second;if(r.obligation_id==obligation->id&&r.due_tick<=due&&due-r.due_tick<obligation->period_ticks&&r.due_tick>=binding->from_tick&&(!ration||r.due_tick>ration->due_tick))ration=&r;}
    if(!ration)return {true,Integer{0},{}};causes.push_back(ration->id);if(!ration->required)return {true,allocation.workers,{}};Integer amount=0,remainder=0;if(!ratio(allocation.workers,ration->consumed,ration->required,0,amount,remainder))return {false,{},common_failure("EconomyQuantityOverflow",allocation.id).error};return {true,amount,{}};
}
}
