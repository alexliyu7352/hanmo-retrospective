// Immediate consumption/rollback/cold-source gate. Family authorization is an explicit typed fixture,
// not evidence of the future registered Family/World integration.
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/household/provision_management_codec.hpp"
#include "resident/domains/local_economy/household/provision_management_view.hpp"
struct ControlFixture final:HouseholdProvisionControlRead {
    CommonStore& common;bool enabled=true;
    explicit ControlFixture(CommonStore& value):common(value){assert(common.append_fact({"family.control","family_household_control","family"}).ok);}
    Outcome<std::optional<HouseholdProvisionControlProof>> current(const StableId& actor,const StableId& household,const StableId& operation,Integer tick)const override{
        if(actor!="teacher"||household!="family"||operation!=household_provision_operation||tick!=common.tick())return {false,{},common_failure("InvalidControlFixture").error};
        if(!enabled)return {true,std::nullopt,{}};
        return {true,HouseholdProvisionControlProof{actor,household,"family.control","private.household.role",operation,common.state().identity.revision,0,99},{}};
    }
    Status validate_accepted(const HouseholdProvisionControlProof& proof,Integer tick)const override{
        if(!common.state().facts.find(proof.fact_id)||proof.fact_id!="family.control"||proof.actor_id!="teacher"||proof.household_group_id!="family"||proof.source_role_id!="private.household.role"||proof.operation!=household_provision_operation||tick<0||tick>99)return common_failure("InvalidControlFixture");return common_success();
    }
};
int main(){
    Fixture f;
    auto& obligation=*f.catalog.obligations.find("workers.food");obligation.principal={SubjectKind::cohort,"workers"};obligation.source_wallet_or_inventory="family.inventory";obligation.authority_ref="family.self";
    auto& binding=*f.catalog.provision_bindings.find("workers.provision");binding.payer_inventory="family.inventory";binding.through_tick=6;
    f.catalog.household_provisioning=HouseholdProvisionCatalog{};f.catalog.household_provisioning->authorizations.insert("family.self",{"family.self","family","workers","workers.food","workers.provision","family.inventory","grain","food"});
    auto& allowed=f.org_catalog.institutional_issuers.find("institution")->obligation_ids;allowed.erase(std::remove(allowed.begin(),allowed.end(),"workers.food"),allowed.end());
    assert(f.common.apply_resources({{AddLot{"family.inventory","grain",50,{Owner::Kind::household,"family",{}},{}}},{}}).ok);
    auto initial=make_initial_local_economy(f.common,f.catalog,0);assert(initial.ok);f.economy=std::move(initial.value);f.validate();
    ControlFixture control(f.common);HouseholdProvisionManagementCatalog catalog{1,{"family.self"}};HouseholdProvisionManagementState management;
    ConfigureHouseholdProvisionCommand command{"teacher","family.self",HouseholdProvisionConfigurationKind::configure,Integer{0}};
    auto apply=[&]{return apply_configure_household_provision(f.common,f.economy,f.catalog,catalog,management,control,command);};
    auto first=apply();assert(first.ok&&first.value.fact_ids.size()==1);
    auto preview=preview_configure_household_provision(f.common,f.economy,f.catalog,catalog,management,control,command);assert(preview.ok&&preview.value.effective_tick==1&&preview.value.first_due_tick==2);
    // Failure after a real pending-resolution fact must roll back both domain state and Common IDs.
    auto sequence=f.common.state().next_instance_id;f.common.state().next_instance_id=INT64_MAX-1;
    const auto before=encode_common_state(f.common.state()),before_management=encode_household_provision_management_state(management);
    command.consumption_limit=4;auto failed=apply();assert(!failed.ok&&compare_boundary(before,encode_common_state(f.common.state())).exact()&&compare_boundary(before_management,encode_household_provision_management_state(management)).exact());
    f.common.state().next_instance_id=sequence;
    auto replaced=apply();assert(replaced.ok&&replaced.value.fact_ids.size()==2);
    command.kind=HouseholdProvisionConfigurationKind::cancel_pending;command.consumption_limit.reset();assert(apply().ok&&management.current_by_authorization.empty());
    command.kind=HouseholdProvisionConfigurationKind::configure;command.consumption_limit=0;assert(apply().ok);
    auto step=[&](Integer tick){OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);HouseholdProvisionManagementContext context{catalog,management};auto result=step_local_economy(f.common,f.economy,f.catalog,access,f.activities,{tick-1,tick,{},{},{}},&context);assert(result.ok);f.common.state().identity.tick=tick;assert(validate_household_provision_management(f.common,f.economy,f.catalog,catalog,management,control).ok);f.validate();};
    auto ration=[&](Integer tick)->RationReceipt{for(const auto& row:f.economy.rations.rows())if(row.second.obligation_id=="workers.food"&&row.second.due_tick==tick)return row.second;std::abort();};
    auto food=[&](){return f.common.count("family.inventory","grain",{{Owner::Kind::household,"family",{}}}).value;};
    step(1);assert(ration(1).required==10&&ration(1).consumed==10&&food()==40);
    f.common.state().metadata.player_actor_id="teacher";
    auto index=build_household_provision_view_index(f.common,f.economy,f.catalog,catalog,management,control);assert(index.ok&&index.value.authorization_ids.size()==1);
    LocalEconomyQuery query;query.observer_actor_id="teacher";query.place_id="town";
    const auto before_read=encode_common_state(f.common.state());
    auto page=query_household_provision_management(f.common,f.economy,f.catalog,catalog,management,control,index.value,query);assert(page.ok&&page.value.rows.size()==1&&page.value.rows[0].available_food==40&&page.value.rows[0].latest_ration->consumed==10);
    query.cursor=LocalEconomyCursor{index.value.identity,"teacher","town",std::nullopt,1};query.cursor->identity.execution_epoch++;
    assert(!query_household_provision_management(f.common,f.economy,f.catalog,catalog,management,control,index.value,query).ok);
    query.cursor.reset();query.observer_actor_id="student";assert(!query_household_provision_management(f.common,f.economy,f.catalog,catalog,management,control,index.value,query).ok);query.observer_actor_id="teacher";
    assert(compare_boundary(before_read,encode_common_state(f.common.state())).exact());
    command.consumption_limit.reset();assert(apply().ok);control.enabled=false;
    auto no_access=build_household_provision_view_index(f.common,f.economy,f.catalog,catalog,management,control);assert(no_access.ok&&no_access.value.authorization_ids.empty()&&no_access.value.latest_ration_by_authorization.empty());
    assert(!query_household_provision_management(f.common,f.economy,f.catalog,catalog,management,control,index.value,query).ok);
    const auto before_denied=encode_common_state(f.common.state());assert(!apply().ok&&compare_boundary(before_denied,encode_common_state(f.common.state())).exact());
    step(2);assert(ration(2).required==10&&ration(2).consumed==0&&ration(2).shortage==10&&food()==40&&f.economy.production_receipts.rows().back().second.effective_workers==0);
    const auto encoded=encode_household_provision_management_state(management);auto cold=decode_household_provision_management_state(encoded);assert(cold.ok&&validate_household_provision_management(f.common,f.economy,f.catalog,catalog,cold.value,control).ok);management=std::move(cold.value);
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);auto copy=management;assert(visit_household_provision_management_checkpoint(writer,copy).ok&&writer.finish().ok);const auto& payload=bytes;CodecVisitor reader(payload);HouseholdProvisionManagementState decoded;assert(visit_household_provision_management_checkpoint(reader,decoded).ok&&reader.finish().ok&&compare_boundary(encoded,encode_household_provision_management_state(decoded)).exact());
    auto bad=management;bad.configurations.erase(bad.configurations.rows().front().first);assert(!validate_household_provision_management(f.common,f.economy,f.catalog,catalog,bad,control).ok);
    step(3);assert(ration(3).required==10&&ration(3).consumed==10&&food()==30&&f.economy.production_receipts.rows().back().second.effective_workers==10);
    std::cout<<"household_provision_management: PASS (real ration limits, unchanged need, current control loss, accepted continuation, late rollback, strict cold source)\n";
}
