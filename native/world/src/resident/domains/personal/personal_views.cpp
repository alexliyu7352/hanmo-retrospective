// 个人战现有页面适配：有限只读DTO复用原生规则，不暴露AI承诺、roll或完整世界。
#include "personal_rules_internal.hpp"
#include "personal_simultaneous_outcomes.hpp"
#include "resident/views/character_presentation.hpp"
namespace hanmo::resident {
    namespace {
    using V=BoundaryValue;
    using D=V::Dict;
    using A=V::Array;
    using namespace personal_detail;
    // 玩家失败沿旧短形结果，查询不产生世界变更或补造空会话。
    V failure(const StableId& code){
        return V{D{{"ok",V{false}},{"code",V{code}}}};
    }
    V strings(const std::vector<StableId>& ids){
        return boundary::string_array(ids);
    }
    V point(PersonalPoint p){
        return V{A{V{p.x},V{p.y}}};
    }
    V values(const OrderedTable<Integer>& table){
        D rows;
        for(auto& row:table.rows())rows.emplace_back(row.first,V{row.second});
        return V{rows};
    }
    // 可见身体与可选敌人分别投影，倒地身体保留而离场者不再可点。
    std::vector<StableId> present(const PersonalBattle& b){
        std::vector<StableId> r;
        for(auto& id:ids(b.units))if(!b.units.find(id)->withdrawn_round)r.push_back(id);
        return r;
    }
    std::vector<StableId> targets(const PersonalBattle& b,const StableId& actor){
        std::vector<StableId> r;
        auto u=b.units.find(actor);
        if(!u)return r;
        for(auto& id:ids(b.units))if(enemy(*u,*b.units.find(id)))r.push_back(id);
        return r;
    }
    // 攻击预览保原主目标字段与可选多目标数组，不引入随机结果。
    V attack(const Outcome<PersonalAttackPreview>& preview){
        if(!preview.ok)return failure(preview.error.original.code);
        D d{{"ok",V{true}},{"damage",V{preview.value.damage}}};
        if(preview.value.hit_bp)d.emplace_back("hit_bp",V{*preview.value.hit_bp});
        if(preview.value.targets){
            A rows;
            for(auto& target:*preview.value.targets){
                D row{{"target_id",V{target.target_id}},{"damage",V{target.damage}}};
                if(target.hit_bp)row.emplace_back("hit_bp",V{*target.hit_bp});
                rows.push_back(V{row});
            }
            d.emplace_back("targets",V{rows});
        }
        return V{d};
    }
    // 控制者优先，其他参与身份取最后fallback，沿旧插入顺序选择。
    StableId selected(const PersonalState& s,const StableId& player){
        StableId fallback;
        for(auto& row:s.battles.rows()){
            auto& b=row.second;
            if(b.phase=="closed"||!b.units.find(player))continue;
            if(b.controller_id==player)return row.first;
            fallback=row.first;
        }
        return fallback;
    }
    }
    // 原目标转稳定语义码，不包含世界奖励、AI候选或未发生的成功。
    BoundaryValue personal_goal_view(const PersonalBattle& b,const PersonalCatalog& c){
        auto& o=*c.objectives.find(b.objective_id);
        std::vector<StableId> protected_ids;
        auto side=b.units.find(b.controller_id)->side;
        for(auto& id:personal_detail::ids(b.units))if(b.units.find(id)->side==side&&b.units.find(id)->protected_actor)protected_ids.push_back(id);
        StableId goal="defeat_opponents",timeout="draw";
        if(o.kind=="escort")goal=o.escort_model=="exit_v1"?"protected_exit":"protect_and_defeat_opponents";
        if(o.escort_model=="exit_v1")timeout="protected_exit_or_truce";
        else if(o.kind=="sparring"&&b.units.size()==2)timeout="hp_ratio_10pp";
        D fields{{"kind",V{o.kind}},{"goal_rule",V{goal}},{"timeout_rule",V{timeout}},{"max_rounds",V{o.max_rounds}},{"allow_withdrawal",V{o.allow_withdrawal}},{"protected_actor_ids",strings(protected_ids)},{"initial_stamina",V{o.initial_stamina.value_or(50)}}};
        if(personal_simultaneous_outcome_selected(c,o.id))fields.emplace_back("mutual_defeat_rule",V{StableId{"draw_v1"}});
        return V{fields};
    }
    // 事件白名单只含已经发生的公开效果，移动坐标和随机roll绝不透传。
    BoundaryValue personal_events_view(const PersonalBattle& b){
        A rows;
        for(auto& e:b.last_events){
            if(!e.action||(*e.action!="attack"&&*e.action!="skill"&&*e.action!="counter"&&*e.action!="withdraw"&&*e.action!="objective"&&*e.action!="outcome"&&*e.action!="stamina"&&*e.action!="status"&&*e.action!="defend"&&*e.action!="wait"))continue;
            D row;
            if(e.actor_id)row.emplace_back("actor_id",V{*e.actor_id});
            if(e.target_id)row.emplace_back("target_id",V{*e.target_id});
            row.emplace_back("action",V{*e.action});
            if(e.skill_id)row.emplace_back("skill_id",V{*e.skill_id});
            if(e.damage)row.emplace_back("damage",V{*e.damage});
            if(e.code)row.emplace_back("code",V{*e.code});
            if(e.status)row.emplace_back("status",V{*e.status});
            if(e.expires_round)row.emplace_back("expires_round",V{*e.expires_round});
            if(e.amount)row.emplace_back("amount",V{*e.amount});
            if(e.hit_bp)row.emplace_back("hit_bp",V{*e.hit_bp});
            rows.push_back(V{row});
        }
        return V{rows};
    }
    // 战果只由已结束会话提供，正HP撤离与倒地分别可见。
    BoundaryValue personal_result_view(const PersonalBattle& b,const PersonalCatalog& c){
        return encode_personal_result_projection(personal_result_projection(b,c));
    }
    // 候选说明复用实际计划校验、移动圈和攻击投影，不能据此推定未来伤害。
    BoundaryValue personal_plan_view(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& intent){
        return encode_personal_plan_projection(personal_plan_projection(b,c,actor,intent));
    }
    // 最低身份戳仅核真实参与引用，不消费角色外貌、钱包或随机。
    BoundaryValue personal_battle_context(const CommonStore& common,const PersonalState& state,const StableId& battle_id){
        auto b=state.battles.find(battle_id);
        if(!b)return failure("UnknownBattle");
        auto player=common.state().metadata.player_actor_id;
        if(common.revision()<0||b->id!=battle_id||!b->units.find(player)||!b->units.find(b->controller_id))return failure("InvalidPersonalBattleViewSource");
        return V{D{{"ok",V{true}},{"actor_id",V{player}},{"revision",V{common.revision()}},{"battle_id",V{battle_id}},{"round",V{b->round}},{"phase",V{b->phase}},{"controller_id",V{b->controller_id}}}};
    }
    // 画布候选只算当前选择会话，不从公开DTO反造假的空AI战场。
    BoundaryValue personal_battle_geometry(const CommonStore& common,const PersonalState& state,const PersonalCatalog& c,const StableId& battle_id,const PersonalPlan& draft){
        auto stamp=personal_battle_context(common,state,battle_id);
        if(!stamp.find("ok")||!std::get<bool>(stamp.find("ok")->data))return stamp;
        auto b=state.battles.find(battle_id);
        D d{{"ok",V{true}},{"movement_distance",V{personal_movement_distance(*b,c,b->controller_id,draft.skill_id,draft.action)}},{"fallback_tactical_preview",personal_plan_view(*b,c,b->controller_id,draft)}};
        for(auto& row:*stamp.as<D>())if(row.first!="ok")d.push_back(row);
        return V{d};
    }
    // 当前页面公开身体字段严格沿旧白名单，仅本人可见冻结护甲反馈。
    BoundaryValue personal_battle_view(const CommonStore& common,const PersonalState& state,const PersonalCatalog& c,const CommonCatalog& cc){
        auto player=common.state().metadata.player_actor_id;
        if(common.revision()<0)return failure("InvalidPersonalBattleViewSource");
        auto id=selected(state,player);
        if(id.empty())return V{D{{"ok",V{true}},{"actor_id",V{player}},{"revision",V{common.revision()}},{"battle_id",V{StableId{}}}}};
        auto stamp=personal_battle_context(common,state,id);
        if(!std::get<bool>(stamp.find("ok")->data))return stamp;
        auto& b=*state.battles.find(id);
        auto own=b.units.find(player);
        if(!own||!c.objectives.find(b.objective_id)||!c.arenas.find(b.battlefield_id)||!personal_detail::armor_valid(*own,c).ok)return failure("InvalidPersonalBattleViewSource");
        D units,names,characters;
        CommonCharacterPresentation presenter(common.state(),cc);
        for(auto& row:b.units.rows()){
            auto& u=row.second;
            D body{{"actor_id",V{u.actor_id}},{"side",V{u.side}},{"position",point(u.position)},{"heading",V{u.heading}},{"radius",V{u.radius}},{"hp",V{u.hp}},{"initial_hp",V{u.initial_hp}},{"stamina",V{u.stamina}},{"protected",V{u.protected_actor}},{"statuses",values(u.statuses)}};
            if(row.first==b.controller_id){
                body.emplace_back("weapon_id",V{u.weapon_id});
                body.emplace_back("learned",values(u.learned));
                body.emplace_back("recovery_until",V{u.recovery_until});
            }
            units.emplace_back(row.first,V{body});
            auto actor=common.state().actors.find(row.first);
            StableId name=row.first;
            if(actor&&actor->display_name)name=*actor->display_name;
            else if(auto def=cc.actor_names.find(actor&&actor->prototype_id?*actor->prototype_id:row.first)){
                if(auto authored=cc.texts.find(def->name_key))name=*authored;
            }
            names.emplace_back(row.first,V{name});
            auto character=presenter.character(row.first);
            if(!character.ok)characters.emplace_back(row.first,failure(character.error.original.code));
            else{
                auto encoded=encode_character_presentation(character.value);
                characters.emplace_back(row.first,encoded.ok?encoded.value:failure(encoded.error.original.code));
            }
        }
        A feedback;
        for(auto& event:b.last_events)if(event.target_id&&event.damage)feedback.push_back(V{D{{"target_id",V{*event.target_id}},{"damage",V{*event.damage}}}});
        V public_battle{D{{"id",V{b.id}},{"controller_id",V{b.controller_id}},{"battlefield_id",V{b.battlefield_id}},{"objective_id",V{b.objective_id}},{"round",V{b.round}},{"phase",V{b.phase}},{"outcome",V{b.outcome}},{"units",V{units}},{"damage_feedback",V{feedback}}}};
        return V{D{{"ok",V{true}},{"actor_id",V{player}},{"revision",V{common.revision()}},{"battle_id",V{id}},{"battle",public_battle},{"equipment_feedback",V{D{{"actor_id",V{player}},{"armor_id",V{own->armor_id.value_or("")}},{"armor_defense",V{own->armor_defense.value_or(0)}}}}},{"present_actor_ids",strings(present(b))},{"selectable_target_ids",strings(targets(b,b.controller_id))},{"objective",personal_goal_view(b,c)},{"events",personal_events_view(b)},{"result",personal_result_view(b,c)},{"display_names",V{names}},{"character_inputs",V{characters}}}};
    }
}
namespace hanmo::resident {
    namespace {
    // 纯有类型目标与事件快照供临时报价使用，不复制可写世界或未来意图。
    PersonalGoalProjection goal_projection(const PersonalBattle& b,const PersonalCatalog& c){
        auto& o=*c.objectives.find(b.objective_id);
        PersonalGoalProjection r;
        r.kind=o.kind;
        r.goal_rule="defeat_opponents";
        r.timeout_rule="draw";
        if(personal_simultaneous_outcome_selected(c,o.id))r.mutual_defeat_rule="draw_v1";
        r.max_rounds=o.max_rounds;
        r.allow_withdrawal=o.allow_withdrawal;
        r.initial_stamina=o.initial_stamina.value_or(50);
        for(auto& id:personal_detail::ids(b.units))if(b.units.find(id)->side==b.units.find(b.controller_id)->side&&b.units.find(id)->protected_actor)r.protected_actor_ids.push_back(id);
        if(o.kind=="escort")r.goal_rule=o.escort_model=="exit_v1"?"protected_exit":"protect_and_defeat_opponents";
        if(o.escort_model=="exit_v1")r.timeout_rule="protected_exit_or_truce";
        else if(o.kind=="sparring"&&b.units.size()==2)r.timeout_rule="hp_ratio_10pp";
        return r;
    }
    V encode_goal(const PersonalGoalProjection& g){
        D fields{{"kind",V{g.kind}},{"goal_rule",V{g.goal_rule}},{"timeout_rule",V{g.timeout_rule}},{"max_rounds",V{g.max_rounds}},{"allow_withdrawal",V{g.allow_withdrawal}},{"protected_actor_ids",strings(g.protected_actor_ids)},{"initial_stamina",V{g.initial_stamina}}};
        if(g.mutual_defeat_rule)fields.emplace_back("mutual_defeat_rule",V{*g.mutual_defeat_rule});
        return V{fields};
    }
    V encode_events(const std::vector<PersonalEvent>& events){
        PersonalBattle view;
        view.last_events=events;
        return personal_events_view(view);
    }
    }
    // 报价冻结当前可见候选说明，之后编码不需要读取当时的世界或目录。
    PersonalPlanProjection personal_plan_projection(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& intent){
        PersonalPlanProjection r;
        auto checked=personal_validate_plan(b,c,actor,intent);
        if(!checked.ok){
            r.code=checked.error.original.code;
            return r;
        }
        r.ok=true;
        auto& p=checked.value;
        auto& u=*b.units.find(actor);
        auto s=c.skills.find(p.skill_id);
        auto base=personal_detail::weapon(u,c);
        r.movement_distance=personal_movement_distance(b,c,actor,p.skill_id,p.action);
        r.selectable=targets(b,actor);
        r.present=present(b);
        r.goal=goal_projection(b,c);
        r.events=b.last_events;
        for(auto& e:r.events){
            e.roll.reset();
            e.position.reset();
        }
        if(base)r.weapon=personal_detail::effective_weapon(*base,s);
        if(p.action=="skill"&&s)r.nominal_stamina_cost=s->stamina_cost;
        else if(p.action=="attack"&&base)r.nominal_stamina_cost=base->stamina_cost;
        if(s){
            r.requires_stationary=s->requires_stationary;
            r.guard_facing=s->guard_facing;
        }
        if(p.action=="attack"||(p.action=="skill"&&s&&s->kind=="strike")){
            r.attack=personal_preview_attack(b,c,actor,p);
            for(auto& id:r.selectable){
                auto candidate=p;
                candidate.target_id=id;
                if(personal_preview_attack(b,c,actor,candidate).ok)r.attackable.push_back(id);
            }
        }
        return r;
    }
    // 战果报价只冻结公开人数、状态和已发生事件，不保未来计划。
    PersonalResultProjection personal_result_projection(const PersonalBattle& b,const PersonalCatalog& c){
        PersonalResultProjection r;
        if(b.phase!="result"&&b.phase!="closed"){
            r.code="NoBattleResult";
            return r;
        }
        r.ok=true;
        r.outcome=b.outcome;
        r.goal=goal_projection(b,c);
        r.events=b.last_events;
        for(auto& e:r.events){
            e.roll.reset();
            e.position.reset();
        }
        for(auto& id:personal_detail::ids(b.units)){
            auto& u=*b.units.find(id);
            r.people.push_back({id,u.withdrawn_round?"withdrawn":u.hp<=0?"down":"standing",u.side,u.hp,u.initial_hp});
            if(u.hp<=0)r.defeated.push_back(id);
            if(u.withdrawn_round)r.withdrawn.push_back(id);
        }
        for(auto& event:b.last_events)if(event.action=="objective"&&(event.code=="EscortExitReached"||event.code=="EscortExitTimeout")&&std::find(r.reasons.begin(),r.reasons.end(),*event.code)==r.reasons.end())r.reasons.push_back(*event.code);
        return r;
    }
    // 有限报价编码保持旧字段顺序及可选字段存在位。
    BoundaryValue encode_personal_plan_projection(const PersonalPlanProjection& r){
        if(!r.ok)return failure(r.code);
        D d{{"ok",V{true}},{"basis",V{StableId{"current_snapshot_candidate"}}},{"may_change_after_simultaneous_movement",V{true}},{"movement_distance",V{r.movement_distance}},{"selectable_target_ids",strings(r.selectable)},{"present_actor_ids",strings(r.present)},{"attackable_target_ids",strings(r.attackable)},{"objective",encode_goal(r.goal)},{"nominal_stamina_cost",V{r.nominal_stamina_cost}},{"recent_events",encode_events(r.events)}};
        if(r.weapon){
            auto& w=*r.weapon;
            d.emplace_back("effective_weapon",V{D{{"category",V{w.category}},{"minimum_range",V{w.minimum_range}},{"maximum_range",V{w.maximum_range}},{"arc_degrees",V{w.arc_degrees}}}});
        }
        if(r.requires_stationary)d.emplace_back("requires_stationary",V{*r.requires_stationary});
        if(r.guard_facing)d.emplace_back("guard_facing",V{*r.guard_facing});
        if(r.attack)d.emplace_back("attack",attack(*r.attack));
        return V{d};
    }
    BoundaryValue encode_personal_result_projection(const PersonalResultProjection& r){
        if(!r.ok)return failure(r.code);
        A people;
        for(auto& u:r.people)people.push_back(V{D{{"actor_id",V{u.actor_id}},{"side",V{u.side}},{"hp",V{u.hp}},{"initial_hp",V{u.initial_hp}},{"status",V{u.status}}}});
        return V{D{{"ok",V{true}},{"outcome",V{r.outcome}},{"objective",encode_goal(r.goal)},{"participants",V{people}},{"defeated_actor_ids",strings(r.defeated)},{"withdrawn_actor_ids",strings(r.withdrawn)},{"reason_codes",strings(r.reasons)},{"events",encode_events(r.events)}}};
    }
}
