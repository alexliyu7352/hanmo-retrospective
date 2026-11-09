#include "resident/domains/military/military_model.hpp"
// 原场景投影与场所侧栏；可见性来自当前真实位置，历史只读既有社会记忆。
#include "scene_presentation.hpp"
#include <picosha2.h>
#include <cmath>
#include <set>
namespace hanmo::resident {
    namespace {
    using V=BoundaryValue;
    using D=V::Dict;
    using A=V::Array;
    struct Ref {
        StableId settlement,place,parent;
    };
    // 领域失败不返回半份场景，也不移动人物来恢复画面。
    Outcome<V> fail(const StableId& code){
        return {false,{},common_failure(code).error};
    }
    V ref(const Ref& r){
        return V{D{{"settlement_id",V{r.settlement}},{"place_id",V{r.place}},{"parent_place_id",V{r.parent}}}};
    }
    V target(const StableId& kind,const StableId& id){
        return V{D{{"kind",V{kind}},{"id",V{id}}}};
    }
    // 精确比较完整位置标签和值，不以同一place子字段误认道路/跟随身份。
    bool equal_location(const Location& a,const Location& b){
        if(a.index()!=b.index())return false;
        if(auto x=std::get_if<SettlementLocation>(&a)){
            auto& y=std::get<SettlementLocation>(b);
            return x->settlement_id==y.settlement_id&&x->place_id==y.place_id;
        }
        if(auto x=std::get_if<WildernessLocation>(&a)){
            auto& y=std::get<WildernessLocation>(b);
            return x->map_id==y.map_id&&x->origin==y.origin&&x->point.x==y.point.x&&x->point.y==y.point.y;
        }
        if(auto x=std::get_if<OnRouteLocation>(&a)){
            auto& y=std::get<OnRouteLocation>(b);
            return x->edge_id==y.edge_id&&x->origin==y.origin&&x->progress==y.progress;
        }
        if(auto x=std::get_if<FollowingLocation>(&a))return x->party_id==std::get<FollowingLocation>(b).party_id;
        return std::get<InArmyLocation>(a).army_id==std::get<InArmyLocation>(b).army_id;
    }
    // 场所引用逐层验同据点与循环；根parent保空串供原DTO消费。
    Outcome<Ref> place_ref(const SceneCatalog& c,const StableId& id){
        auto p=c.places.find(id);
        if(!p)return {false,{},common_failure("UnknownScenePlace").error};
        if(!c.settlements.find(p->settlement_id))return {false,{},common_failure("UnknownSceneSettlement").error};
        std::set<StableId> seen;
        auto cursor=std::optional<StableId>{id};
        while(cursor){
            auto parent=c.places.find(*cursor);
            if(!parent||parent->id!=*cursor||parent->settlement_id!=p->settlement_id||!seen.insert(*cursor).second)return {false,{},common_failure("InvalidScenePlaceHierarchy").error};
            cursor=parent->parent_place;
        }
        return {true,{p->settlement_id,id,p->parent_place.value_or("")},{}};
    }
    // 场景对象只含身份、名称键和既有外观标签，不复制准入规则。
    V place_object(const SceneCatalog& c,const Ref& r){
        auto p=c.places.find(r.place);
        return V{D{{"target_ref",target("place",r.place)},{"location_ref",ref(r)},{"name_key",V{p->name_key}},{"visual_tag",V{p->visual_tag}}}};
    }
    // 热点identity沿旧排序JSON的SHA256，明确要求重新preview而非显示即授权。
    V candidate(const StableId& kind,V params,const StableId& target_kind,const StableId& id,Integer revision){
        auto sorted=params;
        auto& fields=*sorted.as<D>();
        std::sort(fields.begin(),fields.end(),[](auto& a,auto& b){
            return a.first<b.first;
        });
        V canonical_target{D{{"id",V{id}},{"kind",V{target_kind}}}};
        auto canonical=render_boundary_json(V{A{V{kind},canonical_target,sorted}});
        if(!canonical.ok)boundary::fail(canonical.error.original.code,canonical.error.original.path);
        return V{D{{"id",V{kind+":"+picosha2::hash256_hex_string(canonical.value)}},{"target_ref",target(target_kind,id)},{"command_kind",V{kind}},{"command_source",V{StableId{"kernel_compatibility"}}},{"params",params},{"expected_revision",V{revision}},{"enabled",V{false}},{"reason",V{StableId{"KernelPreviewRequired"}}},{"needs_preview",V{true}}}};
    }
    // 只读原三种社会事件记忆，时间和来源保持历史记录而不查询远方人物。
    A known_events(const CommonStore& common,const SocialState& social,const SceneCatalog& c,const StableId& observer,const Ref& r){
        A result;
        auto memories=social.memories.find(observer);
        if(!memories)return result;
        auto ids=memories->order();
        std::sort(ids.begin(),ids.end());
        for(auto& id:ids){
            auto event=social.events.find(id);
            auto memory=memories->find(id);
            if(!event||(event->kind!="aid"&&event->kind!="theft"&&event->kind!="refusal")||event->tick>memory->learned_tick||memory->learned_tick>common.tick())continue;
            auto place=c.places.find(event->place_id);
            if(r.place!=event->place_id&&(!r.place.empty()||r.settlement.empty()||!place||r.settlement!=place->settlement_id))continue;
            result.push_back(V{D{{"fact_id",V{id}},{"kind",V{event->kind}},{"place_id",V{event->place_id}},{"actor_ids",V{A{V{event->actor_id},V{event->target_actor_id}}}},{"fact_tick",V{event->tick}},{"knowledge_tick",V{memory->learned_tick}},{"source_actor_id",V{memory->source_actor_id}}}});
        }
        return result;
    }
    // Vector2地图显示按binary32投影，插值绝不回写原生世界的整数坐标。
    Outcome<ScenePoint> observer_point(const SceneCatalog& c,const Location& location){
        if(auto p=std::get_if<WildernessLocation>(&location))return {true,{double(float(p->point.x)),double(float(p->point.y))},{}};
        if(auto p=std::get_if<SettlementLocation>(&location)){
            auto town=c.settlements.find(p->settlement_id);
            if(!town)return {false,{},common_failure("InvalidSceneMapLocation").error};
            return {true,{double(float(town->point.x)),double(float(town->point.y))},{}};
        }
        auto route=std::get_if<OnRouteLocation>(&location);
        auto edge=route?c.routes.find(route->edge_id):nullptr;
        if(!edge||edge->duration_ticks<=0||edge->points.empty())return {false,{},common_failure("InvalidSceneMapLocation").error};
        std::vector<std::pair<float,float>> points;
        for(auto p:edge->points)points.emplace_back(float(p.x),float(p.y));
        auto distance=[](auto a,auto b){
            float x=a.first-b.first,y=a.second-b.second;
            return std::sqrt(x*x+y*y);
        };
        double total=0;
        for(std::size_t i=1;i<points.size();++i)total+=distance(points[i-1],points[i]);
        double remaining=std::clamp(double(route->progress)/(edge->duration_ticks*100),0.0,1.0)*total;
        for(std::size_t i=1;i<points.size();++i){
            double length=distance(points[i-1],points[i]);
            if(length>0&&remaining<=length){
                float weight=float(remaining/length);
                float x=points[i-1].first+weight*(points[i].first-points[i-1].first),y=points[i-1].second+weight*(points[i].second-points[i-1].second);
                return {true,{double(x),double(y)},{}};
            }
            remaining-=length;
        }
        return {true,{double(points.back().first),double(points.back().second)},{}};
    }
    // 每设施只取最后真实建设事实的公开结果，费用和任务权限不出现在场景。
    A construction(const EstateState& estate,const StableId& id){
        OrderedTable<std::pair<StableId,EstateConstructionFact>> latest;
        auto facts=estate.facts.order();
        std::sort(facts.begin(),facts.end());
        for(auto& key:facts){
            auto f=std::get_if<EstateConstructionFact>(estate.facts.find(key));
            if(!f||f->terms.estate_id!=id)continue;
            auto prior=latest.find(f->terms.facility);
            if(prior&&prior->second.start_tick>f->start_tick)continue;
            latest.set(f->terms.facility,{key,*f});
        }
        auto facilities=latest.order();
        std::sort(facilities.begin(),facilities.end());
        A result;
        for(auto& facility:facilities){
            auto& row=*latest.find(facility);
            auto& f=row.second;
            result.push_back(V{D{{"fact_id",V{row.first}},{"facility",V{facility}},{"phase",V{f.phase}},{"start_tick",V{f.start_tick}},{"completed_tick",V{f.completed_tick}},{"success",V{f.success}},{"from_level",V{f.terms.from_level}},{"to_level",V{f.terms.to_level}}}});
        }
        return result;
    }
    // 当前对白说话人只在会话存在时查，普通空场景不预读人物外貌。
    V dialogue_speaker(const InteractionState& state,const InteractionCatalog& catalog,const CommonPresentation& presentation,const StableId& actor){
        for(auto& session:state.interactions)if(session.actor_id==actor){
            StableId speaker=session.context.provider_actor_id;
            if(session.phase=="say")for(auto& dialogue:catalog.dialogues)if(dialogue.id==session.dialogue_id)for(auto& node:dialogue.nodes)if(node.id==session.node_id)speaker=node.speaker=="$provider"?session.context.provider_actor_id:node.speaker=="$player"?actor:node.speaker;
            auto name=presentation.display_name(speaker);
            return V{D{{"actor_id",V{speaker}},{"display_name",V{name.ok?name.value:StableId{}}}}};
        }
        return V{D{}};
    }
    }
    // 原scene.query完整可见分支，所有结果都是独立有限显示值。
    Outcome<BoundaryValue> query_scene_view(const CommonStore& common,const InteractionState&,const InteractionCatalog& interaction,const SocialState& social,const EstateState& estates,const EstateCatalog& estate_catalog,const CommonCatalog& catalog,const StableId& place,const StableId& view){
        try{
            if(view!="world"&&view!="city"&&view!="interior")return fail("InvalidSceneView");
            auto observer=common.state().metadata.player_actor_id;
            auto actor=common.actor(observer);
            if(!actor)return fail("UnknownSceneObserver");
            if(!actor->alive)return fail("SceneObserverUnavailable");
            if(!interaction.scene)return fail("InvalidSceneState");
            auto& c=*interaction.scene;
            auto location=common.resolved_location(observer);
            if(!location.ok)return fail("InvalidSceneObserverLocation");
            Ref current;
            if(auto town=std::get_if<SettlementLocation>(&location.value)){
                auto at=place_ref(c,town->place_id);
                if(!at.ok||at.value.settlement!=town->settlement_id)return fail("InvalidSceneObserverLocation");
                current=at.value;
            }
            auto requested=place.empty()?Outcome<Ref>{true,{current.settlement,"",""},{}}:place_ref(c,place);
            if(!requested.ok)return {false,{},requested.error};
            auto reference=requested.value;
            if(view=="interior"&&place.empty())return fail("ScenePlaceRequired");
            if(view=="city"&&reference.settlement.empty())return fail("SceneSettlementRequired");
            bool same=!current.place.empty()&&current.settlement==reference.settlement;
            bool at=same&&(place.empty()||current.place==place);
            bool visible=view=="interior"?at:same;
            if(view=="world"&&place.empty()&&(std::holds_alternative<WildernessLocation>(location.value)||std::holds_alternative<OnRouteLocation>(location.value)))visible=true;
            auto events=known_events(common,social,c,observer,reference);
            Integer tick=visible?common.tick():-1;
            if(!visible)for(auto& e:events)tick=std::max(tick,boundary::get_int(e,"knowledge_tick","event"));
            D layers;
            for(auto name:{"background","buildings","furnishings","actors","effects","foreground"})layers.emplace_back(name,V{A{}});
            V result{D{{"ok",V{true}},{"view_kind",V{view}},{"observer_id",V{observer}},{"location_ref",ref(reference)},{"revision",V{common.revision()}},{"knowledge_tick",V{tick}},{"visibility",V{StableId{visible?"current":events.empty()?"unknown":"remembered"}}},{"layers",V{layers}},{"people",V{A{}}},{"hotspot_candidates",V{A{}}},{"known_events",V{events}},{"geometry",V{D{}}},{"state_capabilities",V{D{{"place_hierarchy",V{D{{"supported",V{true}}}}},{"estate_facility_level",V{D{{"supported",V{c.estates}}}}},{"building_damage",V{D{{"supported",V{false}},{"code",V{StableId{"SceneStateCapabilityUnavailable"}}}}}},{"business_open",V{D{{"supported",V{false}},{"code",V{StableId{"SceneStateCapabilityUnavailable"}}}}}}}}}}};
            if(!visible)return {true,result,{}};
            StableId template_place=place;
            if(template_place.empty()&&view=="city"){
                auto settlement=c.settlements.find(reference.settlement);
                template_place=settlement?settlement->entry_place:StableId{};
                auto entry=place_ref(c,template_place);
                if(!entry.ok||entry.value.settlement!=reference.settlement)return fail("InvalidSceneCityEntry");
            }
            StableId template_id,source_kind,source_id;
            if(!template_place.empty()){
                auto p=c.places.find(template_place);
                if(!p||!p->template_mapping_valid||std::find(p->invalid_template_views.begin(),p->invalid_template_views.end(),view)!=p->invalid_template_views.end())return fail("InvalidSceneTemplateMapping");
                auto selected=p->scene_template_ids.find(view);
                if(selected){
                    if(selected->empty())return fail("InvalidSceneTemplateMapping");
                    template_id=*selected;
                }
                else if(view!="world"&&!p->legacy_template_valid)return fail("InvalidSceneTemplateMapping");
                else if(view!="world"&&p->scene_template_id){
                    if(p->scene_template_id->empty())return fail("InvalidSceneTemplateMapping");
                    template_id=*p->scene_template_id;
                }
                if(!template_id.empty()){
                    source_kind="place";
                    source_id=template_place;
                }
            }
            if(template_id.empty()&&view=="world"){
                auto id=c.scenario_maps.find(common.state().metadata.scenario_id);
                auto map=id?c.maps.find(*id):nullptr;
                if(map){
                    if(!map->template_mapping_valid||(map->scene_template_id&&map->scene_template_id->empty()))return fail("InvalidSceneTemplateMapping");
                    if(map->scene_template_id){
                        template_id=*map->scene_template_id;
                        source_kind="world_map";
                        source_id=map->id;
                    }
                }
            }
            if(!template_id.empty()){
                result.as<D>()->emplace_back("template_id",V{template_id});
                result.as<D>()->emplace_back("template_source",target(source_kind,source_id));
            }
            auto layer=[&](const StableId& id)->A&{return *result.find("layers")->find(id)->as<A>();
            };
            auto& people=*result.find("people")->as<A>();
            auto& candidates=*result.find("hotspot_candidates")->as<A>();
            if(!reference.settlement.empty()){
                if(view=="interior")layer("background").push_back(place_object(c,reference));
                else{
                    auto town=c.settlements.find(reference.settlement);
                    layer("background").push_back(V{D{{"target_ref",target("settlement",reference.settlement)},{"name_key",V{town?town->name_key:StableId{}}}}});
                }
            }
            if(view=="world"){
                auto point=observer_point(c,location.value);
                if(!point.ok)return {false,{},point.error};
                *result.find("geometry")=V{D{{"observer_position",V{A{V{point.value.x},V{point.value.y}}}}}};
            }
            if(view=="city"){
                auto ids=c.places.order();
                std::sort(ids.begin(),ids.end());
                for(auto& id:ids){
                    auto p=c.places.find(id);
                    if(p->settlement_id!=reference.settlement)continue;
                    auto r=place_ref(c,id);
                    if(!r.ok)return {false,{},r.error};
                    auto parent=c.places.find(r.value.parent);
                    if(r.value.parent.empty()||(parent&&!parent->parent_place))layer("buildings").push_back(place_object(c,r.value));
                }
            }
            if(at){
                A passages;
                auto current_place=catalog.places.find(current.place);
                if(!current_place)return fail("UnknownScenePlace");
                for(auto& id:current_place->passages){
                    auto destination=place_ref(c,id);
                    if(!destination.ok||destination.value.settlement!=current.settlement)return fail("InvalidScenePassage");
                    passages.push_back(place_object(c,destination.value));
                    candidates.push_back(candidate("move_local",V{D{{"place_id",V{id}}}},"place",id,common.revision()));
                }
                result.as<D>()->emplace_back("passage_targets",V{passages});
                auto ids=common.actor_order();
                std::sort(ids.begin(),ids.end());
                CommonPresentation names(common.state(),catalog);
                for(auto& id:ids){
                    auto person=common.state().actors.find(id);
                    auto loc=common.resolved_location(id);
                    if(!person||!person->alive||!loc.ok||!equal_location(loc.value,location.value))continue;
                    auto definition=catalog.actor_names.find(person->prototype_id.value_or(id));
                    if(!definition)return fail("InvalidSceneActor");
                    auto name=names.display_name(id);
                    if(!name.ok)return {false,{},name.error};
                    Integer age=person->birth_tick?(common.tick()-*person->birth_tick)/720:-1;
                    A roles;
                    auto role_ids=catalog.roles.order();
                    std::sort(role_ids.begin(),role_ids.end());
                    for(auto& role_id:role_ids){
                        auto role=catalog.roles.find(role_id);
                        auto assignment=common.state().roles.find(role_id);
                        if(!assignment||assignment->actor_id!=id||!assignment->enabled||role->place_id!=current.place)continue;
                        roles.push_back(V{role_id});
                        if(id!=observer)candidates.push_back(candidate("talk",V{D{{"role_id",V{role_id}}}},"actor",id,common.revision()));
                    }
                    V projected{D{{"target_ref",target("actor",id)},{"actor_id",V{id}},{"location_ref",ref(current)},{"display_name",V{name.value}},{"age_years",V{age}},{"portrait_kind",V{definition->portrait_kind}},{"presentation_profile_id",V{definition->presentation_profile_id}},{"visibility_version",V{common.revision()}},{"knowledge_tick",V{common.tick()}},{"role_ids",V{roles}}}};
                    people.push_back(projected);
                    layer("actors").push_back(projected);
                }
                std::vector<const PropDefinition*> props;
                for(auto& p:interaction.props)props.push_back(&p);
                std::sort(props.begin(),props.end(),[](auto a,auto b){
                    return a->id<b->id;
                });
                for(auto prop:props)if(prop->place_id==current.place){
                    layer("furnishings").push_back(V{D{{"target_ref",target("prop",prop->id)},{"location_ref",ref(current)},{"name_key",V{prop->name_key}},{"kind",V{prop->kind}}}});
                    candidates.push_back(candidate("inspect",V{D{{"prop_id",V{prop->id}}}},"prop",prop->id,common.revision()));
                }
                auto estate_ids=estate_catalog.estates.order();
                std::sort(estate_ids.begin(),estate_ids.end());
                for(auto& id:estate_ids){
                    auto definition=estate_catalog.estates.find(id);
                    if(definition->place_id!=current.place)continue;
                    auto estate=estates.estates.find(id);
                    if(!estate)return fail("MissingSceneEstateState");
                    auto name=c.estate_name_keys.find(id);
                    auto facilities=boundary::ordered(V{D{{"farm",V{estate->facilities.farm}},{"granary",V{estate->facilities.granary}},{"palisade",V{estate->facilities.palisade}}}},estate->facilities.field_order);
                    layer("buildings").push_back(V{D{{"target_ref",target("estate",id)},{"location_ref",ref(current)},{"name_key",V{name?*name:StableId{}}},{"facilities",facilities},{"construction",V{construction(estates,id)}}}});
                    if(c.estates)for(auto facility:{"farm","granary","palisade"})candidates.push_back(candidate("estate_build",V{D{{"estate_id",V{id}},{"facility",V{StableId{facility}}}}},"estate",id,common.revision()));
                }
            }
            return {true,result,{}};
        }
        catch(const boundary::DecodeFailure& e){
            return {false,{},e.error};
        }
    }
    // 侧栏沿原对白早退，然后当前岗位主题、庄务插入序、本人军令和排队事件。
    Outcome<BoundaryValue> query_place_sidebar(const CommonStore& common,const InteractionState& state,const InteractionCatalog& catalog,const InteractionAccess& access,const EstateState& estate,const EstateCatalog& estates,const MilitaryState& military,const MilitaryCatalog& armies,const CommonCatalog& cc){
        auto player=common.state().metadata.player_actor_id;
        auto actor=common.actor(player);
        if(!actor)return fail("NoWorld");
        auto raw=boundary::encode_location(actor->location);
        D location;
        for(auto name:{"kind","place_id","settlement_id"})if(auto field=raw.find(name))location.emplace_back(name,*field);
        CommonPresentation presentation(common.state(),cc);
        V speaker=dialogue_speaker(state,catalog,presentation,player);
        V result{D{{"ok",V{true}},{"actor_id",V{player}},{"revision",V{common.revision()}},{"location",V{location}},{"dialogue_speaker",speaker},{"action_busy",V{false}},{"people",V{A{}}},{"estate_ids",V{A{}}},{"military_here",V{false}},{"operating_campaigns",V{A{}}},{"queued_event_ids",V{A{}}}}};
        if(!speaker.as<D>()->empty())return {true,result,{}};
        *result.find("action_busy")=V{!actor->action_id.empty()};
        StableId place;
        if(auto p=std::get_if<SettlementLocation>(&actor->location))place=p->place_id;
        auto topics=query_interaction_topics(common,state,catalog,access,player,place);
        if(!topics.ok)return topics;
        for(auto& group:*topics.value.as<A>()){
            auto id=boundary::get_string(group,"provider_actor_id","topics");
            auto person=common.actor(id);
            auto resolved=common.resolved_location(id);
            auto loc=resolved.ok?std::get_if<SettlementLocation>(&resolved.value):nullptr;
            if(!person||id==player||!loc||loc->place_id!=place||!person->alive)continue;
            A rows;
            for(auto& item:*group.find("topics")->as<A>())rows.push_back(V{D{{"role_id",*item.find("role_id")},{"available",*item.find("available")},{"code",*item.find("code")}}});
            auto name=presentation.display_name(id);
            result.find("people")->as<A>()->push_back(V{D{{"actor_id",V{id}},{"display_name",V{name.ok?name.value:StableId{}}},{"topics",V{rows}}}});
        }
        for(auto& row:estate.estates.rows())if(estate_available(common,estate,estates,row.first))result.find("estate_ids")->as<A>()->push_back(V{row.first});
        bool here=false;
        for(auto& camp:armies.garrisons)if(camp.place_id==place)here=true;
        for(auto& row:estates.estates.rows()){
            auto owned=estate.estates.find(row.first);
            if(row.second.place_id==place&&owned&&owned->owner_actor_id==player)here=true;
        }
        for(auto& campaign:military.campaigns)if(campaign.actor_id==player&&campaign.status!="closed")here=true;
        for(auto& army:military.armies)if(army.commander_actor_id==player&&army.status=="active")here=true;
        *result.find("military_here")=V{here};
        for(auto& campaign:military.campaigns){
            if(campaign.actor_id!=player||campaign.status!="operating")continue;
            StableId target;
            for(auto& order:armies.orders)if(order.id==campaign.template_id)target=order.target_stronghold_id;
            StableId siege;
            for(auto& fort:military.strongholds)if(fort.id==target)siege=fort.siege_id;
            result.find("operating_campaigns")->as<A>()->push_back(V{D{{"campaign_id",V{campaign.id}},{"target_siege_id",V{siege}}}});
        }
        for(auto& event:state.events)if(event.actor_id==player&&event.status=="queued")result.find("queued_event_ids")->as<A>()->push_back(V{event.event_id});
        return {true,result,{}};
    }
    // 可见当地活人才能读取原公开外貌，不将场景查询扩大到远方全人物。
    Outcome<BoundaryValue> query_scene_character(const CommonStore& common,const InteractionState&,const InteractionCatalog&,const CommonCatalog& catalog,const StableId& id){
        auto player=common.state().metadata.player_actor_id;
        auto actor=common.actor(id);
        auto self=common.resolved_location(player),other=common.resolved_location(id);
        if(!actor||!actor->alive||!self.ok||!other.ok||!equal_location(self.value,other.value))return fail("CharacterNotVisible");
        CommonCharacterPresentation presentation(common.state(),catalog);
        auto character=presentation.character(id);
        if(!character.ok)return {false,{},character.error};
        return encode_character_presentation(character.value);
    }
}
