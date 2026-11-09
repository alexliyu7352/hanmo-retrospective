// 单一Flecs关联查询的纯typed候选门；独立decoder链接桩不进入产品，不冒真实Schema/wire闭环。
#include "hanmo/legacy_projection.hpp"
#include "typed_wire.hpp"
#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
#ifndef HANMO_TYPED_FIXTURE_DECODER
#error "本测试必须使用独立typed fixture target，不能把桩链接进产品"
#endif
namespace {
// 仅本测试进程的一次C++分配故障；不用改生产源码或第三方分配器。
int fail_after_allocations=-1;
bool allocation_fault_triggered=false;
}
// 下一次候选C++分配精确失败一次，异常构造与报告不再受扰动。
void* operator new(std::size_t size) {
    if(fail_after_allocations>=0 && fail_after_allocations--==0) { allocation_fault_triggered=true; throw std::bad_alloc{}; }
    if(void* memory=std::malloc(size?size:1)) return memory;
    throw std::bad_alloc{};
}
// 测试分配器对应释放，不保留全局对象或干扰Flecs自身malloc策略。
void operator delete(void* pointer) noexcept { std::free(pointer); }
// 编译器按对象大小释放时仍交同一测试malloc配对。
void operator delete(void* pointer,std::size_t) noexcept { std::free(pointer); }
namespace {
using namespace hanmo;
using namespace hanmo::detail;
using D=Value::Dict;
using A=Value::Array;
LegacyProjectionInput candidate;
std::vector<ActorCommitmentsQuery> requested;
std::map<std::string,std::map<std::string,Value>> originals;
int checks=0,failures=0;
std::string observed_source;
bool fail_after_decode=false;
// 显式String值与整数值区分，fixture不调用产品record重组生成期望。
Value text(const std::string& value) { return Value{value}; }
// 整数fixture直接为int64，未知扩展可覆盖承载宽度边界。
Value number(std::int64_t value) { return Value{value}; }
// 产权是完整值，附加键不被测试降成kind/id。
Value owner(const std::string& kind,const std::string& id) { return Value{D{{"kind",text(kind)},{"id",text(id)}}}; }
// 断言汇总决定退出码，错误信息标明本门纯typed范围。
void check(bool ok,const std::string& label) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<label<<'\n'; } }
// 原始期望记录逐项显式构造，不经C的record函数或B的decoder。
void fixture() {
    candidate={}; requested={{"second",{"actor:1","a"}},{"first",{}},{"idle",{"idle"}}}; originals.clear();
    candidate.source={{"tick",number(8)},{"revision",number(12)}};
    candidate.provenance={{"source_commit",text(std::string(40,'b'))}};
    candidate.references={{"catalog_actors",{"unactivated","prototype"}},{"parties",{"party:1"}},{"armies",{"army:1"}},{"tasks",{"task:1"}}};
    candidate.coverage=Value{A{Value{D{{"scope",text("deferred")},{"name",text("history")}}}}};
    originals["metadata"]["source"]=Value{candidate.source}; originals["metadata"]["provenance"]=Value{candidate.provenance}; originals["metadata"]["coverage"]=candidate.coverage;
    originals["metadata"]["references"]=Value{D{{"catalog_actors",Value{A{text("prototype"),text("unactivated")}}},{"parties",Value{A{text("party:1")}}},{"armies",Value{A{text("army:1")}}},{"tasks",Value{A{text("task:1")}}}}};
    for(const std::string id:{"actor:1","a","idle"}) {
        ActorState actor; actor.id=id; actor.wallet_id="wallet:"+id; actor.inventory_id="bag:"+id; actor.action_id=id=="idle"?"":"action:1";
        actor.alive=true; actor.available=true; actor.hp=100; actor.fatigue=9; actor.merit=1;
        actor.location=Value{D{{"kind",text(id=="a"?"Following":"InArmy")},{id=="a"?"party_id":"army_id",text(id=="a"?"party:1":"army:1")}}};
        actor.extras={{"attributes",Value{D{{"strength",number(12)}}}},{"note",Value{A{number(std::numeric_limits<std::int64_t>::min()),text("1")}}}};
        D raw={{"id",text(id)},{"wallet_id",text(actor.wallet_id)},{"inventory_id",text(actor.inventory_id)},{"action_id",text(actor.action_id)},{"alive",Value{true}},{"available",Value{true}},{"hp",number(100)},{"fatigue",number(9)},{"merit",number(1)},{"location",actor.location},{"attributes",Value{D{{"strength",number(12)}}}},{"note",Value{A{number(std::numeric_limits<std::int64_t>::min()),text("1")}}}};
        if(id=="actor:1") { actor.prototype_id="prototype";actor.display_name="真人";actor.birth_tick=-10;actor.created_tick=0;raw.emplace("prototype_id",text("prototype"));raw.emplace("display_name",text("真人"));raw.emplace("birth_tick",number(-10));raw.emplace("created_tick",number(0)); }
        originals["actors"][id]=Value{raw}; candidate.actors.push_back(actor);
        WalletState wallet;wallet.id=actor.wallet_id;wallet.owner=owner("actor",id);wallet.balance=100;
        if(id=="a")wallet.reserved={{"action:1",0}};
        wallet.extras={{"extra",number(4)}};
        D reserved;if(id=="a")reserved={{"action:1",number(0)}};
        originals["wallets"][wallet.id]=Value{D{{"id",text(wallet.id)},{"owner",wallet.owner},{"balance",number(100)},{"reserved",Value{reserved}},{"extra",number(4)}}};candidate.wallets.push_back(wallet);
        InventoryState inventory;inventory.id=actor.inventory_id;inventory.owner=owner("actor",id);inventory.capacity=100;inventory.revision=2;inventory.extras={{"note",text("original")}};
        A raw_lots;
        if(id=="a") {
            Value task_owner=owner("task","task:1");std::get<D>(task_owner.data).emplace("nested",Value{A{number(1),text("1")}});
            for(const auto& row:std::vector<std::vector<std::string>>{{"lot:z","3",""},{"lot:a","2",""},{"lot:r","10","action:1"}}) {
                Lot lot;lot.id=row[0];lot.item_id="grain";lot.quantity=std::stoll(row[1]);lot.owner=row[0]=="lot:a"?owner("actor","a"):task_owner;lot.reserved_by=row[2];lot.extras={{"extra",Value{A{text("later"),text("earlier")}}}};
                inventory.lots.push_back(lot);raw_lots.push_back(Value{D{{"id",text(lot.id)},{"item_id",text("grain")},{"quantity",number(lot.quantity)},{"owner",lot.owner},{"reserved_by",text(lot.reserved_by)},{"extra",Value{A{text("later"),text("earlier")}}}}});
            }
        }
        originals["inventories"][inventory.id]=Value{D{{"id",text(inventory.id)},{"owner",inventory.owner},{"capacity",number(100)},{"revision",number(2)},{"lots",Value{raw_lots}},{"note",text("original")}}};candidate.inventories.push_back(inventory);
    }
    WalletState public_wallet;public_wallet.id="a-public";public_wallet.owner=owner("organization","org");public_wallet.balance=9;public_wallet.reserved={{"action:1",7}};
    originals["wallets"][public_wallet.id]=Value{D{{"id",text(public_wallet.id)},{"owner",public_wallet.owner},{"balance",number(9)},{"reserved",Value{D{{"action:1",number(7)}}}}}};candidate.wallets.push_back(public_wallet);
    LegacyActivity activity;activity.id="action:1";activity.kind="army_form";activity.actor_id="actor:1";activity.participants={"actor:1","a"};activity.start_tick=7;activity.remaining_ticks=3;
    activity.params={{"paused",Value{true}},{"path",Value{A{text("second"),text("first")}}}};activity.extras={{"legacy_extension",text("preserved")}};
    originals["activities"][activity.id]=Value{D{{"id",text("action:1")},{"kind",text("army_form")},{"actor_id",text("actor:1")},{"participants",Value{A{text("actor:1"),text("a")}}},{"start_tick",number(7)},{"remaining_ticks",number(3)},{"params",Value{activity.params}},{"legacy_extension",text("preserved")}}};candidate.activities.push_back(activity);
    OrganizationDefinition organization;organization.id="org";organization.name_key="org.name";organization.kind="clan";organization.leader_actor="unactivated";organization.extras={{"note",number(5)}};
    originals["organizations"]["org"]=Value{D{{"id",text("org")},{"name_key",text("org.name")},{"kind",text("clan")},{"leader_actor",text("unactivated")},{"note",number(5)}}};candidate.organizations.push_back(organization);
    for(const std::string id:{"office:z","office:a"}) {
        LegacyOffice office;office.id=id;office.holder_actor_id="actor:1";office.enabled=id=="office:a";office.appointment_id=office.enabled?"appointment":"legacy.missing";office.extras={{"note",text("old")}};
        office.definition={{"id",text(id)},{"organization_id",text("org")},{"qualification",Value{D{{"unknown",text("untouched")}}}}};
        originals["offices"][id]=Value{D{{"state",Value{D{{"id",text(id)},{"holder_actor_id",text("actor:1")},{"enabled",Value{office.enabled}},{"appointment_id",text(office.appointment_id)},{"note",text("old")}}}},{"definition",Value{office.definition}}}};candidate.offices.push_back(office);
    }
    for(const std::string id:{"actor:1","appointment"}) {
        LegacyFact fact;fact.id=id;fact.kind=id=="actor:1"?"actor_creation":"military_action";fact.extras={{"actor_id",text("actor:1")},{"success",Value{true}}};
        const D raw_fact={{"kind",text(fact.kind)},{"actor_id",text("actor:1")},{"success",Value{true}}};
        originals["facts"][id]=Value{D{{"id",text(id)},{"record",Value{raw_fact}}}};candidate.facts.push_back(fact);
    }
    ItemDefinition item;item.id="grain";item.weight=1;item.extras={{"name_key",text("grain.name")}};
    originals["items"][item.id]=Value{D{{"id",text("grain")},{"weight",number(1)},{"name_key",text("grain.name")}}};candidate.items.push_back(item);
}
}
namespace hanmo::detail {
// 仅测试target用纯值候选替换decoder；生产hanmo_world绝不包含这个符号实现。
Result<LegacyProjectionInput> decode_legacy_artifact(const std::string& bytes) {
    auto result=candidate; result.input_sha256=sha256(bytes); result.payload_sha256=std::string(64,'a');
    // 本固定fixture的路由JSON析构先分配一次；越过该noexcept第三方边界再命中自有候选。
    fail_after_allocations=fail_after_decode?1:-1; fail_after_decode=false;
    return {true,std::move(result),{}};
}
// 测试明确拆开wire边界；记录实际store来源并提供已解码batch，真实JSON门归B。
Result<std::vector<ActorCommitmentsQuery>> decode_actor_queries(const std::string& bytes,const std::string& source) {
    observed_source=source;
    if(bytes=="bad")return {false,{}, {"FixtureQueryRejected","/queries"}};
    return {true,requested,{}};
}
}
// 原型、任务产权、任职历史和原序均从实际Flecs组件查询，不能直接返回上述候选JSON。
int main() {
    fixture();hanmo::ProjectionStore store;
    const std::string bytes="{\"format\":\"HM-NATIVE-LEGACY-ARTIFACT/1\"}";
    const auto installed=store.install_once(bytes);check(installed.ok,"纯typed候选实际Flecs安装");if(!installed.ok)return 1;
    const auto audit=store.audit();check(audit.profile=="HM-NATIVE-LEGACY-ARTIFACT/1" && audit.install_count==1,"新profile一次安装");
    for(const auto& [domain,rows]:originals)for(const auto& [id,original]:rows)check(audit.record_component_sha256.at(domain).at(id)==sha256(encode_value(original).dump()),"实际组件完整重组 "+domain+"/"+id);
    check(audit.lot_ids.at("bag:a")==std::vector<std::string>({"lot:z","lot:a","lot:r"}) && audit.lot_ids.at("bag:idle").empty(),"完整原lot序与空库存");
    check(audit.component_sha256.at("bag:a")==audit.record_component_sha256.at("inventories").at("bag:a"),"两审计口调用唯一库存重组");
    check(audit.record_component_sha256.at("actors").count("actor:1") && audit.record_component_sha256.at("facts").count("actor:1"),"Actor和Fact同ID分域保全");
    auto result=store.actor_commitments("ok");check(result.ok && result.value.size()==3,"完整batch返回");if(!result.ok)return 1;
    check(observed_source==sha256(bytes),"查询decoder收到实际安装artifact SHA");
    check(result.value[0].query_id=="second" && result.value[1].query_id=="first" && result.value[1].actors.empty(),"query顺序不排序且空列表为空");
    const auto& rows=result.value[0].actors;check(rows.size()==2 && rows[0].actor_id=="a" && rows[1].actor_id=="actor:1","actor外层按ID排序");
    check(rows[0].location==candidate.actors[1].location && rows[1].location==candidate.actors[0].location,"Following/InArmy原值不解析");
    check(rows[0].wallet_id=="wallet:a" && rows[0].inventory_id=="bag:a","实际私人资产引用");
    for(const auto& row:rows) {
        check(row.current_activity && row.current_activity->actor_id=="actor:1" && row.current_activity->participants==std::vector<std::string>({"actor:1","a"}),"participant原序且不冒principal");
        check(row.current_activity->start_tick==7 && row.current_activity->remaining_ticks==3 && row.current_activity->params==candidate.activities[0].params && row.current_activity->extras.empty(),"保七字段/暂停原值不造due或dump extras");
        check(row.wallet_reservations.size()==2 && row.wallet_reservations[0].wallet_id=="a-public" && row.wallet_reservations[0].amount==7 && row.wallet_reservations[1].amount==0,"所有钱包关联、外层排序与零预留");
        check(row.goods_reservations.size()==1 && row.goods_reservations[0].inventory_id=="bag:a" && row.goods_reservations[0].lot_index==2 && row.goods_reservations[0].lot.id=="lot:r","只取当前reserved批且保原下标");
        check(row.goods_reservations[0].lot.owner==candidate.inventories[1].lots[2].owner && row.goods_reservations[0].container_owner==owner("actor","a") && row.goods_reservations[0].lot.extras==candidate.inventories[1].lots[2].extras,"任务full owner/lot extras与容器产权分开");
    }
    check(rows[1].offices.size()==2 && rows[1].offices[0].office_id=="office:a" && rows[1].offices[0].enabled && rows[1].offices[0].appointment_resolved,"enabled职务关联已存fact且办公室排序");
    check(!rows[1].offices[1].enabled && !rows[1].offices[1].appointment_resolved && rows[1].offices[1].appointment_id=="legacy.missing","disabled缺历史fact仍保原ID无授权");
    check(rows[1].offices[0].organization.leader_actor==std::optional<std::string>{"unactivated"},"目录领袖未激活不补真人");
    const auto& idle=result.value[2].actors[0];check(!idle.current_activity && idle.wallet_reservations.empty() && idle.goods_reservations.empty(),"无当前行动为空不推付款");
    result.value[0].actors[0].current_activity->participants.clear(); result.value[0].actors[0].goods_reservations[0].lot.extras.clear();result.value[0].actors[1].offices[0].organization.extras.clear();std::get<D>(result.value[0].actors[0].location.data).clear();
    auto repeated=store.actor_commitments("ok");check(repeated.ok && repeated.value[0].actors[0].current_activity->participants.size()==2 && !repeated.value[0].actors[0].goods_reservations[0].lot.extras.empty() && !repeated.value[0].actors[1].offices[0].organization.extras.empty(),"结果嵌套修改无别名");
    candidate.actors.clear();candidate.wallets.clear();candidate.inventories.clear();candidate.activities.clear();candidate.facts.clear();
    check(store.audit().record_component_sha256==audit.record_component_sha256 && store.actor_commitments("ok").value[0].actors.size()==2,"清除解码候选不影响实际已装组件");
    requested={{"good",{"a"}},{"bad",{"missing"}}};const auto bad=store.actor_commitments("ok");check(!bad.ok && bad.value.empty() && bad.error.code=="UnknownActor" && bad.error.path=="missing","后条未知actor整批无部分结果");
    check(store.actor_commitments("bad").error.code=="FixtureQueryRejected","先完整decoder失败后才查未知actor");
    check(store.install_once("{").error.code=="AlreadyInstalled","二次安装优先且组件不动");
    // 候选解码成功后、Flecs实例发布之前的确定性分配失败必须保持完整空态。
    fixture();hanmo::ProjectionStore recovering;allocation_fault_triggered=false;fail_after_decode=true;
    const auto failed_install=recovering.install_once(bytes);const auto empty_audit=recovering.audit();
    check(allocation_fault_triggered && !failed_install.ok && failed_install.error.code=="ProjectionResourceLimit" && failed_install.error.path=="/runtime","候选构建分配失败给明确原运行错误");
    check(!empty_audit.installed && empty_audit.install_count==0 && empty_audit.input_sha256.empty() && empty_audit.record_component_sha256.empty() && empty_audit.lot_ids.empty(),"构建失败无组件/身份半安装");
    check(recovering.actor_commitments("ok").error.code=="NotInstalled" && recovering.install_once(bytes).ok,"同对象构建失败后可重试一次");
    check(recovering.actor_commitments("ok").ok && recovering.install_once(bytes).error.code=="AlreadyInstalled","重试真查询可读且成功后不重装");
    // 第二物理容器有非ID排序的两个匹配批次，夹着无预留lot；结果只保相对原序。
    fixture();InventoryState another=candidate.inventories[1];another.id="a-first-container";
    Lot first=another.lots[2];first.id="lot:zz";Lot skipped=another.lots[1];skipped.id="lot:skip";Lot last=another.lots[2];last.id="lot:aa";
    another.lots={first,skipped,last};candidate.inventories.push_back(another);
    hanmo::ProjectionStore ordered;check(ordered.install_once("{\"format\":\"HM-NATIVE-LEGACY-ARTIFACT/1\",\"fixture\":\"order\"}").ok,"独立排序候选安装");
    const auto ordered_result=ordered.actor_commitments("ok");const auto& goods=ordered_result.value[0].actors[0].goods_reservations;
    check(goods.size()==3 && goods[0].inventory_id=="a-first-container" && goods[0].lot_index==0 && goods[0].lot.id=="lot:zz" && goods[1].inventory_id=="a-first-container" && goods[1].lot_index==2 && goods[1].lot.id=="lot:aa" && goods[2].inventory_id=="bag:a" && goods[2].lot_index==2,"只排外层库存且保匹配lot原下标/相对序");
    check(ordered.audit().lot_ids.at("a-first-container")==std::vector<std::string>({"lot:zz","lot:skip","lot:aa"}),"组件audit完整保未占用批次位置");
    std::cout<<"ACTOR_COMMITMENTS_TYPED_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
