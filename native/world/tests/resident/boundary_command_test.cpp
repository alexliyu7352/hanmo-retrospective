// 原GD报价fixture只验证typed输出边界，不把预填报价当作native规则计算或性能结果。
#include "resident/kernel/command_types.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
using V=BoundaryValue;
// fixture文件保留原数值标签/Dictionary次序。
V load(const char* path) {std::ifstream in(path);std::string bytes((std::istreambuf_iterator<char>(in)),{});auto parsed=parse_boundary_json(bytes);assert(parsed.ok);return std::move(parsed.value);}
// 这些短读取器只用于测试期构造具体报价，不出现在生产规则中。
Integer number(const V& value,const char* key){auto p=value.find(key);assert(p&&p->as<Integer>());return *p->as<Integer>();}
StableId string(const V& value,const char* key){auto p=value.find(key);assert(p&&p->as<StableId>());return *p->as<StableId>();}
// 当前全部恢复/1000报价形状由typed领域owner给出，测试仅安装冻结返回值。
void verify_preview(const V& command,const V& raw) {
    const auto revision=number(raw,"revision");
    if(!*raw.find("ok")->as<bool>()){RuleError error;error.original.code=string(raw,"code");auto encoded=encode_resident_preview({false,{},error},revision);assert(encoded.ok&&compare_boundary(raw,encoded.value).exact());return;}
    const auto kind=string(command,"kind");auto decoded=decode_resident_command(kind,*command.find("params"));assert(decoded.ok);
    QuoteDetails details;details.kind=kind;
    if(raw.find("terrain")) {
        TravelQuote q;q.duration=number(raw,"duration");q.cost=number(raw,"cost");q.map_id=string(raw,"map_id");q.origin=string(raw,"origin");q.mode=string(raw,"mode");q.budget=number(raw,"budget");q.starting_load=number(raw,"starting_load");q.payer_wallet_id=string(raw,"payer_wallet_id");q.money_per_tick=number(raw,"money_per_tick");q.fatigue_per_tick=number(raw,"fatigue_per_tick");
        const auto& destination=*raw.find("destination");if(auto s=destination.as<StableId>())q.destination=*s;else {const auto& a=*destination.as<V::Array>();q.destination=PointI{*a[0].as<Integer>(),*a[1].as<Integer>()};}
        for(const auto& p:*raw.find("points")->as<V::Array>()){const auto& a=*p.as<V::Array>();q.points.push_back({*a[0].as<Integer>(),*a[1].as<Integer>()});}
        if(kind=="travel")details.data=q;else details.data=PreparedTravelOp{"actor.player",std::get<TravelCommandPayload>(decoded.value.data),q};
    } else if(kind.rfind("estate_",0)==0) {
        EstateQuote q;q.command=std::get<EstateCommand>(decoded.value.data);
        if(auto cash=raw.find("cash"))q.cash=EstateCashTerms{string(*cash,"from_wallet_id"),string(*cash,"to_wallet_id"),number(*cash,"quantity")};
        if(auto transfer=raw.find("transfer")){EstateTransferTerms t;t.from_inventory_id=string(*transfer,"from_inventory_id");t.to_inventory_id=string(*transfer,"to_inventory_id");t.item_id=string(*transfer,"item_id");t.quantity=number(*transfer,"quantity");t.source_id=string(*transfer,"source_id");for(const auto& owner:*transfer->find("owners")->as<V::Array>())t.owners.push_back(boundary::decode_owner(owner,"fixture"));t.new_owner=boundary::decode_owner(*transfer->find("new_owner"),"fixture");q.transfer=t;}details.data=q;
    } else if(std::holds_alternative<InteractionCommand>(decoded.value.data)) {
        InteractionQuote q;if(auto x=raw.find("duration"))q.duration=*x->as<Integer>();for(auto p:std::initializer_list<std::pair<const char*,StableId*>>{{"encounter_id",&q.encounter_id},{"dialogue_id",&q.dialogue_id},{"next",&q.next}})if(auto x=raw.find(p.first))*p.second=*x->as<StableId>();if(auto context=raw.find("context"))q.context=interaction_codec::decode_context(*context,"fixture");details.data=q;
    } else if(kind=="pause_travel"||kind=="resume_travel"||kind=="travel_wait") details.data=PreparedTravelOp{"actor.player",std::get<TravelCommandPayload>(decoded.value.data),{}};
    else {DurationQuote q;if(raw.find("duration"))q.duration=number(raw,"duration");details.data=q;}
    auto operation=std::make_shared<PreparedDomainOp>();operation->command=decoded.value;operation->quote=details;
    auto preview=std::make_shared<PreviewResult>();preview->kind=kind;preview->quote=std::make_shared<QuoteDetails>(details);preview->operation=operation;
    auto encoded=encode_resident_preview({true,preview,{}},revision);
    if(!encoded.ok){std::cerr<<kind<<": "<<encoded.error.original.code<<"\n";assert(false);}
    const auto comparison=compare_boundary(raw,encoded.value);if(!comparison.exact()){std::cerr<<kind<<": mismatch "<<(comparison.value_difference?comparison.value_difference->path:comparison.order_difference->path)<<"\n";assert(false);}
}
int main(int argc,char** argv) {
    assert(argc==3);auto index=load(argv[1]);int count=0;
    for(const auto& row:*index.find("commands")->as<V::Array>()){verify_preview(*row.find("command"),*row.find("preview"));++count;}
    auto recovery=load(argv[2]);for(const auto& c:*recovery.find("cases")->as<V::Array>())for(const auto& row:*c.find("trace")->as<V::Array>()){verify_preview(*row.find("command"),*row.find("preview"));++count;}
    auto empty=V{V::Dict{}};auto travel=decode_resident_command("travel",empty);assert(travel.ok&&std::get<StableId>(std::get<TravelCommand>(std::get<TravelCommandPayload>(travel.value.data)).destination).empty());
    auto malformed=parse_boundary_json(R"({"destination":true,"via":{}})");auto deferred=decode_resident_command("travel",malformed.value);assert(deferred.ok&&std::get<TravelCommand>(std::get<TravelCommandPayload>(deferred.value.data)).via.size()==9);
    auto waiting=decode_resident_command("wait",V{V::Dict{{"ignored",{Integer{1}}}}});assert(!waiting.ok&&waiting.error.kind==ErrorKind::unsupported_capability);
    auto unknown=decode_resident_command("estate_unknown",empty);assert(!unknown.ok&&unknown.error.original.code=="UnsupportedAction");
    auto encounter=decode_resident_command("encounter_leave",empty);assert(encounter.ok&&std::get<EncounterLeave>(std::get<InteractionCommand>(encounter.value.data)).encounter_id.empty());
    std::cout<<"real GD preview boundary fixtures passed: "<<count<<"; rule execution not tested here\n";
}
