// 地图同地观察的唯一有限边界编码，游标严格绑定原身份和位置，不接受额外世界字段。
#include "map_presence.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 同现有查询边界用固定十六进制世界ID，不输出内部指针或可变引用。
V identity_value(const WorldIdentity& identity){
    StableId hex;const char* digits="0123456789abcdef";
    for(const auto byte:identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}
    return boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{identity.execution_epoch}},{"revision",V{identity.revision}},{"tick",V{identity.tick}}});
}
// 位置键是原游标的精确匹配条件，不能用它请求远方观察。
V location_value(const MapPresenceLocationKey& key){return boundary::obj({{"kind",V{Integer(key.kind)}},{"primary",V{key.primary}},{"secondary",V{key.secondary}},{"x",V{key.x}},{"y",V{key.y}}});}
// 整数锚点保持原native数据，浮点道路插值沿Godot原MapProjection实现。
V point(const PointI& value){return V{V::Array{V{value.x},V{value.y}}};}
// 下一页同时携带来源和范围；只暴露当前同地序列中的偏移。
V cursor_value(const MapPresenceCursor& value){return boundary::obj({{"identity",identity_value(value.identity)},{"observer_actor_id",V{value.observer_actor_id}},{"location",location_value(value.location)},{"offset",V{value.offset}}});}
}

BoundaryValue encode_map_presence(const MapPresencePage& page){
    V::Array rows;
    for(const auto& row:page.rows)rows.push_back(boundary::obj({{"kind",V{"actor"}},{"actor_id",V{row.actor_id}},{"display_name",V{row.display_name}},{"observed_tick",V{row.observed_tick}}}));
    V::Dict position{{"world_map_id",V{page.position.world_map_id}}};
    if(page.position.road){
        const auto& road=*page.position.road;V::Array points;for(const auto& p:road.points)points.push_back(point(p));
        position.emplace_back("road_position",boundary::obj({{"edge_id",V{road.edge_id}},{"progress",V{road.progress}},{"length",V{road.length}},{"points",V{std::move(points)}},{"raw",V{road.raw}}}));
    }else position.emplace_back("observer_position",point(page.position.observer_position));
    V::Dict result{{"version",V{Integer(1)}},{"identity",identity_value(page.identity)},{"observer_actor_id",V{page.observer_actor_id}},{"location",location_value(page.location)},{"position",V{std::move(position)}},{"cursor",V{page.cursor}},{"rows",V{std::move(rows)}}};
    if(page.next_cursor)result.emplace_back("next_cursor",cursor_value(*page.next_cursor));
    return V{std::move(result)};
}

Outcome<MapPresenceCursor> decode_map_presence_cursor(const BoundaryValue& input){try{
    using namespace boundary;fields(input,{"identity","observer_actor_id","location","offset"},{},"cursor");
    const auto& identity=field(input,"identity","cursor");fields(identity,{"world_id","execution_epoch","revision","tick"},{},"cursor/identity");
    MapPresenceCursor result;const auto hex=get_string(identity,"world_id","cursor/identity");
    if(hex.size()!=result.identity.world_id.size()*2)fail("InvalidMapPresenceIdentity","cursor/identity");
    const auto nibble=[](char value)->int{if(value>='0'&&value<='9')return value-'0';if(value>='a'&&value<='f')return value-'a'+10;return -1;};
    for(std::size_t i=0;i<result.identity.world_id.size();++i){const auto hi=nibble(hex[i*2]),lo=nibble(hex[i*2+1]);if(hi<0||lo<0)fail("InvalidMapPresenceIdentity","cursor/identity");result.identity.world_id[i]=static_cast<std::uint8_t>(hi*16+lo);}
    result.identity.execution_epoch=get_int(identity,"execution_epoch","cursor/identity");result.identity.revision=get_int(identity,"revision","cursor/identity");result.identity.tick=get_int(identity,"tick","cursor/identity");
    result.observer_actor_id=get_string(input,"observer_actor_id","cursor");result.offset=get_int(input,"offset","cursor");
    const auto& location=field(input,"location","cursor");fields(location,{"kind","primary","secondary","x","y"},{},"cursor/location");
    const auto kind=get_int(location,"kind","cursor/location");if(kind<0||kind>2)fail("InvalidMapPresenceLocation","cursor/location");result.location.kind=static_cast<int>(kind);
    result.location.primary=get_string(location,"primary","cursor/location");result.location.secondary=get_string(location,"secondary","cursor/location");result.location.x=get_int(location,"x","cursor/location");result.location.y=get_int(location,"y","cursor/location");
    if(result.identity.execution_epoch<0||result.identity.revision<0||result.identity.tick<0||result.observer_actor_id.empty()||result.observer_actor_id.size()>4096||result.location.primary.size()>4096||result.location.secondary.size()>4096||result.offset<0)fail("InvalidMapPresenceCursor","cursor");
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
}
