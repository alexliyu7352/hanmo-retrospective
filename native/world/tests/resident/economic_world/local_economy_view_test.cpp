// 复用既有真实经济夹具核权限、历史结果和非变；不建立第二套工资或生产实现。
#define main dynamic_economy_fixture_main
#include "../dynamic_economy_test.cpp"
#undef main
#include "resident/domains/local_economy/views/local_economy_view.hpp"
// 可见性从真实Grant与本人身份得出，成员或同地不能替代账簿权限。
void private_and_authorized_reads(){
    Fixture f;f.common.state().metadata.player_actor_id="teacher";f.step(1);
    OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);
    const auto before_common=encode_common_state(f.common.state()),before_economy=encode_local_economy_state(f.economy);
    auto page=query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"teacher","town",{}, {}});assert(page.ok&&page.value.rows.size()==1);
    const auto* arrear=std::get_if<LocalArrearView>(&page.value.rows[0]);assert(arrear&&arrear->original_due==20&&arrear->paid_amount==7&&arrear->outstanding==13);
    assert(compare_boundary(before_common,encode_common_state(f.common.state())).exact()&&compare_boundary(before_economy,encode_local_economy_state(f.economy)).exact());
    assert(!query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"mayor","town",{}, {}}).ok);
    assert(!query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"teacher","remote",{}, {}}).ok);
    f.common.state().metadata.player_actor_id="mayor";ActingAuthority authority{"city","mayor.office",0,"mayor"};
    assert(!query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"mayor","town",authority,{}}).ok);
    auto* grant=f.org_catalog.grants.find("city.grant");grant->allowed_command_kinds.push_back("inspect_local_economy");
    page=query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"mayor","town",authority,{}});assert(page.ok);
    bool found_production=false,found_arrear=false;for(const auto& row:page.value.rows){if(const auto* p=std::get_if<LocalProductionView>(&row)){found_production=true;assert(p->allocated_workers==10&&p->last_result&&p->last_result->batches==10&&p->next_due_tick==2);}found_arrear|=std::holds_alternative<LocalArrearView>(row);}assert(found_production&&!found_arrear);
    grant->scopes.push_back({ScopeRef::Kind::account,"city.wallet"});page=query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"mayor","town",authority,{}});assert(page.ok);for(const auto& row:page.value.rows)found_arrear|=std::holds_alternative<LocalArrearView>(row);assert(found_arrear);
    f.organizations.offices.find("mayor.office")->epoch=1;assert(!query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"mayor","town",authority,{}}).ok);
}
// 分页不重发生产或工资；冷状态与原游标产生相同页，身份变化必须重新读取。
void bounded_and_cold_reads(){
    Fixture f;f.common.state().metadata.player_actor_id="teacher";f.step(1);OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);
    for(int i=0;i<70;++i){auto original=f.economy.arrears.rows()[0].second;original.id="display_fixture."+std::to_string(i);f.economy.arrears.insert(original.id,original);}
    // 这里只扩大投影的合法来源记录；不把该分页夹具冒充生产可提交银行。
    const auto before=encode_local_economy_state(f.economy);auto page=query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"teacher","town",{}, {}});assert(page.ok&&page.value.rows.size()==32&&page.value.next_cursor&&page.value.scanned<=128);
    auto encoded=encode_local_economy_page(page.value);auto decoded=decode_local_economy_cursor(*encoded.find("next_cursor"));assert(decoded.ok);
    auto bad=*encoded.find("next_cursor");std::get<BoundaryValue::Dict>(bad.data).push_back({"unexpected",BoundaryValue{Integer(1)}});assert(!decode_local_economy_cursor(bad).ok);
    auto cold=decode_local_economy_state(before);assert(cold.ok);auto warm_page=query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"teacher","town",{},decoded.value});auto cold_page=query_local_economy(f.common,cold.value,f.catalog,access,build_local_economy_index(f.common,cold.value,f.catalog,f.organizations,access),{"teacher","town",{},decoded.value});assert(warm_page.ok&&cold_page.ok&&compare_boundary(encode_local_economy_page(warm_page.value),encode_local_economy_page(cold_page.value)).exact());
    auto stale=decoded.value;++stale.identity.execution_epoch;assert(!query_local_economy(f.common,f.economy,f.catalog,access,build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access),{"teacher","town",{},stale}).ok);
    assert(compare_boundary(before,encode_local_economy_state(f.economy)).exact());
}
// 远方不可见人口不能改变本人的分页形状，索引只保留合法来源序列。
void hidden_records_do_not_shape_pages(){
    Fixture f;f.common.state().metadata.player_actor_id="teacher";f.step(1);OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);
    auto index=build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access);auto first=query_local_economy(f.common,f.economy,f.catalog,access,index,{"teacher","town",{}, {}});assert(first.ok);
    for(int i=0;i<200;++i){auto hidden=*f.economy.cohorts.find("trainees");hidden.id="remote."+std::to_string(i);hidden.site_id="hidden_town";f.economy.cohorts.insert(hidden.id,hidden);}
    f.validate();auto rebuilt=build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access);auto after=query_local_economy(f.common,f.economy,f.catalog,access,rebuilt,{"teacher","town",{}, {}});assert(after.ok&&compare_boundary(encode_local_economy_page(first.value),encode_local_economy_page(after.value)).exact());assert(rebuilt.personal_sources.size()==1&&after.value.scanned==1&&!after.value.next_cursor);
    ++f.common.state().identity.revision;assert(!query_local_economy(f.common,f.economy,f.catalog,access,rebuilt,{"teacher","town",{}, {}}).ok);
}
// SDK形状只可选本人的现场；任意观察者、未知参数、伪造authority和旧页均拒绝。
void strict_player_arguments(){
    Fixture f;f.common.state().metadata.player_actor_id="teacher";OrganizationAccess access(f.common,f.organizations,f.org_catalog,f.economy);auto index=build_local_economy_index(f.common,f.economy,f.catalog,f.organizations,access);
    auto args=boundary::obj({{"place_id",BoundaryValue{StableId("town")}},{"cursor",BoundaryValue{}},{"authority",BoundaryValue{}}});auto query=decode_local_economy_query(args,"teacher");assert(query.ok&&query.value.observer_actor_id=="teacher"&&query_local_economy(f.common,f.economy,f.catalog,access,index,query.value).ok);
    auto remote=args;remote.find("place_id")->data=StableId("remote");auto remote_query=decode_local_economy_query(remote,"teacher");assert(remote_query.ok&&!query_local_economy(f.common,f.economy,f.catalog,access,index,remote_query.value).ok);
    auto actor=args;actor.as<BoundaryValue::Dict>()->push_back({"observer_actor_id",BoundaryValue{StableId("mayor")}});assert(!decode_local_economy_query(actor,"teacher").ok);
    auto malformed=args;malformed.find("cursor")->data=Integer{0};assert(!decode_local_economy_query(malformed,"teacher").ok);
    auto forged=args;forged.find("authority")->data=boundary::obj({{"organization_id",BoundaryValue{StableId("city")}},{"office_id",BoundaryValue{StableId("mayor.office")}},{"authority_epoch",BoundaryValue{Integer{0}}},{"actor_id",BoundaryValue{StableId("mayor")}}}).data;assert(!decode_local_economy_query(forged,"teacher").ok);
    assert(!decode_local_economy_query(args,"").ok);
}
int main(){strict_player_arguments();private_and_authorized_reads();bounded_and_cold_reads();hidden_records_do_not_shape_pages();std::cout<<"local_economy_view: PASS (private/account/site scopes, revoked authority, real production and arrear, immutable bounded query, strict cursor, cold read, hidden records preserve pagination)\n";}
