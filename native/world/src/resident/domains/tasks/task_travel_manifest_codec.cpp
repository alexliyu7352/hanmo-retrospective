// 携行摘要边界只有货批与同行者两类；没有库存入口、坐标、世界写回或通用字段包。
#include "task_travel_manifest.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 世界十六进制身份与原live来源一致，跨读档的epoch也必须匹配。
V identity_value(const WorldIdentity& value){StableId hex;const char* digits="0123456789abcdef";for(const auto byte:value.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{value.execution_epoch}},{"revision",V{value.revision}},{"tick",V{value.tick}}});}
V cursor_value(const TaskTravelManifestCursor& cursor){return boundary::obj({{"identity",identity_value(cursor.identity)},{"observer_actor_id",V{cursor.observer_actor_id}},{"offset",V{cursor.offset}}});}
}
// 每个货批保独立lot_id，前端逐行显示，不把分页合成一份可独立修改的总库存。
BoundaryValue encode_task_travel_manifest(const TaskTravelManifestPage& page){
    V::Array rows;for(const auto& entry:page.rows)rows.push_back(std::visit([](const auto& row)->V{
        V::Dict value{{"task_id",V{row.task_id}},{"display_name",V{row.display_name}},{"task_status",V{row.task_status}},{"observed_tick",V{row.observed_tick}}};
        if constexpr(std::is_same_v<std::decay_t<decltype(row)>,TaskTravelCargo>){value.emplace_back("kind",V{"cargo"});value.emplace_back("lot_id",V{row.lot_id});value.emplace_back("item_id",V{row.item_id});value.emplace_back("item_name",V{row.item_name});value.emplace_back("cargo_kind",V{row.cargo_kind});value.emplace_back("quantity",V{row.quantity});value.emplace_back("reserved",V{row.reserved});}
        else{value.emplace_back("kind",V{"escort"});value.emplace_back("actor_id",V{row.actor_id});value.emplace_back("actor_name",V{row.actor_name});value.emplace_back("objective_state",V{row.objective_state});}return V{std::move(value)};
    },entry));
    V::Dict result{{"ok",V{true}},{"version",V{Integer{1}}},{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"cursor",V{page.cursor}},{"scanned",V{page.scanned}},{"rows",V{std::move(rows)}}};if(page.next_cursor)result.emplace_back("next_cursor",cursor_value(*page.next_cursor));return V{std::move(result)};
}
// 续页严格校验闭合形状，不接受caller指定其他人物或任意远方地点。
Outcome<TaskTravelManifestCursor> decode_task_travel_manifest_cursor(const BoundaryValue& input){try{
    using namespace boundary;fields(input,{"identity","observer_actor_id","offset"},{},"cursor");const auto& identity=field(input,"identity","cursor");fields(identity,{"world_id","execution_epoch","revision","tick"},{},"cursor/identity");TaskTravelManifestCursor result;const auto hex=get_string(identity,"world_id","identity");if(hex.size()!=result.identity.world_id.size()*2)fail("InvalidTaskTravelIdentity","cursor");
    const auto nibble=[](char value)->int{if(value>='0'&&value<='9')return value-'0';if(value>='a'&&value<='f')return value-'a'+10;return -1;};for(std::size_t n=0;n<result.identity.world_id.size();++n){const auto high=nibble(hex[n*2]),low=nibble(hex[n*2+1]);if(high<0||low<0)fail("InvalidTaskTravelIdentity","cursor");result.identity.world_id[n]=static_cast<std::uint8_t>(high*16+low);}
    result.identity.execution_epoch=get_int(identity,"execution_epoch","identity");result.identity.revision=get_int(identity,"revision","identity");result.identity.tick=get_int(identity,"tick","identity");result.observer_actor_id=get_string(input,"observer_actor_id","cursor");result.offset=get_int(input,"offset","cursor");if(result.identity.execution_epoch<0||result.identity.revision<0||result.identity.tick<0||result.offset<0||result.observer_actor_id.empty()||result.observer_actor_id.size()>4096)fail("InvalidTaskTravelCursor","cursor");return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
