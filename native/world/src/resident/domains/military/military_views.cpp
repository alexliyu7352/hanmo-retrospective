#include "resident/domains/military/military_model.hpp"
#include "military_command_resources.hpp"
#include "military_field_terrain.hpp"
#include "military_commanded_damage.hpp"
#include "military_sequential_rounds.hpp"
// 原MilitaryBattlePage逐字段投影；所有移动圈与攻击目标复用实际纯轮次规则，表现不另算战果。
#include "military_views.hpp"
#include "military_rout_status.hpp"
#include "battle_codec.hpp"
#include "military_commander.hpp"
#include "battle_request_codec.hpp"
#include "battle_world.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
bool has(const std::vector<StableId>& a,const StableId& id){return std::find(a.begin(),a.end(),id)!=a.end();}
const BattleUnit* unit(const MilitaryBattleRecord& b,const StableId& id){for(auto& u:b.units)if(u.unit_id==id)return &u;return nullptr;}
const MilitaryOrder* order(const MilitaryOrders& o,const StableId& id){for(auto& p:o)if(p.first==id)return &p.second;return nullptr;}
V strings(const std::vector<StableId>& a){A out;for(auto& x:a)out.push_back(V{x});return V{std::move(out)};}
V point(HexPoint p){return V{A{V{p.q},V{p.r}}};}
V path(const HexPath& p){A route;for(auto x:p.path)route.push_back(point(x));return V{D{{"position",point(p.position)},{"path",V{std::move(route)}},{"cost",V{p.cost}}}};}
V failure(const RuleError& e){return V{D{{"ok",V{false}},{"code",V{e.original.code}},{"path",V{e.original.path}}}};}
V identity(const CommonStore& c,const MilitaryBattleRecord& b){return V{D{{"actor_id",V{c.state().metadata.player_actor_id}},{"revision",V{c.revision()}},{"battle_id",V{b.id}},{"round",V{b.round}},{"phase",V{b.phase}}}};}
V quote(const Outcome<MilitaryOrderQuote>& result){if(!result.ok)return failure(result.error);auto& q=result.value;A route;for(auto p:q.path)route.push_back(point(p));D d{{"ok",V{true}},{"order",battle_codec::encode(q.order)}};
#define OPT(n) if(q.n)d.emplace_back(#n,V{*q.n})
OPT(incoming_damage_bp);OPT(support_share_bp);OPT(hit_chance);OPT(casualties_min);OPT(casualties_max);
#undef OPT
d.emplace_back("path",V{route});d.emplace_back("cost",V{q.cost});return V{std::move(d)};}
// 条件来源最多保留最近六行，排序与GD一致；总数单独记录，不复制或修改永久事实。
void remember_condition(A& history,V row){history.push_back(std::move(row));std::stable_sort(history.begin(),history.end(),[](const V& a,const V& b){auto at=*a.find("tick")->as<Integer>(),bt=*b.find("tick")->as<Integer>();if(at!=bt)return at>bt;auto& ak=*a.find("kind")->as<StableId>();auto& bk=*b.find("kind")->as<StableId>();if(ak!=bk)return ak<bk;return *a.find("source_id")->as<StableId>()<*b.find("source_id")->as<StableId>();});if(history.size()>6)history.pop_back();}
// 原可控门后才读取本部现状、作者初值及实际课时/行军身份；缺条件不造默认值，不输出整事实或敌军私有历史。
V unit_condition(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& id){
    auto source=find(s.military_units,id);if(!source||!source->training||!source->fatigue)return V{D{}};
    V initial{D{}};if(auto camp=find(cat.garrisons,source->source_garrison_id))if(auto definition=find(camp->units,id);definition&&definition->initial_training&&definition->initial_fatigue)initial=V{D{{"garrison_id",V{camp->id}},{"unit_id",V{id}},{"training",V{*definition->initial_training}},{"fatigue",V{*definition->initial_fatigue}}}};
    Integer count=0;A history;
    for(const auto& fact:s.training_facts)if(fact.terms.unit_id==id)for(const auto& lesson:fact.lessons){if(lesson.tick>c.tick())continue;++count;remember_condition(history,V{D{{"kind",V{StableId{"military_training"}}},{"source_id",V{fact.id}},{"tick",V{lesson.tick}},{"supply_fact_id",V{lesson.supply_fact_id}},{"training_before",V{lesson.training_before}},{"training_after",V{lesson.training_after}},{"fatigue_before",V{lesson.fatigue_before}},{"fatigue_after",V{lesson.fatigue_after}}}});}
    for(const auto& fact:s.march_facts){if(fact.rule!="terrain_march_v1"||fact.tick>c.tick())continue;for(const auto& profile:fact.units)if(profile.unit_id==id){++count;remember_condition(history,V{D{{"kind",V{StableId{"military_march"}}},{"source_id",V{fact.id}},{"tick",V{fact.tick}},{"supply_fact_id",V{fact.supply_fact_id}},{"training_before",V{profile.training}},{"training_after",V{profile.training}},{"fatigue_before",V{profile.fatigue_before}},{"fatigue_after",V{profile.fatigue_after}},{"rule",V{fact.rule}},{"action_id",V{fact.movement.action_id}},{"plan_id",V{fact.movement.plan_id}}}});break;}}
    for(const auto& fact:s.garrison_rest.facts)if(fact.unit_id==id&&fact.tick<=c.tick()){++count;remember_condition(history,V{D{{"kind",V{StableId{"military_garrison_rest"}}},{"source_id",V{fact.id}},{"tick",V{fact.tick}},{"supply_fact_id",V{fact.supply_fact_id}},{"training_before",V{fact.training}},{"training_after",V{fact.training}},{"fatigue_before",V{fact.fatigue_before}},{"fatigue_after",V{fact.fatigue_after}}}});}
    return V{D{{"training",V{*source->training}},{"fatigue",V{*source->fatigue}},{"source",V{D{{"kind",V{StableId{"military_unit"}}},{"unit_id",V{id}}}}},{"initial_source",std::move(initial)},{"history_count",V{count}},{"history",V{std::move(history)}}}};
}
// 本人的有限请求历史仅来自已完成裁定；不公开其它人的请求或尚未裁定AI草案。
V request_page(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryBattleRecord& battle){
 if(!cat.request_policy)return V{D{}};const auto& actor=c.state().metadata.player_actor_id;Integer used=0,count=0;A history;
 for(const auto& fact:s.request_facts)if(fact.actor_id==actor&&fact.request.battle_id==battle.id){++count;if(fact.round==battle.round)++used;history.push_back(military_request_codec::encode_answer(military_request_codec::answer(fact)));if(history.size()>6)history.erase(history.begin());}
 return V{D{{"profile",V{StableId{"request_policy/1"}}},{"per_actor_round_limit",V{cat.request_policy->per_actor_round_limit}},{"used_this_round",V{used}},{"boundary_open",V{battle.command_scope=="unit"&&battle.phase=="planning"&&!battle.plan_submitted}},{"history_count",V{count}},{"history",V{std::move(history)}}}};
}
bool available(const MilitaryBattleRecord* b,const StableId& player){return b&&b->phase!="closed"&&b->controller_actor_id==player;}
bool controllable(const MilitaryBattleRecord& b,const BattleUnit& u,const StableId& player){if(b.phase!="planning"||b.controller_actor_id!=player||!has(b.command_unit_ids,u.unit_id)||u.side!=b.controller_side||u.withdrawn||u.fit.empty()||u.morale<=0||military_commander_loss(u)==2)return false;return b.command_scope=="unit"?u.commander_actor_id==player:u.army_id==b.command_army_id;}
// 目标说明与旧中文文本和计数一致，不从胜率或模拟战果替代当前已完成进度。
StableId objective_text(const MilitaryBattleRecord& b,const BattleObjectiveDefinition& o){auto prefix=o.side==b.controller_side?"我军":"敌军";Integer escaped=0;for(auto& id:b.objective_state.escaped_unit_ids)if(unit(b,id)->side==o.side)++escaped;if(o.kind=="repel")return "击退对方全部作战部队。";if(o.kind=="hold")return StableId(prefix)+"守住标记地，连续 "+std::to_string(o.hold_rounds)+" 轮（已守 "+std::to_string(b.objective_state.hold_rounds)+" 轮）。";if(o.kind=="breakthrough")return StableId(prefix)+"至少 "+std::to_string(o.required_units)+" 部由本方退路突破（已离场 "+std::to_string(escaped)+" 部）。";if(o.kind=="escort")return StableId(prefix)+"护送指定 "+std::to_string(b.objective_unit_ids.size())+" 部完整离场；保护部队失去全部战力即失败。";if(o.kind=="cover_retreat")return StableId(prefix)+"掩护指定部队，至少 "+std::to_string(o.required_units)+" 部经退路离场（已离场 "+std::to_string(escaped)+" 部）。";return "未知作战目标，请检查内容定义。";}
}
StableId active_military_battle(const MilitaryState& s,const StableId& actor){std::vector<const MilitaryBattleRecord*> battles;for(auto& b:s.battles)battles.push_back(&b);std::sort(battles.begin(),battles.end(),[](auto a,auto b){return a->id<b->id;});for(auto b:battles)if(available(b,actor)&&b->phase=="planning"&&!b->plan_submitted)return b->id;for(auto b:battles)if(available(b,actor))return b->id;return {};}
Outcome<BoundaryValue> military_battle_page(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const CommonCatalog& common,const StableId& selected){const auto& player=c.state().metadata.player_actor_id;auto id=selected.empty()?active_military_battle(s,player):selected;if(id.empty())return {true,V{D{{"ok",V{true}},{"active",V{false}}}},{}};auto b=find(s.battles,id);if(!available(b,player))return {false,{},common_failure("MilitaryBattleUnavailable").error};auto field=find(cat.battle_catalog.battlefields,b->battlefield_id);auto goal=find(cat.battle_catalog.objectives,b->objective_id);if(!field||!goal)return {false,{},common_failure("InvalidMilitaryBattleViewSource").error};CommonPresentation display(c.state(),common);std::vector<MilitarySequentialTurn> sequential;if(military_sequential_round_in_use(*b,cat.battle_catalog)&&b->phase=="planning"){auto read=query_military_sequential_turns(*b,cat.battle_catalog);if(!read.ok)return {false,{},read.error};sequential=std::move(read.value);}D units;for(auto& u:b->units){auto troop=find(cat.battle_catalog.troop_types,u.troop_type_id);if(!troop)return {false,{},common_failure("InvalidMilitaryBattleViewSource").error};auto name=display.display_name(u.commander_actor_id);if(!name.ok)return {false,{},name.error};A tactics;for(auto& t:troop->tactics)tactics.push_back(V{D{{"id",V{t.id}},{"kind",V{t.kind}},{"point_cost",V{t.point_cost}},{"display_name",V{t.name}}}});D statuses;for(auto& p:u.statuses)statuses.emplace_back(p.first,V{p.second});D row{{"unit_id",V{u.unit_id}},{"display_name",V{name.value+"部"}},{"troop_display_name",V{troop->has_name_key?troop->name:troop->category=="infantry"?"步卒":troop->category=="archer"?"弓手":troop->category=="cavalry"?"骑兵":"部队"}},{"category",V{troop->category}},{"portrait_kind",V{troop->portrait_kind}},{"controllable",V{controllable(*b,u,player)}},{"command_member",V{has(b->command_unit_ids,u.unit_id)}},{"protected",V{has(b->objective_unit_ids,u.unit_id)}},{"tactics",V{tactics}},{"position",point(u.position)},{"heading",V{u.heading}},{"side",V{u.side}},{"morale",V{u.morale}},{"tactic_points",V{u.tactic_points}},{"withdrawn",V{u.withdrawn}},{"statuses",V{statuses}},{"fit_count",V{Integer(u.fit.size())}},{"wounded_count",V{Integer(u.wounded.size())}},{"dead_count",V{Integer(u.dead.size())}},{"captured_count",V{Integer(u.captured.size())}},{"deserted_count",V{Integer(u.deserted.size())}},{"tactic_capacity",V{troop->tactic_capacity}},{"movement",V{troop->movement}},{"minimum_range",V{troop->minimum_range}},{"maximum_range",V{troop->maximum_range}}};if(military_rout_rule_in_use(*b,cat.battle_catalog)){Integer began=0,blocked=0;for(const auto& state:u.statuses){if(state.first=="military_rout_started_v1")began=state.second;else if(state.first=="military_rout_blocked_v1")blocked=state.second;}StableId source;Integer source_round=0;for(const auto& proof:s.rout.rounds)if(proof.battle_id==b->id){const auto* previous=unit(proof.before,u.unit_id);const auto* after=unit(proof.after,u.unit_id);if(after&&previous&&(military_unit_routing(*after)||after->withdrawn!=previous->withdrawn||after->captured!=previous->captured)){source=proof.id;source_round=proof.round;}}row.emplace_back("rout",V{D{{"active",V{military_unit_routing(u)}},{"started_round",V{began}},{"blocked_rounds",V{blocked}},{"source_fact_id",V{source}},{"source_round",V{source_round}}}});}
if(b->command_shock)for(const auto& cause:b->command_shock->causes)if(cause.army_id==u.army_id)for(const auto& fact:s.rout.rounds)if(fact.battle_id==b->id&&fact.round==cause.round)for(const auto& event:fact.after.last_events)if(event.kind=="command_shock"&&event.unit_id==u.unit_id&&event.target_id==std::optional<StableId>{cause.unit_id})row.emplace_back("command_shock",V{D{{"source_fact_id",V{fact.id}},{"source_round",V{cause.round}},{"leader_unit_id",V{cause.unit_id}},{"cause",V{cause.kind}},{"morale_change",V{event.morale.value_or(0)}}}});
if(b->combat_conditions&&has(b->command_unit_ids,u.unit_id))for(const auto& frozen:b->combat_conditions->units)if(frozen.unit_id==u.unit_id){D proof{{"entry_tick",V{b->combat_conditions->entry_tick}},{"efficiency_bp",V{frozen.efficiency_bp}},{"last_condition_fact_id",V{frozen.last_condition_fact_id}},{"configured",V{frozen.training.has_value()}}};if(frozen.training){proof.emplace_back("training",V{*frozen.training});proof.emplace_back("fatigue",V{*frozen.fatigue});}row.emplace_back("combat_conditions",V{std::move(proof)});}
// 只显示本人真实可指挥部队的原入战资格，不泄漏敌方完整XP或当前人物表。
if(b->commanded_damage&&has(b->command_unit_ids,u.unit_id))for(const auto& frozen:b->commanded_damage->units)if(frozen.unit_id==u.unit_id){auto strength=military_commanded_strength(frozen.leadership,frozen.level,frozen.training);if(strength.ok)row.emplace_back("commanded_damage",V{D{{"entry_tick",V{b->commanded_damage->entry_tick}},{"commander_actor_id",V{frozen.commander_actor_id}},{"skill_id",V{frozen.skill_id}},{"leadership",V{frozen.leadership}},{"skill_level",V{frozen.level}},{"training",V{frozen.training}},{"strength",V{strength.value}},{"command_source_id",V{frozen.command_source.id}}}});}

if(has(b->command_unit_ids,u.unit_id))for(std::size_t i=0;i<sequential.size();++i)if(sequential[i].unit_id==u.unit_id)row.emplace_back("sequential",V{D{{"round",V{b->round}},{"ordinal",V{Integer(i+1)}},{"initiative",V{sequential[i].initiative}},{"entry_tick",V{b->commanded_damage->entry_tick}}}});

// 本人获权且确在本轮队列才报价恢复；结束/跳过部队不显示虚构的下次可用额。
if(b->command_resources&&has(b->command_unit_ids,u.unit_id)){bool queued=false;for(const auto& turn:sequential)if(turn.unit_id==u.unit_id)queued=true;D resource{{"entry_tick",V{b->command_resources->entry_tick}},{"before",V{u.tactic_points}},{"capacity",V{Integer(100)}},{"queued",V{queued}}};if(queued){auto quote=query_military_command_resources(*b,*b->command_resources,u.unit_id);if(!quote.ok)return {false,{},quote.error};resource.emplace_back("recovered",V{quote.value.recovered});resource.emplace_back("available",V{quote.value.available});}row.emplace_back("command_resources",V{std::move(resource)});}
if(field->field_terrain){const auto* tile=military_hex::cell(*field,u.position);if(!tile)return {false,{},common_failure("MilitaryFieldTerrainViewSourceRequired",u.unit_id).error};row.emplace_back("field_terrain",V{D{{"profile",V{field->field_terrain->profile}},{"terrain",V{tile->terrain}},{"movement_cost",V{military_hex::movement_cost(*field,u.position,*troop)}},{"maximum_range",V{military_field_attack_maximum_range(*field,*troop,u.position)}},{"zone_retreat",V{military_field_retreat_required(*b,cat.battle_catalog,u.unit_id,{})}}}});}if(controllable(*b,u,player)){auto condition=unit_condition(c,s,cat,u.unit_id);if(!condition.as<D>()->empty())row.emplace_back("condition",std::move(condition));}units.emplace_back(u.unit_id,V{std::move(row)});}A cells,exits;for(auto& cell:field->cells)cells.push_back(V{D{{"q",V{cell.q}},{"r",V{cell.r}},{"terrain",V{cell.terrain}}}});for(auto& e:field->exits)exits.push_back(V{D{{"position",point(e.position)},{"side",V{e.side}}}});D geometry{{"cells",V{cells}},{"exits",V{exits}},{"visual_tag",V{field->visual_tag.value_or("")}}};if(goal->point)geometry.emplace_back("point",point(*goal->point));A events;for(auto& e:b->last_events){D row{{"kind",V{e.kind}}};if(!e.unit_id.empty())row.emplace_back("unit_id",V{e.unit_id});
#define OPT(n) if(e.n)row.emplace_back(#n,V{*e.n})
OPT(target_id);OPT(casualties);OPT(code);OPT(captured);OPT(deserted);
#undef OPT
events.push_back(V{std::move(row)});}V result=b->result?battle_codec::encode(*b->result):V{D{}};V logistics{D{}};auto army=find(s.armies,b->command_army_id);if(!army&&b->command_army_id.empty())army=active_army_for_actor(s,player);if(army&&army->commander_actor_id==player){auto q=preview_army_supply(c,s,cat,army->id);if(!q.ok)logistics=failure(q.error);else{auto& p=q.value;logistics=V{D{{"ok",V{true}},{"food_item_id",V{p.food_item_id}},{"mouth_count",V{p.mouth_count}},{"required",V{p.required}},{"available",V{p.available}},{"full_ticks",V{p.full_ticks}},{"shortage",V{p.shortage}}}};}}D header{{"display_name",V{field->name}},{"stage_visual_tag",V{field->visual_tag.value_or("city")}},{"max_rounds",V{goal->max_rounds}},{"controller_side",V{b->controller_side}},{"command_scope",V{b->command_scope}},{"command_unit_ids",strings(b->command_unit_ids)},{"plan_submitted",V{b->plan_submitted}}};if(military_rout_rule_in_use(*b,cat.battle_catalog))header.emplace_back("rout_retreat",V{StableId{"士气归零后自动沿真实退路撤离，不再接受作战指令。连续两轮无路可走且有相邻有效敌军看管才归俘；抵达本方出口保留现存兵员。"}});D page{{"ok",V{true}},{"active",V{true}},{"identity",identity(c,*b)},{"header",V{std::move(header)}},{"units",V{std::move(units)}},{"objective",V{D{{"kind",V{goal->kind}},{"text",V{objective_text(*b,*goal)}}}}},{"result",result},{"events",V{std::move(events)}},{"logistics",logistics},{"geometry",V{std::move(geometry)}}};if(cat.request_policy)page.emplace_back("requests",request_page(c,s,cat,*b));return {true,V{std::move(page)},{}};}
// 草案与实际轮次复用同一围城来源；战法候选按完整当前指令复核，不保留普通移动圈。
Outcome<BoundaryValue> military_battle_draft(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const StableId& id,const StableId& selected,const MilitaryOrders& orders){
    auto b=find(s.battles,id);auto player=c.state().metadata.player_actor_id;
    if(!available(b,player))return {false,{},common_failure("MilitaryBattleUnavailable").error};
    auto sources=military_damage_sources(s,cat,*b);
    if(!sources.ok)return {false,{},sources.error};
    auto validated=validate_military_orders(*b,cat.battle_catalog,player,orders);
    V validation;
    if(!validated.ok)validation=failure(validated.error);
    else{D normalized;for(auto& p:validated.value)normalized.emplace_back(p.first,battle_codec::encode(p.second));validation=V{D{{"ok",V{true}},{"orders",V{std::move(normalized)}}}};}
    auto selected_order=order(orders,selected);V selected_quote{D{}};
    if(selected_order)selected_quote=quote(preview_military_order(*b,cat.battle_catalog,selected,*selected_order,sources.value));
    std::vector<HexPath> movement;std::vector<StableId> attackable,supportable;
    auto chosen=unit(*b,selected);
    if(b->phase=="planning"&&chosen){
        const bool using_tactic=selected_order&&selected_order->action=="tactic";
        bool supporting=false;
        if(using_tactic)if(auto troop=find(cat.battle_catalog.troop_types,chosen->troop_type_id))for(auto& tactic:troop->tactics)if(tactic.id==selected_order->tactic_id&&tactic.kind=="support")supporting=true;
        auto candidates=military_movement_options(*b,cat.battle_catalog,selected,using_tactic?selected_order->tactic_id:StableId{});
        if(candidates.ok){
            if(!using_tactic)movement=std::move(candidates.value);
            else for(auto& option:candidates.value){
                auto candidate=*selected_order;candidate.destination=option.position;
                auto q=preview_military_order(*b,cat.battle_catalog,selected,candidate,sources.value);
                if(q.ok)movement.push_back({candidate.destination,std::move(q.value.path),q.value.cost});
            }
        }
        if(selected_order)for(auto& target:b->units){
            auto candidate=*selected_order;candidate.target_id=target.unit_id;
            if(supporting){if(preview_military_order(*b,cat.battle_catalog,selected,candidate,sources.value).ok)supportable.push_back(target.unit_id);continue;}
            if(target.side==chosen->side)continue;
            if(candidate.action!="tactic"){candidate.action="attack";candidate.tactic_id.clear();}
            if(preview_military_order(*b,cat.battle_catalog,selected,candidate,sources.value).ok)attackable.push_back(target.unit_id);
        }
    }
    A moves;for(auto& m:movement)moves.push_back(path(m));
    return {true,V{D{{"ok",V{true}},{"identity",identity(c,*b)},{"orders_validation",validation},{"selected_quote",selected_quote},{"movement",V{std::move(moves)}},{"attackable",strings(attackable)},{"supportable",strings(supportable)}}},{}};
}
}
