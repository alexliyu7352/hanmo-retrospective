// 场景目录只编译原有限显示字段；模板映射保坏值标志，仍在可见分支才报告原错误。
#include "scene_presentation.hpp"
namespace hanmo::resident {
    namespace {
    // 坐标显示允许原整数或浮点，进入Vector2时才沿Godot binary32舍入。
    ScenePoint point(const BoundaryValue& value,const StableId& path){
        auto& p=boundary::array(value,path);
        if(p.size()!=2)boundary::fail("InvalidSceneMapLocation",path);
        auto number=[&](const BoundaryValue& v){
            if(auto i=v.as<Integer>())return double(*i);
            return boundary::real(v,path);
        };
        return {number(p[0]),number(p[1])};
    }
    }
    Outcome<SceneCatalog> decode_scene_catalog(const BoundaryValue& data){
        try{
            using namespace boundary;
            SceneCatalog c;
            auto caps=strings(field(data,"required_capabilities","data"),"required_capabilities");
            c.estates=std::find(caps.begin(),caps.end(),"estates")!=caps.end();
            for(auto& row:object(field(data,"places","data"),"places")){
                ScenePlaceDefinition p;
                p.id=row.first;
                p.settlement_id=get_string(row.second,"settlement_id",row.first);
                if(auto v=row.second.find("name_key"))p.name_key=string(*v,row.first);
                if(auto v=row.second.find("visual_tag"))p.visual_tag=string(*v,row.first);
                auto parent=row.second.find("parent_place");
                if(!parent)fail("InvalidScenePlaceHierarchy",row.first);
                if(!parent->as<std::monostate>())p.parent_place=string(*parent,row.first);
                if(auto mapping=row.second.find("scene_template_ids")){
                    if(auto fields=mapping->as<BoundaryValue::Dict>()){
                        for(auto& item:*fields){
                            if(auto name=item.second.as<StableId>())p.scene_template_ids.insert(item.first,*name);
                            else p.invalid_template_views.push_back(item.first);
                        }
                    }
                    else p.template_mapping_valid=false;
                }
                if(auto v=row.second.find("scene_template_id")){
                    if(auto name=v->as<StableId>())p.scene_template_id=*name;
                    else p.legacy_template_valid=false;
                }
                c.places.insert(row.first,p);
            }
            for(auto& row:object(field(data,"settlements","data"),"settlements")){
                SceneSettlementDefinition s;
                s.id=row.first;
                if(auto v=row.second.find("name_key"))s.name_key=string(*v,row.first);
                if(auto v=row.second.find("entry_place"))s.entry_place=string(*v,row.first);
                s.point=point(field(row.second,"map_xy",row.first),row.first);
                c.settlements.insert(row.first,s);
            }
            for(auto& row:object(field(data,"routes","data"),"routes")){
                SceneRouteDefinition r;
                r.id=row.first;
                r.duration_ticks=get_int(row.second,"duration_ticks",row.first);
                auto origin=c.settlements.find(get_string(row.second,"origin",row.first)),destination=c.settlements.find(get_string(row.second,"destination",row.first));
                if(!origin||!destination)fail("InvalidSceneMapLocation",row.first);
                r.points.push_back(origin->point);
                if(auto shape=row.second.find("shape"))for(auto& p:array(*shape,row.first))r.points.push_back(point(p,row.first));
                r.points.push_back(destination->point);
                c.routes.insert(row.first,r);
            }
            for(auto& row:object(field(data,"world_maps","data"),"world_maps")){
                SceneMapDefinition m;
                m.id=row.first;
                if(auto v=row.second.find("scene_template_id")){
                    if(auto name=v->as<StableId>())m.scene_template_id=*name;
                    else m.template_mapping_valid=false;
                }
                c.maps.insert(row.first,m);
            }
            for(auto& row:object(field(data,"scenarios","data"),"scenarios"))if(auto map=row.second.find("world_map_id"))c.scenario_maps.insert(row.first,string(*map,row.first));
            if(auto estates=data.find("estates"))for(auto& row:object(*estates,"estates")){
                StableId name;
                if(auto v=row.second.find("name_key"))name=string(*v,row.first);
                c.estate_name_keys.insert(row.first,name);
            }
            return {true,std::move(c),{}};
        }
        catch(const boundary::DecodeFailure& e){
            return {false,{},e.error};
        }
    }
}
