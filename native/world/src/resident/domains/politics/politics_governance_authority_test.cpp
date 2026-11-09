// 政治连续性组件使用真实Common/Office/Grant；不声称Task/S5释放资源已集成。
#define main original_economy_fixture_main
#include "../../../../tests/resident/dynamic_economy_test.cpp"
#undef main
#include "politics_governance.hpp"
namespace {
// 不隐藏正式业务拒绝，所有源从实际批准入口产生。
template<class T>T require(Outcome<T> result){if(!result.ok){std::cerr<<result.error.original.code<<'\n';std::abort();}return std::move(result.value);}
struct GovernanceFixture:Fixture {
    PoliticsState politics;PoliticsGovernanceState governance;PoliticsGovernanceCatalog rules;
    explicit GovernanceFixture(const StableId& continuity="continue_under_organization"){
        common_catalog->places.insert("town",{"town","settlement",{}});common_catalog->texts.insert("appease.name","安民");
        auto* grant=org_catalog.grants.find("city.grant");grant->allowed_command_kinds.push_back("political_commission_governance");grant->amount_limit=1000;grant->may_delegate=true;grant->continuity_policy_id=continuity;grant->valid_until=2;
        Jurisdiction j;j.id="district";j.seat_place_id="town";j.political_organization_id=j.controlling_organization_id="city";j.office_id="mayor.office";j.public_wallet_id="city.wallet";j.public_inventory_ids={"city.inventory"};politics.jurisdictions.insert(j.id,j);
        rules.enabled=true;rules.appeasement_rules.insert("appease",{"appease","1","appease.name","grain",6,120,10,3,100});rules.commission_policies.insert("policy",{"policy","appease","relief.city",24,24});
    }
    // 私人预算从不作为公款短缺的备用支出，批准也不自动让对方接单。
    GovernanceAuthorityApproval issue(){return require(issue_governance_authority(common,*common_catalog,politics,governance,rules,organizations,org_catalog,activities,"mayor",{{"city","mayor.office",0,"mayor"},"student","district","policy","city.inventory",0}));}
    PoliticsGovernanceAccess access()const{return {common,politics,governance,rules,organizations,org_catalog,activities};}
};
}
int main(){
    GovernanceFixture f;auto approved=f.issue();assert(approved.rule_name_key=="appease.name"&&approved.work_ticks==6&&approved.cash_limit==120&&approved.grain_limit==10&&approved.offer_expires_tick==3);assert(f.common.wallet("city.wallet")->balance==7&&f.common.actor("student")->action_id.empty());
    require(f.access().governance_authority_for_acceptance(approved.authority_fact_id,"student",f.common.revision()));assert(!f.access().governance_authority_for_acceptance(approved.authority_fact_id,"teacher",f.common.revision()).ok);assert(!f.access().governance_authority_for_acceptance(approved.authority_fact_id,"student",f.common.revision()+1).ok);
    f.common.state().actors.find("student")->location=SettlementLocation{"settlement","elsewhere"};assert(!f.access().governance_authority_for_acceptance(approved.authority_fact_id,"student",f.common.revision()).ok);f.common.state().actors.find("student")->location=SettlementLocation{"settlement","town"};
    f.common.state().identity.tick=4;assert(!f.access().governance_authority_for_acceptance(approved.authority_fact_id,"student",f.common.revision()).ok);assert(require(f.access().governance_continuity(approved.authority_fact_id,"student")).kind==GovernanceContinuityKind::continue_work);
    require(invalidate_office(f.common,f.organizations,f.org_catalog,"mayor.office",4,approved.authority_fact_id));assert(f.organizations.organizations.find("city")->status==OrganizationStatus::limited);assert(require(f.access().governance_continuity(approved.authority_fact_id,"student")).kind==GovernanceContinuityKind::continue_work);
    f.common.state().actors.find("student")->available=false;assert(require(f.access().governance_continuity(approved.authority_fact_id,"student")).kind==GovernanceContinuityKind::pause_executor);f.common.state().actors.find("student")->available=true;
    f.politics.jurisdictions.find("district")->controlling_organization_id="other";assert(require(f.access().governance_continuity(approved.authority_fact_id,"student")).kind==GovernanceContinuityKind::needs_reassignment);f.politics.jurisdictions.find("district")->controlling_organization_id="city";
    f.common.state().actors.find("student")->alive=false;assert(require(f.access().governance_continuity(approved.authority_fact_id,"student")).kind==GovernanceContinuityKind::terminate_executor_dead);
    GovernanceFixture dissolved;auto d=dissolved.issue();dissolved.organizations.organizations.find("city")->status=OrganizationStatus::dissolved;assert(require(dissolved.access().governance_continuity(d.authority_fact_id,"student")).kind==GovernanceContinuityKind::terminate_principal_dissolved);
    GovernanceFixture stops("stop_when_vacant");auto stop=stops.issue();require(invalidate_office(stops.common,stops.organizations,stops.org_catalog,"mayor.office",0,stop.authority_fact_id));assert(require(stops.access().governance_continuity(stop.authority_fact_id,"student")).kind==GovernanceContinuityKind::needs_reassignment);
    std::cout<<"governance authority PASS: actual issue and local acceptance, expired offer versus accepted continuity, vacancy versus dissolution, five explicit lifecycle results; no Task/resource release claimed\n";
}
