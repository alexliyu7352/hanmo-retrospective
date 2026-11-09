// 旧军政一步提案的纯算法；原领域提供窄事实，所有分支与BFS仍在此处真实执行。
#include "hanmo/military_schedule.hpp"
#include "typed_wire.hpp"
#include <optional>

namespace hanmo {
namespace {
using detail::Json;using detail::at;using detail::fail;using Value=detail::Value;
using Params=Value::Dict;using Proposal=MilitaryScheduleProposal;
using Table=std::map<std::string,const Json*>;
// 闭合对象和字段类型仅核wire，不复查世界Schema、军权或资产规则。
void object(const Json& v,const std::string& path,std::initializer_list<const char*> keys) {
    if(!v.is_object())fail("NativeMilitaryInvalidValue",path);
    detail::fields(v,keys,{},path);
}
// 数组原序与重复值均保留；只拒绝协议之外的容器类型。
void array(const Json& v,const std::string& path) {if(!v.is_array())fail("NativeMilitaryInvalidValue",path);}
// 已核字符串只按原值取出，不替换运行身份或清洗缺失引用。
std::string text(const Json& v,const char* key){return v.at(key).get<std::string>();}
// 字符串字段一次性校验，算法后续不作宽松转换。
void texts(const Json& v,const std::string& path,std::initializer_list<const char*> keys){for(auto key:keys)(void)detail::json_string(v.at(key),at(path,key));}
// Godot布尔必须保bool，不接受整数真值。
void booleans(const Json& v,const std::string& path,std::initializer_list<const char*> keys){for(auto key:keys)if(!v.at(key).is_boolean())fail("NativeMilitaryInvalidValue",at(path,key));}
// 原整数使用规范有符号int64编码，未套钱粮或已提交世界范围门。
std::int64_t integer(const Json& v,const std::string& path){return detail::decimal(detail::json_string(v,path),path);}
// 三项实际角色Context只机械转纯值，选择报告办理人仍由原生分支完成。
void context(const Json& v,const std::string& path){object(v,path,{"place_id","role_id","provider_actor_id"});texts(v,path,{"place_id","role_id","provider_actor_id"});}
// 作者数组不去重，军团编组unit_ids必须按原顺序输出。
void strings(const Json& v,const std::string& path){array(v,path);for(std::size_t i=0;i<v.size();++i)(void)detail::json_string(v[i],at(path,std::to_string(i)));}
// 每个真实campaign只携带领域查询结果，不接收GD计算好的命令或移动步骤。
void campaign_fields(const Json& v,const std::string& path){
    object(v,path,{"id","actor_id","template_id","status","army_id","office_id","office_enabled","office_holder_actor_id","objective_result_nonempty","pending_siege","return_choice","report_roles"});
    texts(v,path,{"id","actor_id","template_id","status","army_id","office_id","office_holder_actor_id"});booleans(v,path,{"office_enabled","objective_result_nonempty"});
    const auto& siege=v.at("pending_siege");const auto sp=at(path,"pending_siege");object(siege,sp,{"id","phase","player_siege_id"});texts(siege,sp,{"id","phase","player_siege_id"});
    const auto& choice=v.at("return_choice");const auto cp=at(path,"return_choice");object(choice,cp,{"ok","garrison_id","place_id"});booleans(choice,cp,{"ok"});texts(choice,cp,{"garrison_id","place_id"});
    const auto& roles=v.at("report_roles");const auto rp=at(path,"report_roles");array(roles,rp);
    for(std::size_t i=0;i<roles.size();++i){const auto p=at(rp,std::to_string(i));object(roles[i],p,{"role_id","settlement_id","context"});texts(roles[i],p,{"role_id","settlement_id"});context(roles[i].at("context"),at(p,"context"));}
}
// 整行闭合类型检查不会代替原资格过滤，也不限制正常helper失败或空角色。
void actor_fields(const Json& v,const std::string& path){
    object(v,path,{"actor_id","alive","available","action_id","combat_occupied","is_static","location","plan","office","order","campaigns"});
    texts(v,path,{"actor_id","action_id"});booleans(v,path,{"alive","available","combat_occupied","is_static"});
    const auto& location=v.at("location");const auto lp=at(path,"location");object(location,lp,{"kind","place_id","settlement_id"});texts(location,lp,{"kind","place_id","settlement_id"});
    const auto& plan=v.at("plan");const auto pp=at(path,"plan");object(plan,pp,{"office_id","order_id","unit_ids","food_quantity","money"});texts(plan,pp,{"office_id","order_id"});strings(plan.at("unit_ids"),at(pp,"unit_ids"));(void)integer(plan.at("food_quantity"),at(pp,"food_quantity"));(void)integer(plan.at("money"),at(pp,"money"));
    const auto& office=v.at("office");const auto op=at(path,"office");object(office,op,{"enabled","holder_actor_id","appointment_id","garrison_place_id","appointing_context"});booleans(office,op,{"enabled"});texts(office,op,{"holder_actor_id","appointment_id","garrison_place_id"});context(office.at("appointing_context"),at(op,"appointing_context"));
    const auto& order=v.at("order");const auto dp=at(path,"order");object(order,dp,{"issuer_context","objective_kind"});texts(order,dp,{"objective_kind"});context(order.at("issuer_context"),at(dp,"issuer_context"));
    const auto& campaigns=v.at("campaigns");const auto cp=at(path,"campaigns");array(campaigns,cp);for(std::size_t i=0;i<campaigns.size();++i)campaign_fields(campaigns[i],at(cp,std::to_string(i)));
}
// 返回中的Context保持原String键值，不经任意JSON到Variant转换。
Value context_value(const Json& v){return Value{Params{{"place_id",Value{text(v,"place_id")}},{"role_id",Value{text(v,"role_id")}},{"provider_actor_id",Value{text(v,"provider_actor_id")}}}};}
// 固定分支构造唯一命令，actor_id最后由外层原序循环补入。
Proposal command(const std::string& kind,Params params){return {"",kind,std::move(params)};}
// 只在原算法实际访问时拒绝缺图节点；未走的坏引用不会改变早退。
const Json& place(const Table& places,const std::string& id){const auto found=places.find(id);if(found==places.end())fail("NativeMilitaryMissingPlace",at("/military_schedule/places",id));return *found->second;}
// 城内BFS保passages顺序与第一次访问，返回首段而非整条路径或瞬移。
std::optional<Proposal> walk(const Json& actor,const Table& places,const std::string& target){
    const auto start=text(actor.at("location"),"place_id");if(start.empty() || start==target)return {};
    std::vector<std::string> pending{start};std::map<std::string,std::string> first{{start,""}};
    for(std::size_t cursor=0;cursor<pending.size();++cursor){const auto here=pending[cursor];for(const auto& value:place(places,here).at("passages")){
        const auto next=value.get<std::string>();if(first.count(next))continue;
        first[next]=here==start?next:first.at(here);if(next==target)return command("move_local",{{"place_id",Value{first.at(next)}}});pending.push_back(next);
    }}return {};
}
// 目标存在和place等值判断先于kind；原入口行走、跨城旅行与不支持位置顺序不变。
std::optional<Proposal> at_or_walk(const Json& actor,const Table& places,const Table& settlements,const std::string& target,const std::string& kind,Params params){
    if(!places.count(target))return {};
    const auto& location=actor.at("location");const auto current=text(location,"place_id");if(current==target)return command(kind,std::move(params));
    const auto city=text(place(places,target),"settlement_id");const auto location_kind=text(location,"kind");
    if(location_kind=="Settlement" && text(location,"settlement_id")==city)return walk(actor,places,target);
    if(location_kind=="Settlement"){
        const auto source=text(location,"settlement_id");const auto found=settlements.find(source);if(found==settlements.end())fail("NativeMilitaryMissingSettlement",at("/military_schedule/settlements",source));
        const auto entry=text(*found->second,"entry_place");if(current!=entry)return walk(actor,places,entry);
    }
    if(location_kind=="Settlement" || location_kind=="Wilderness")return command("travel",{{"destination",Value{city}}});
    return {};
}
// 旧_next依次消费领域事实；返程/报告优先于现任军权，正常无友营或无角色仅等待。
std::optional<Proposal> next(const Json& actor,const Json* campaign,const Table& places,const Table& settlements,const std::string& path){
    const auto actor_id=text(actor,"actor_id");
    if(campaign){
        const auto& siege=campaign->at("pending_siege");const auto siege_id=text(siege,"id");
        if(!siege_id.empty()){
            if(text(siege,"phase")=="result" && text(siege,"player_siege_id")!=siege_id)return command("siege_close",{{"siege_id",Value{siege_id}}});
            return {};
        }
        const auto status=text(*campaign,"status");
        if(status=="returning"){
            const auto& choice=campaign->at("return_choice");if(!choice.at("ok").get<bool>())return {};
            return at_or_walk(actor,places,settlements,text(choice,"place_id"),"army_return",{{"army_id",Value{text(*campaign,"army_id")}},{"garrison_id",Value{text(choice,"garrison_id")}}});
        }
        if(status=="awaiting_report"){
            const auto& roles=campaign->at("report_roles");if(roles.empty())return {};
            const Json* chosen=&roles[0];const auto city=text(actor.at("location"),"settlement_id");
            for(const auto& role:roles)if(text(role,"settlement_id")==city){chosen=&role;break;}
            const auto& ctx=chosen->at("context");return at_or_walk(actor,places,settlements,text(ctx,"place_id"),"campaign_report",{{"campaign_id",Value{text(*campaign,"id")}},{"context",context_value(ctx)}});
        }
        if(!campaign->at("office_enabled").get<bool>() || text(*campaign,"office_holder_actor_id")!=actor_id)return command(text(*campaign,"army_id").empty()?"campaign_cancel":"campaign_return",{{"campaign_id",Value{text(*campaign,"id")}}});
    }
    const auto& plan=actor.at("plan");const auto& office=actor.at("office");
    if(!office.at("enabled").get<bool>()){
        if(!actor.at("is_static").get<bool>() || !text(office,"appointment_id").empty())return {};
        const auto& ctx=office.at("appointing_context");return at_or_walk(actor,places,settlements,text(ctx,"place_id"),"office_accept",{{"office_id",Value{text(plan,"office_id")}},{"context",context_value(ctx)}});
    }
    if(text(office,"holder_actor_id")!=actor_id)return {};
    const auto& order=actor.at("order");
    if(!campaign){const auto& ctx=order.at("issuer_context");return at_or_walk(actor,places,settlements,text(ctx,"place_id"),"campaign_accept",{{"template_id",Value{text(plan,"order_id")}},{"context",context_value(ctx)}});}
    const auto status=text(*campaign,"status");
    if(status=="accepted"){
        Value::Array units;for(const auto& unit:plan.at("unit_ids"))units.push_back(Value{unit.get<std::string>()});
        Params params{{"order_id",Value{text(*campaign,"id")}}, {"unit_ids",Value{std::move(units)}},
            {"food_quantity",Value{integer(plan.at("food_quantity"),at(at(path,"plan"),"food_quantity"))}},
            {"money",Value{integer(plan.at("money"),at(at(path,"plan"),"money"))}}};
        return at_or_walk(actor,places,settlements,text(office,"garrison_place_id"),"army_form",std::move(params));
    }
    if(status=="mustered"){
        const auto city=text(place(places,text(office,"garrison_place_id")),"settlement_id");const auto found=settlements.find(city);if(found==settlements.end())fail("NativeMilitaryMissingSettlement",at("/military_schedule/settlements",city));
        return at_or_walk(actor,places,settlements,text(*found->second,"entry_place"),"campaign_depart",{{"campaign_id",Value{text(*campaign,"id")}}});
    }
    if(status=="operating"){
        if(campaign->at("objective_result_nonempty").get<bool>())return command("campaign_return",{{"campaign_id",Value{text(*campaign,"id")}}});
        const auto objective=text(order,"objective_kind");if(objective=="deliver_supply")return command("campaign_deliver",{{"campaign_id",Value{text(*campaign,"id")}}});
        if(objective=="occupy")return command("siege_start",{{"campaign_id",Value{text(*campaign,"id")}}});
    }return {};
}
} // 所有读集与BFS容器只活在本次调用，不形成跨半日缓存。

// 整份协议先核类型，再保原人物/军令顺序计算；任何失败销毁整批局部结果。
Result<MilitaryScheduleBatch> military_schedule_proposals(const std::string& bytes){
    try{
        const std::string path="/military_schedule";const auto input=detail::parse(bytes,path);
        object(input,path,{"format","scope_token","player_actor_id","actors","places","settlements"});texts(input,path,{"format","scope_token","player_actor_id"});
        if(text(input,"format")!="HM-NATIVE-MILITARY-SCHEDULE/1")fail("NativeMilitaryUnsupportedFormat",at(path,"format"));
        const auto& actors=input.at("actors");array(actors,at(path,"actors"));std::set<std::string> ids;
        for(std::size_t i=0;i<actors.size();++i){const auto p=at(at(path,"actors"),std::to_string(i));actor_fields(actors[i],p);if(!ids.insert(text(actors[i],"actor_id")).second)fail("NativeMilitaryDuplicateActor",at(p,"actor_id"));}
        Table places,settlements;
        for(auto key:{"places","settlements"}){
            const auto& rows=input.at(key);array(rows,at(path,key));for(std::size_t i=0;i<rows.size();++i){const auto p=at(at(path,key),std::to_string(i));const auto& row=rows[i];
                if(std::string(key)=="places"){object(row,p,{"id","settlement_id","passages"});texts(row,p,{"id","settlement_id"});strings(row.at("passages"),at(p,"passages"));if(!places.emplace(text(row,"id"),&row).second)fail("NativeMilitaryDuplicatePlace",at(p,"id"));}
                else{object(row,p,{"id","entry_place"});texts(row,p,{"id","entry_place"});if(!settlements.emplace(text(row,"id"),&row).second)fail("NativeMilitaryDuplicateSettlement",at(p,"id"));}
            }
        }
        MilitaryScheduleBatch batch;batch.scope_token=text(input,"scope_token");batch.evaluated_count=static_cast<std::int64_t>(actors.size());const auto player=text(input,"player_actor_id");
        for(std::size_t i=0;i<actors.size();++i){const auto& actor=actors[i];const auto id=text(actor,"actor_id");if(id==player)continue;
            if(!actor.at("alive").get<bool>() || !actor.at("available").get<bool>() || !text(actor,"action_id").empty() || actor.at("combat_occupied").get<bool>())continue;
            const Json* campaign=nullptr;for(const auto& current:actor.at("campaigns"))if(text(current,"actor_id")==id && text(current,"template_id")==text(actor.at("plan"),"order_id"))campaign=&current;
            if(campaign && text(*campaign,"status")=="closed")continue;
            auto proposal=next(actor,campaign,places,settlements,at(at(path,"actors"),std::to_string(i)));if(proposal){proposal->actor_id=id;batch.proposals.push_back(std::move(*proposal));}
        }return {true,std::move(batch),{}};
    }catch(const detail::Failure& e){return {false,{},e.error};}catch(...){return {false,{}, {"NativeMilitaryFailure","/military_schedule"}};}
}
} // 所有实际世界改变仍由原普通命令与最外层事务负责。
