// One domain read-risk gate, using the existing real economy fixture. No World/Main matrix.
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/views/production_history.hpp"
int main(){
    Fixture fixture;fixture.common.state().metadata.player_actor_id="mayor";
    fixture.org_catalog.grants.find("city.grant")->allowed_command_kinds.push_back("inspect_local_economy");
    for(Integer tick=1;tick<=34;++tick)fixture.step(tick);
    fixture.validate();
    OrganizationAccess access(fixture.common,fixture.organizations,fixture.org_catalog,fixture.economy);
    ActingAuthority authority{"city","mayor.office",0,"mayor"};
    ProductionHistoryQuery query{"mayor","town","farm",authority,{}};
    const auto common_before=encode_common_state(fixture.common.state()),economy_before=encode_local_economy_state(fixture.economy);
    auto index=build_local_economy_index(fixture.common,fixture.economy,fixture.catalog,fixture.organizations,access);
    auto read=[&](const ProductionHistoryQuery& request){return query_production_history(fixture.common,fixture.economy,fixture.catalog,access,index,request);};
    auto first=read(query);assert(first.ok&&first.value.rows.size()==32&&first.value.next_cursor);
    assert(first.value.rows.front().due_tick==34&&first.value.rows.back().due_tick==3);
    const auto encoded=encode_production_history_page(first.value);
    for(const auto& row:boundary::array(*encoded.find("rows"),"/rows")){assert(!row.find("cause_fact_ids"));for(const char* field:{"inputs","outputs","unrealized_output"})for(const auto& quantity:boundary::array(*row.find(field),field))assert(!quantity.find("inventory_id"));}
    auto args=boundary::obj({{"place_id",BoundaryValue{"town"}},{"unit_id",BoundaryValue{"farm"}},{"authority",boundary::obj({{"organization_id",BoundaryValue{"city"}},{"office_id",BoundaryValue{"mayor.office"}},{"actor_id",BoundaryValue{"mayor"}},{"authority_epoch",BoundaryValue{Integer(0)}}})},{"cursor",*encoded.find("next_cursor")}});
    auto decoded=decode_production_history_query(args,"mayor");assert(decoded.ok);
    auto second=read(decoded.value);assert(second.ok&&second.value.rows.size()==2&&!second.value.next_cursor&&second.value.rows.back().due_tick==1);
    auto rejected=decoded.value;rejected.cursor->unit_id="other";assert(!read(rejected).ok);
    rejected=decoded.value;++rejected.cursor->source.identity.execution_epoch;assert(!read(rejected).ok);
    rejected=decoded.value;rejected.cursor->source.offset=-1;assert(!read(rejected).ok);
    rejected=query;rejected.authority.reset();assert(!read(rejected).ok);
    rejected=query;rejected.place_id="elsewhere";assert(!read(rejected).ok);
    rejected=query;rejected.observer_actor_id="teacher";assert(!read(rejected).ok);
    auto injected=args;injected.as<BoundaryValue::Dict>()->emplace_back("observer_actor_id",BoundaryValue{"teacher"});assert(!decode_production_history_query(injected,"mayor").ok);
    // Untrusted foreign/private records are projection-only counterexamples, never submitted banks.
    const auto original=fixture.economy.production_receipts.rows().front().second;
    for(int i=0;i<80;++i){auto hidden=original;hidden.id="hidden."+std::to_string(i);hidden.unit_id="unobservable.unit";fixture.economy.production_receipts.insert(hidden.id,hidden);}
    auto private_record=original;private_record.id="private.prior.receipt";private_record.inputs.push_back({"family.inventory","grain",1});
    fixture.economy.production_receipts.insert(private_record.id,private_record);
    fixture.common.state().facts.insert(private_record.id,{private_record.id,"local_production","local_economy",{}});
    index=build_local_economy_index(fixture.common,fixture.economy,fixture.catalog,fixture.organizations,access);
    auto after_hidden=read(query);assert(after_hidden.ok&&compare_boundary(encoded,encode_production_history_page(after_hidden.value)).exact());
    fixture.economy.production_receipts.erase(private_record.id);fixture.common.state().facts.erase(private_record.id);
    for(int i=0;i<80;++i)fixture.economy.production_receipts.erase("hidden."+std::to_string(i));
    // Acquiring the current unit does not unlock an original owner's history.
    fixture.economy.production_units.find("farm")->owner={Owner::Kind::actor,"mayor",{}};
    index=build_local_economy_index(fixture.common,fixture.economy,fixture.catalog,fixture.organizations,access);assert(!read(query).ok);
    fixture.economy.production_units.find("farm")->owner=fixture.catalog.production_units.find("farm")->owner;
    index=build_local_economy_index(fixture.common,fixture.economy,fixture.catalog,fixture.organizations,access);
    fixture.organizations.offices.find("mayor.office")->epoch=1;assert(!read(query).ok);fixture.organizations.offices.find("mayor.office")->epoch=0;
    assert(compare_boundary(common_before,encode_common_state(fixture.common.state())).exact());
    assert(compare_boundary(economy_before,encode_local_economy_state(fixture.economy)).exact());
    std::cout<<"production_history: PASS (real receipts, bounded lawful pagination, private sources and stale scopes rejected, no writes)\n";
}
