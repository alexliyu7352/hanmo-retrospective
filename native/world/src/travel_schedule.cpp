// 已授权作者行程的同义纯算法；只读有限输入，不安装投影或触碰世界权威。
#include "hanmo/travel_schedule.hpp"
#include "typed_wire.hpp"
#include <optional>
#include <set>

namespace hanmo {
namespace {
using detail::Json;
using detail::at;
using detail::fail;
// 临时行仅含原算法读集；未到的目的地与index不施加世界Schema约束。
struct ActorRow {
    std::string actor_id,action_id,kind,settlement_id,place_id,entry_place;
    bool alive=false,available=false;
    struct Itinerary { std::int64_t index=0,ready_tick=0; };
    std::optional<Itinerary> itinerary;
    std::vector<std::string> destinations;
};
// 保留真实队伍原序及成员；第一匹配空ID也必须阻止继续搜后队伍。
struct PartyRow { std::string id; std::vector<std::string> members; };
// 会话只保本人key与实际provider，不传对白、其它facts或全世界。
struct InteractionRow { std::string actor_id,provider_actor_id; };
// 只核JSON结构类型；所有容器已由共用parse做字节与物理深度限额。
void object(const Json& value,const std::string& path) {
    if(!value.is_object()) fail("NativeTravelInvalidValue",path);
}
// 数组不排序、不去重，目的地重复项影响原一次推进语义。
void array(const Json& value,const std::string& path) {
    if(!value.is_array()) fail("NativeTravelInvalidValue",path);
}
// 布尔不接受数字或字符串转换，维持Godot原值类型。
bool boolean(const Json& value,const std::string& path) {
    if(!value.is_boolean()) fail("NativeTravelInvalidValue",path);
    return value.get<bool>();
}
// 规范十进制复用既有int64解析，不限制候选ready_tick为已提交MAX_TICK。
std::int64_t integer(const Json& value,const std::string& path) {
    return detail::decimal(detail::json_string(value,path),path);
}
// 字符串数组仅核协议类型，空列表与索引范围留到算法实际解引用时。
std::vector<std::string> strings(const Json& value,const std::string& path) {
    array(value,path);
    std::vector<std::string> result;result.reserve(value.size());
    for(std::size_t i=0;i<value.size();++i) result.push_back(detail::json_string(value[i],at(path,std::to_string(i))));
    return result;
}
// 封闭人物行解析不承担Behavior授权或完整世界Schema，运行身份来自旧域适配。
ActorRow actor(const Json& value,const std::string& path) {
    object(value,path);
    detail::fields(value,{"actor_id","action_id","alive","available","location","itinerary","destinations"},{},path);
    ActorRow result;
    result.actor_id=detail::json_string(value.at("actor_id"),at(path,"actor_id"));
    result.action_id=detail::json_string(value.at("action_id"),at(path,"action_id"));
    result.alive=boolean(value.at("alive"),at(path,"alive"));
    result.available=boolean(value.at("available"),at(path,"available"));
    const auto& location=value.at("location");const auto location_path=at(path,"location");
    object(location,location_path);
    detail::fields(location,{"kind"},{"settlement_id","place_id","entry_place"},location_path);
    result.kind=detail::json_string(location.at("kind"),at(location_path,"kind"));
    if(result.kind=="Settlement") {
        detail::fields(location,{"kind","settlement_id","place_id","entry_place"},{},location_path);
        result.settlement_id=detail::json_string(location.at("settlement_id"),at(location_path,"settlement_id"));
        result.place_id=detail::json_string(location.at("place_id"),at(location_path,"place_id"));
        result.entry_place=detail::json_string(location.at("entry_place"),at(location_path,"entry_place"));
    } else detail::fields(location,{"kind"},{},location_path);
    const auto& itinerary=value.at("itinerary");const auto itinerary_path=at(path,"itinerary");
    if(!itinerary.is_null()) {
        object(itinerary,itinerary_path);detail::fields(itinerary,{"index","ready_tick"},{},itinerary_path);
        result.itinerary=ActorRow::Itinerary{integer(itinerary.at("index"),at(itinerary_path,"index")),integer(itinerary.at("ready_tick"),at(itinerary_path,"ready_tick"))};
    }
    result.destinations=strings(value.at("destinations"),at(path,"destinations"));
    return result;
}
// 原Location.party_id先找到成员所在的第一队伍，再由调用点判断其ID非空。
std::string party_id(const std::vector<PartyRow>& parties,const std::string& actor_id) {
    for(const auto& party:parties) for(const auto& member:party.members) if(member==actor_id) return party.id;
    return {};
}
// Godot Array允许[-size,-1]索引；只拒绝实际越界，负号取模由原有符号index保留。
const std::string& destination(const ActorRow& actor,std::int64_t index,const std::string& path) {
    const auto count=static_cast<std::int64_t>(actor.destinations.size());
    if(index < -count || index>=count) fail("NativeTravelDestinationIndex",path);
    return actor.destinations[static_cast<std::size_t>(index<0?count+index:index)];
}
} // 全部临时记录只活在单次同步函数调用中。

// 先解析整份协议，后按旧行序过滤；任何后项失败都销毁局部完整候选而不返回前缀。
Result<TravelScheduleBatch> travel_schedule_proposals(const std::string& bytes) {
    try {
        const std::string path="/travel_schedule";
        const auto input=detail::parse(bytes,path);object(input,path);
        detail::fields(input,{"format","scope_token","tick","player_actor_id","actors","parties","interactions"},{},path);
        if(detail::json_string(input.at("format"),at(path,"format"))!="HM-NATIVE-TRAVEL-SCHEDULE/1") fail("NativeTravelUnsupportedFormat",at(path,"format"));
        TravelScheduleBatch batch;
        batch.scope_token=detail::json_string(input.at("scope_token"),at(path,"scope_token"));
        const auto tick=integer(input.at("tick"),at(path,"tick"));
        const auto player=detail::json_string(input.at("player_actor_id"),at(path,"player_actor_id"));
        const auto& raw_actors=input.at("actors");array(raw_actors,at(path,"actors"));
        std::vector<ActorRow> actors;actors.reserve(raw_actors.size());std::set<std::string> ids;
        for(std::size_t i=0;i<raw_actors.size();++i) {
            const auto actor_path=at(at(path,"actors"),std::to_string(i));
            auto row=actor(raw_actors[i],actor_path);
            if(!ids.insert(row.actor_id).second) fail("NativeTravelDuplicateActor",at(actor_path,"actor_id"));
            actors.push_back(std::move(row));
        }
        batch.evaluated_count=static_cast<std::int64_t>(actors.size());
        const auto& raw_parties=input.at("parties");array(raw_parties,at(path,"parties"));
        std::vector<PartyRow> parties;parties.reserve(raw_parties.size());
        for(std::size_t i=0;i<raw_parties.size();++i) {
            const auto party_path=at(at(path,"parties"),std::to_string(i));const auto& row=raw_parties[i];
            object(row,party_path);detail::fields(row,{"id","members"},{},party_path);
            parties.push_back({detail::json_string(row.at("id"),at(party_path,"id")),strings(row.at("members"),at(party_path,"members"))});
        }
        const auto& raw_interactions=input.at("interactions");array(raw_interactions,at(path,"interactions"));
        std::vector<InteractionRow> interactions;interactions.reserve(raw_interactions.size());
        for(std::size_t i=0;i<raw_interactions.size();++i) {
            const auto interaction_path=at(at(path,"interactions"),std::to_string(i));const auto& row=raw_interactions[i];
            object(row,interaction_path);detail::fields(row,{"actor_id","provider_actor_id"},{},interaction_path);
            interactions.push_back({detail::json_string(row.at("actor_id"),at(interaction_path,"actor_id")),detail::json_string(row.at("provider_actor_id"),at(interaction_path,"provider_actor_id"))});
        }
        for(std::size_t i=0;i<actors.size();++i) {
            const auto& row=actors[i];
            if(row.actor_id==player || !row.itinerary) continue;
            if(!row.action_id.empty() || row.itinerary->ready_tick>tick || !party_id(parties,row.actor_id).empty()) continue;
            if(!row.alive || !row.available || row.kind!="Settlement") continue;
            if(row.place_id!=row.entry_place) continue;
            bool talking=false;
            for(const auto& session:interactions) if(session.actor_id==row.actor_id || session.provider_actor_id==row.actor_id) talking=true;
            if(talking) continue;
            const auto index_path=at(at(at(at(path,"actors"),std::to_string(i)),"itinerary"),"index");
            auto index=row.itinerary->index;
            const auto count=static_cast<std::int64_t>(row.destinations.size());
            if(destination(row,index,index_path)==row.settlement_id) index=(index+1)%count;
            const auto& target=destination(row,index,index_path);
            if(target==row.settlement_id) continue;
            batch.proposals.push_back({row.actor_id,target,(index+1)%count});
        }
        return {true,std::move(batch),{}};
    } catch(const detail::Failure& failure) { return {false,{},failure.error}; }
    catch(...) { return {false,{}, {"NativeTravelFailure","/travel_schedule"}}; }
}
} // 原世界事务仍是提案执行及发布的唯一所有者。
