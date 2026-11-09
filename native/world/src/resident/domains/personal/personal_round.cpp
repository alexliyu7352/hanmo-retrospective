// 个人战计划、伤害和同时结算；保留原回合顺序，命中仅消费候选的唯一duel流。
#include "personal_rules_internal.hpp"
#include "personal_simultaneous_outcomes.hpp"
#include <cmath>
#include <set>
namespace hanmo::resident {
    namespace personal_detail {
        // 引用只查当前快照武器，没有临时拳脚或表现资源兜底。
        const PersonalWeapon* weapon(const PersonalUnit& u,const PersonalCatalog& c){
            return c.weapons.find(u.weapon_id);
        }
        // 已掌握与武器资格共同决定主动、心法是否可用。
        Outcome<const PersonalSkill*> skill(const PersonalUnit& u,const PersonalCatalog& c,const StableId& id){
            auto s=c.skills.find(id);
            if(!s)return fail<const PersonalSkill*>("UnknownSkill");
            if(value(u.learned,id)<=0)return fail<const PersonalSkill*>("SkillNotLearned");
            auto w=weapon(u,c);
            if(!w||std::find(s->weapons.begin(),s->weapons.end(),w->category)==s->weapons.end())return fail<const PersonalSkill*>("SkillWeaponMismatch");
            return {true,s,{}};
        }
        // 不合格心法不生效，也不在被动路径扣其主动消耗。
        const PersonalSkill* heart(const PersonalUnit& u,const PersonalCatalog& c){
            if(u.heart_id.empty())return nullptr;
            auto s=skill(u,c,u.heart_id);
            return s.ok&&s.value->kind=="heart"?s.value:nullptr;
        }
        // 射程扩展只修改本次外圈，不改变武器定义和内圈。
        PersonalWeapon effective_weapon(const PersonalWeapon& w,const PersonalSkill* s){
            auto result=w;
            if(s&&s->maximum_range_bonus)result.maximum_range+=*s->maximum_range_bonus;
            return result;
        }
        // 原武器、心法和主动移动加成采用整数熟练度；迟缓最后折半。
        Integer move_distance(const PersonalUnit& u,const PersonalCatalog& c,Integer round,const PersonalSkill* s){
            if(s&&s->requires_stationary.value_or(false))return 0;
            auto w=weapon(u,c);
            Integer d=w?w->move_distance:90;
            if(auto h=heart(u,c))d+=h->move_bonus*value(u.learned,h->id)/100;
            if(s)d+=s->move_bonus*value(u.learned,s->id)/100;
            if(status(u,"slow",round))d/=2;
            return d;
        }
        // 缺省双字段表示无甲；有甲必须与真实冻结定义一致。
        Status armor_valid(const PersonalUnit& u,const PersonalCatalog& c){
            if(!u.armor_id&&!u.armor_defense)return common_success();
            if(!u.armor_id||!u.armor_defense||u.armor_id->empty())return common_failure("InvalidArmorSnapshot");
            auto d=c.armor_defenses.find(*u.armor_id);
            return d&&*d==*u.armor_defense&&*d>=0&&*d<=100?common_success():common_failure("InvalidArmorSnapshot");
        }
        // 脱离是显式轮次事实，不能将倒地或世界死亡代替离场。
        Status withdrawal_valid(const PersonalBattle& b,const PersonalCatalog& c,const StableId& id){
            auto& u=*b.units.find(id);
            if(!u.withdrawn_round)return common_success();
            if(*u.withdrawn_round<1||*u.withdrawn_round>b.round||u.hp<=0)return common_failure("InvalidWithdrawalState");
            auto a=c.arenas.find(b.battlefield_id);
            auto o=c.objectives.find(b.objective_id);
            if(!a||!o||!o->allow_withdrawal||!personal_at_exit(*a,u.side,u.position))return common_failure("InvalidWithdrawalExit");
            if(b.phase=="planning"&&(*u.withdrawn_round>=b.round||id==b.controller_id||u.protected_actor))return common_failure("InvalidWithdrawalPhase");
            if(id==b.controller_id||u.protected_actor){
                auto controller=b.units.find(b.controller_id);
                if(b.outcome!="withdrawn"||!controller||controller->withdrawn_round!=u.withdrawn_round)return common_failure("InvalidGroupWithdrawal");
            }
            return common_success();
        }
        // 每次纯轮次入口检查必要已知形状，非法内容不降级为必中或空AI。
        Status battle_error(const PersonalBattle& b,const PersonalCatalog& c){
            if(b.phase!="planning")return common_failure("BattleNotPlanning");
            auto outcome_policy=validate_personal_simultaneous_outcomes(c);if(!outcome_policy.ok)return outcome_policy;
            if(b.round<1)return common_failure("InvalidBattle");
            auto objective=c.objectives.find(b.objective_id);
            if(!objective||!c.arenas.find(b.battlefield_id))return common_failure("UnknownCombatContent");
            if(!b.units.find(b.controller_id))return common_failure("MissingController");
            bool protected_found=false;
            for(auto& row:b.units.rows()){
                auto& u=row.second;
                if(u.actor_id!=row.first||u.side<0||u.side>1||u.heading<0||u.heading>359||u.radius<0||u.hp<0||u.initial_hp<0||u.hp>u.initial_hp||u.stamina<0||u.stamina>100||u.martial<0||u.recovery_until<0)return common_failure("InvalidCombatant");
                auto withdrawal=withdrawal_valid(b,c,row.first);
                if(!withdrawal.ok)return withdrawal;
                auto armor=armor_valid(u,c);
                if(!armor.ok)return armor;
                if(objective->hit_model=="skill_v1"){
                    if(!u.skill_levels||u.skill_levels->martial<0||u.skill_levels->martial>4||u.skill_levels->archery<0||u.skill_levels->archery>4)return common_failure("InvalidPersonalHitSnapshot");
                }
                else if(u.skill_levels)return common_failure("UnexpectedPersonalHitSnapshot");
                for(auto& x:u.learned.rows())if(x.second<0||x.second>100)return common_failure("InvalidMastery");
                for(auto& x:u.statuses.rows())if(x.first!="slow"&&x.first!="exposed")return common_failure("InvalidStatus");
                if(objective->escort_model=="exit_v1"&&u.protected_actor){
                    if(u.side!=b.units.find(b.controller_id)->side||row.first==b.controller_id)return common_failure("InvalidEscortProtectedSide");
                    protected_found=true;
                }
            }
            if(objective->escort_model=="exit_v1"&&!protected_found)return common_failure("ProtectedPersonMissing");
            return common_success();
        }
        // 原命中公式只读入场普通技艺派生等级，不取随机也不写人物XP。
        Integer hit_chance(const PersonalUnit& a,const PersonalUnit& t,const PersonalWeapon& w,const PersonalSkill* s){
            if(!a.skill_levels||!t.skill_levels)return 0;
            Integer attack=w.category=="bow"?a.skill_levels->archery:a.skill_levels->martial;
            return std::clamp<Integer>(7500+200*(attack-t.skill_levels->martial)+20*(a.martial-t.martial)+(s?s->hit_bonus_bp.value_or(0):0),1500,9500);
        }
        // 目标护甲先减基数，再乘主动、心法和显式侧背倍率，各步沿整数截断。
        Integer attack_power(const PersonalUnit& a,const PersonalUnit& t,const PersonalWeapon& w,const PersonalSkill* s,const PersonalCatalog& c,const PersonalObjective& o){
            Integer r=std::max<Integer>(1,w.damage+a.martial/8-t.martial/20-t.armor_defense.value_or(0));
            if(s)r=scale(r,mastered(s->power_bp,value(a.learned,s->id)));
            if(auto h=heart(a,c))r=scale(r,mastered(h->power_bp,value(a.learned,h->id)));
            Integer facing=10000;
            if(o.facing_damage){
                auto sector=personal_guard_sector(t.position,t.heading,a.position);
                if(sector=="side")facing=o.facing_damage->side_bp;
                else if(sector=="back")facing=o.facing_damage->back_bp;
            }
            return scale(r,facing);
        }
        // 目标心法、暴露、临时守势依次生效，最后保最少一点真实伤害。
        Integer mitigate(Integer power,const PersonalUnit& t,const PersonalCatalog& c,Integer round,Integer guard){
            if(auto h=heart(t,c))power=scale(power,mastered(h->defense_bp,value(t.learned,h->id)));
            if(status(t,"exposed",round))power=scale(power,12500);
            return std::max<Integer>(1,scale(power,guard));
        }
        Integer damage(const PersonalUnit& a,const PersonalUnit& t,const PersonalWeapon& w,const PersonalSkill* s,const PersonalCatalog& c,Integer round,Integer guard,const PersonalObjective& o){
            return mitigate(attack_power(a,t,w,s,c,o),t,c,round,guard);
        }
        // 只有正面保留方向防御，侧后不自动套半伤。
        Integer guard_multiplier(const PersonalUnit& a,const PersonalUnit& t,Integer guard){
            return personal_guard_sector(t.position,t.heading,a.position)=="front"?guard:10000;
        }
        // 主目标合法后才能选附加敌人，距离与ID稳定排序决定唯一随机消费顺序。
        Outcome<std::vector<StableId>> select_targets(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const StableId& target,const PersonalSkill* s,const PersonalWeapon& w){
            auto& a=*b.units.find(actor);
            auto t=b.units.find(target);
            if(!t||!enemy(a,*t))return fail<std::vector<StableId>>("InvalidTarget");
            auto& arena=*c.arenas.find(b.battlefield_id);
            auto legal=personal_can_attack(arena,a.position,t->position,a.heading,w);
            if(!legal.ok)return {false,{},legal.error};
            if(!s||!s->area)return {true,{target},{}};
            auto within=[&](const PersonalUnit& u){
                double x=double(u.position.x)-a.position.x,y=double(u.position.y)-a.position.y,angle=a.heading*3.14159265358979323846/180;
                if(s->area->shape=="line")return x*std::cos(static_cast<float>(angle))+y*std::sin(static_cast<float>(angle))>1e-7&&std::abs(x*std::sin(static_cast<float>(angle))-y*std::cos(static_cast<float>(angle)))<=s->area->half_width.value_or(0)+1e-7;
                double relative=std::atan2(y,x)*180/3.14159265358979323846-a.heading;
                double delta=std::abs(std::fmod(std::fmod(relative+180,360)+360,360)-180);
                return delta<=s->area->arc_degrees.value_or(0)/2.0+1e-7;
            };
            if(!within(*t))return fail<std::vector<StableId>>("OutsideStrikeArea");
            std::vector<StableId> extra;
            for(auto& row:b.units.rows())if(row.first!=target&&enemy(a,row.second)&&within(row.second)&&personal_can_attack(arena,a.position,row.second.position,a.heading,w).ok)extra.push_back(row.first);
            std::sort(extra.begin(),extra.end(),[&](auto& x,auto& y){
                auto px=b.units.find(x)->position,py=b.units.find(y)->position;
            double xx=double(a.position.x)-px.x,xy=double(a.position.y)-px.y,yx=double(a.position.x)-py.x,yy=double(a.position.y)-py.y;
            double dx=xx*xx+xy*xy,dy=yx*yx+yy*yy;
                return dx==dy?x<y:dx<dy;
            });
            std::vector<StableId> result{target};
            for(auto& id:extra){
                if(Integer(result.size())>=s->area->max_targets)break;
                result.push_back(id);
            }
            std::sort(result.begin(),result.end());
            return {true,std::move(result),{}};
        }
        // 计划输出固定五字段，去掉界面临时字段和冷加载原序。
        PersonalPlan plan(PersonalPoint point,Integer heading,const StableId& action,const StableId& target,const StableId& skill){
            PersonalPlan p;
            p.destination=point;
            p.heading=heading;
            p.action=action;
            p.target_id=target;
            p.skill_id=skill;
            return p;
        }
        // 终局只锁定战场，世界伤势、事实和接触解除留给会话拥有者一次性执行。
        void finish(PersonalBattle& b,const PersonalCatalog& c,const StableId& outcome){
            b.phase="result";
            b.outcome=outcome;
            b.ai_plans={};
            auto& o=*c.objectives.find(b.objective_id);
            if(o.escort_model=="exit_v1"&&(outcome=="victory"||(outcome=="draw"&&b.round>=o.max_rounds))){
                PersonalEvent e;
                e.action="objective";
                e.code=outcome=="victory"?"EscortExitReached":"EscortExitTimeout";
                b.last_events.push_back(e);
            }
            PersonalEvent e;
            e.action="outcome";
            e.code=outcome;
            b.last_events.push_back(e);
        }
    }
    using namespace personal_detail;
    // 保护失败与控制者倒地优先；显式护送出口不能被歼敌替代。
    StableId personal_outcome(const PersonalBattle& b,const PersonalCatalog& c){
        auto& controller=*b.units.find(b.controller_id);
        auto& o=*c.objectives.find(b.objective_id);
        for(auto& row:b.units.rows())if(row.second.side==controller.side&&row.second.protected_actor&&row.second.hp<=0)return "protected_lost";
        if(personal_mutual_defeat_draw(b,c))return "draw";
        if(controller.hp<=0)return "defeat";
        if(o.escort_model=="exit_v1"){
            bool found=false;
            for(auto& row:b.units.rows())if(row.second.side==controller.side&&row.second.protected_actor){
                found=true;
                if(row.second.hp<=0||!personal_at_exit(*c.arenas.find(b.battlefield_id),row.second.side,row.second.position))return {};
            }
            return found?"victory":"";
        }
        for(auto& row:b.units.rows())if(enemy(controller,row.second))return {};
        return "victory";
    }
    // 两人切磋沿原十个百分点比例裁决，其他超时保平局。
    StableId personal_limit_outcome(const PersonalBattle& b,const PersonalCatalog& c){
        auto& o=*c.objectives.find(b.objective_id);
        if(o.escort_model=="exit_v1"){
            auto r=personal_outcome(b,c);
            return r.empty()?"draw":r;
        }
        if(o.kind!="sparring"||b.units.size()!=2)return "draw";
        auto& a=*b.units.find(b.controller_id);
        const PersonalUnit* opponent=nullptr;
        for(auto& row:b.units.rows())if(row.first!=b.controller_id)opponent=&row.second;
        if(!opponent||opponent->side==a.side)return "draw";
        auto& t=*opponent;
        for(auto u:{&a,&t})if(u->initial_hp<1||u->initial_hp>100||u->hp<0||u->hp>u->initial_hp)return "draw";
        auto difference=a.hp*t.initial_hp-t.hp*a.initial_hp;
        if(std::abs(difference)*10<a.initial_hp*t.initial_hp)return "draw";
        return difference>0?"victory":"defeat";
    }
    // 身份、招式、移动和消耗按旧首错顺序校验；射程在同步移动后另查。
    Outcome<PersonalPlan> personal_validate_plan(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& p){
        auto valid=battle_error(b,c);
        if(!valid.ok)return {false,{},valid.error};
        auto u=b.units.find(actor);
        if(!u)return fail<PersonalPlan>("UnknownCombatant");
        if(u->hp<=0)return fail<PersonalPlan>("ActorDown");
        if(u->withdrawn_round)return fail<PersonalPlan>("ActorWithdrawn");
        if(p.action!="attack"&&p.action!="defend"&&p.action!="wait"&&p.action!="skill"&&p.action!="withdraw")return fail<PersonalPlan>("InvalidAction");
        if(p.heading<0||p.heading>359)return fail<PersonalPlan>("InvalidPlan");
        if(p.action=="defend"&&p.destination!=u->position)return fail<PersonalPlan>("DefendRequiresStationary");
        const PersonalSkill* s=nullptr;
        if(p.action=="skill"){
            auto learned=skill(*u,c,p.skill_id);
            if(!learned.ok)return {false,{},learned.error};
            s=learned.value;
            if(s->kind=="heart")return fail<PersonalPlan>("PassiveHeart");
        }
        else if(!p.skill_id.empty())return fail<PersonalPlan>("UnexpectedSkill");
        if(s&&s->requires_stationary.value_or(false)&&p.destination!=u->position)return fail<PersonalPlan>("StrikeRequiresStationary");
        if(recovering(*u,b.round)&&(p.destination!=u->position||p.action=="attack"||(s&&s->kind=="strike")))return fail<PersonalPlan>("WeaponRecovering");
        auto movement=personal_can_move(*c.arenas.find(b.battlefield_id),u->position,p.destination,move_distance(*u,c,b.round,s),u->radius);
        if(!movement.ok)return {false,{},movement.error};
        if(p.action=="withdraw"&&!c.objectives.find(b.objective_id)->allow_withdrawal)return fail<PersonalPlan>("WithdrawalForbidden");
        if(p.action=="withdraw"&&u->protected_actor)return fail<PersonalPlan>("ProtectedWithdrawalRequiresController");
        if(p.action=="attack"||(s&&s->kind=="strike")){
            auto t=b.units.find(p.target_id);
            if(!t||t->hp<=0||t->withdrawn_round)return fail<PersonalPlan>("InvalidTarget");
            if(t->side==u->side)return fail<PersonalPlan>("FriendlyTarget");
            auto w=weapon(*u,c);
            if(!w)return fail<PersonalPlan>("NoWeapon");
            if(recovering(*u,b.round))return fail<PersonalPlan>("WeaponRecovering");
            if(u->stamina<(s?s->stamina_cost:w->stamina_cost))return fail<PersonalPlan>("NotEnoughStamina");
        }
        else if(!p.target_id.empty())return fail<PersonalPlan>("UnexpectedTarget");
        if(s&&u->stamina<s->stamina_cost)return fail<PersonalPlan>("NotEnoughStamina");
        return {true,plan(p.destination,p.heading,p.action,p.target_id,p.skill_id),{}};
    }
    // 玩家公开预览复用同一伤害叶，既不预读AI意图也不推进随机。
    Outcome<PersonalAttackPreview> personal_preview_attack(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& p){
        auto valid=battle_error(b,c);
        if(!valid.ok)return {false,{},valid.error};
        auto u=b.units.find(actor);
        if(!u)return fail<PersonalAttackPreview>("UnknownCombatant");
        if(u->hp<=0)return fail<PersonalAttackPreview>("ActorDown");
        if(u->withdrawn_round)return fail<PersonalAttackPreview>("ActorWithdrawn");
        auto t=b.units.find(p.target_id);
        if(!t||t->hp<=0||t->withdrawn_round)return fail<PersonalAttackPreview>("InvalidTarget");
        if(t->side==u->side)return fail<PersonalAttackPreview>("FriendlyTarget");
        auto w=weapon(*u,c);
        if(!w)return fail<PersonalAttackPreview>("NoWeapon");
        if(recovering(*u,b.round))return fail<PersonalAttackPreview>("WeaponRecovering");
        const PersonalSkill* s=nullptr;
        if(!p.skill_id.empty()){
            auto r=skill(*u,c,p.skill_id);
            if(!r.ok)return {false,{},r.error};
            s=r.value;
            if(s->kind!="strike")return fail<PersonalAttackPreview>("NotAttackSkill");
        }
        if(u->stamina<(s?s->stamina_cost:w->stamina_cost))return fail<PersonalAttackPreview>("NotEnoughStamina");
        auto checked=personal_validate_plan(b,c,actor,plan(p.destination,p.heading,s?"skill":"attack",p.target_id,p.skill_id));
        if(!checked.ok)return {false,{},checked.error};
        auto projected=b;
        auto& a=*projected.units.find(actor);
        a.position=p.destination;
        a.heading=p.heading;
        auto effective=effective_weapon(*w,s);
        auto selected=select_targets(projected,c,actor,p.target_id,s,effective);
        if(!selected.ok)return {false,{},selected.error};
        auto& objective=*c.objectives.find(b.objective_id);
        PersonalAttackPreview result;
        std::vector<PersonalAttackTarget> targets;
        for(auto& id:selected.value){
            auto& target=*projected.units.find(id);
            PersonalAttackTarget row;
            row.target_id=id;
            row.damage=damage(a,target,effective,s,c,b.round,10000,objective);
            if(objective.hit_model=="skill_v1")row.hit_bp=hit_chance(a,target,effective,s);
            if(id==p.target_id){
                result.damage=row.damage;
                result.hit_bp=row.hit_bp;
            }
            targets.push_back(row);
        }
        if(s&&s->area)result.targets=targets;
        return {true,result,{}};
    }
    // UI移动圈仍服从倒地、离场、装填和技能代价，非法动作返回零。
    Integer personal_movement_distance(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const StableId& skill_id,const StableId& action){
        if(!battle_error(b,c).ok)return 0;
        auto u=b.units.find(actor);
        if(!u||action=="defend"||(!action.empty()&&action!="attack"&&action!="wait"&&action!="skill"&&action!="withdraw")||u->hp<=0||u->withdrawn_round||recovering(*u,b.round))return 0;
        const PersonalSkill* s=nullptr;
        if(!skill_id.empty()){
            auto r=skill(*u,c,skill_id);
            if(!r.ok||(r.value->kind!="guard"&&r.value->kind!="strike")||u->stamina<r.value->stamina_cost)return 0;
            s=r.value;
        }
        return move_distance(*u,c,b.round,s);
    }
    // 一轮先验证全部承诺，再同步移动、防御、出手、反击、扣血、撤离和恢复。
    Outcome<PersonalBattle> personal_resolve(const PersonalBattle& b,const PersonalCatalog& c,const PersonalPlan& player,CommonStore& common){
        auto valid=battle_error(b,c);
        if(!valid.ok)return {false,{},valid.error};
        auto& objective=*c.objectives.find(b.objective_id);
        if(objective.hit_model=="skill_v1"&&!common.state().rng.find("duel"))return fail<PersonalBattle>("InvalidDuelRandomState");
        auto next=b;
        next.last_events.clear();
        auto initial=personal_outcome(next,c);
        if(!initial.empty()){
            finish(next,c,initial);
            return {true,next,{}};
        }
        auto checked=personal_validate_plan(b,c,b.controller_id,player);
        if(!checked.ok)return {false,{},checked.error};
        OrderedTable<PersonalPlan> plans;
        plans.insert(b.controller_id,checked.value);
        for(auto& row:b.ai_plans.rows()){
            auto u=b.units.find(row.first);
            if(!u||row.first==b.controller_id||u->hp<=0||u->withdrawn_round)return fail<PersonalBattle>("UnexpectedAIPlan");
        }
        for(auto& id:ids(b.units)){
            auto& u=*b.units.find(id);
            if(id==b.controller_id||u.hp<=0||u.withdrawn_round)continue;
            auto p=b.ai_plans.find(id);
            if(!p)return fail<PersonalBattle>("MissingAIPlan");
            auto r=personal_validate_plan(b,c,id,*p);
            if(!r.ok){
                auto e=common_failure("InvalidAIPlan").error;
                e.actor_id=id;
                e.reason=r.error.original.code;
                return {false,{},e};
            }
            plans.insert(id,r.value);
        }
        auto moved=personal_resolve_moves(*c.arenas.find(b.battlefield_id),next.units,plans);
        if(!moved.ok)return {false,{},moved.error};
        OrderedTable<Integer> guards,counters,totals;
        std::set<StableId> directional,melee;
        std::vector<PersonalEvent> primary,statuses;
        for(auto& id:ids(plans)){
            auto& u=*next.units.find(id);
            auto& p=*plans.find(id);
            u.position=*moved.value.positions.find(id);
            u.heading=p.heading;
            auto names=u.statuses.order();
            for(auto& name:names)if(*u.statuses.find(name)<next.round)u.statuses.erase(name);
            PersonalEvent event;
            event.actor_id=id;
            event.action="move";
            event.position=u.position;
            event.code=std::find(moved.value.blocked.begin(),moved.value.blocked.end(),id)==moved.value.blocked.end()?"Moved":"MoveStopped";
            next.last_events.push_back(event);
            event={};
            event.actor_id=id;
            if(p.action=="defend"){
                guards.set(id,5000);
                directional.insert(id);
                if(objective.stamina_model!="round_v1")u.stamina=std::min<Integer>(100,u.stamina+18);
                event.action="defend";
                event.code="Guarded";
                next.last_events.push_back(event);
            }
            else if(p.action=="wait"){
                if(objective.stamina_model!="round_v1")u.stamina=std::min<Integer>(100,u.stamina+25);
                event.action="wait";
                event.code="Rested";
                next.last_events.push_back(event);
            }
            else if(p.action=="skill"&&c.skills.find(p.skill_id)->kind=="guard"){
                auto& s=*c.skills.find(p.skill_id);
                u.stamina-=s.stamina_cost;
                guards.set(id,mastered(s.defense_bp,value(u.learned,p.skill_id)));
                if(s.guard_facing=="front")directional.insert(id);
                if(s.counter_damage_bp)counters.set(id,*s.counter_damage_bp);
                event.action="skill";
                event.skill_id=p.skill_id;
                event.code="Guarded";
                next.last_events.push_back(event);
            }
        }
        for(auto& id:ids(plans)){
            auto& p=*plans.find(id);
            auto s=c.skills.find(p.skill_id);
            if(p.action!="attack"&&!(p.action=="skill"&&s&&s->kind=="strike"))continue;
            auto& u=*next.units.find(id);
            auto w=effective_weapon(*weapon(u,c),s);
            u.recovery_until=next.round+w.recovery_rounds+1;
            Outcome<std::vector<StableId>> selected;
            if(s&&s->requires_stationary.value_or(false)&&(p.destination!=b.units.find(id)->position||u.position!=b.units.find(id)->position))selected=fail<std::vector<StableId>>("StrikeRequiresStationary");
            else selected=select_targets(next,c,id,p.target_id,s,w);
            if(!selected.ok){
                PersonalEvent e;
                e.actor_id=id;
                e.target_id=p.target_id;
                e.action=p.action;
                e.skill_id=p.skill_id;
                e.damage=0;
                e.code=selected.error.original.code;
                next.last_events.push_back(e);
                continue;
            }
            u.stamina-=s?s->stamina_cost:w.stamina_cost;
            for(auto& target_id:selected.value){
                auto& t=*next.units.find(target_id);
                PersonalEvent e;
                e.actor_id=id;
                e.target_id=target_id;
                e.action=p.action;
                e.skill_id=p.skill_id;
                e.damage=0;
                if(objective.hit_model=="skill_v1"){
                    e.hit_bp=hit_chance(u,t,w,s);
                    auto roll=common.random_bounded("duel",0,9999);
                    if(!roll.ok)return {false,{},roll.error};
                    e.roll=roll.value;
                    if(roll.value>=*e.hit_bp){
                        e.code="Miss";
                        next.last_events.push_back(e);
                        continue;
                    }
                }
                Integer guard=value(guards,target_id,10000);
                if(directional.count(target_id))guard=guard_multiplier(u,t,guard);
                auto dealt=damage(u,t,w,s,c,next.round,guard,objective);
                totals.set(target_id,value(totals,target_id)+dealt);
                e.damage=dealt;
                e.code="Hit";
                next.last_events.push_back(e);
                primary.push_back(e);
                if(w.category=="sword"||w.category=="spear")melee.insert(target_id);
                if(s&&s->status!="none"&&s->status_rounds>0){
                    PersonalEvent status;
                    status.actor_id=id;
                    status.target_id=target_id;
                    status.action="status";
                    status.status=s->status;
                    status.expires_round=next.round+s->status_rounds;
                    statuses.push_back(status);
                }
            }
        }
        std::sort(primary.begin(),primary.end(),[](auto& a,auto& b){
            return a.actor_id==b.actor_id?a.target_id<b.target_id:a.actor_id<b.actor_id;
        });
        std::set<StableId> counter_used;
        for(auto& hit:primary){
            auto id=*hit.target_id,other=*hit.actor_id;
            if(!counters.find(id)||counter_used.count(id))continue;
            auto& counter=*next.units.find(id);
            auto& target=*next.units.find(other);
            auto w=weapon(counter,c),incoming=weapon(target,c);
            if(counter.side==target.side||!w||!incoming||(w->category!="sword"&&w->category!="spear")||(incoming->category!="sword"&&incoming->category!="spear")||!personal_can_attack(*c.arenas.find(b.battlefield_id),counter.position,target.position,counter.heading,*w).ok)continue;
            counter_used.insert(id);
            Integer guard=value(guards,other,10000);
            if(directional.count(other))guard=guard_multiplier(counter,target,guard);
            auto power=scale(attack_power(counter,target,*w,nullptr,c,objective),*counters.find(id));
            auto dealt=mitigate(power,target,c,next.round,guard);
            totals.set(other,value(totals,other)+dealt);
            melee.insert(other);
            PersonalEvent e;
            e.actor_id=id;
            e.target_id=other;
            e.action="counter";
            e.skill_id=plans.find(id)->skill_id;
            e.damage=dealt;
            e.code="Hit";
            next.last_events.push_back(e);
        }
        for(auto& row:totals.rows()){
            auto& u=*next.units.find(row.first);
            u.hp=std::max<Integer>(0,u.hp-row.second);
        }
        for(auto& e:statuses){
            auto& u=*next.units.find(*e.target_id);
            u.statuses.set(*e.status,std::max(value(u.statuses,*e.status,-1),*e.expires_round));
            next.last_events.push_back(e);
        }
        auto outcome=personal_outcome(next,c);
        auto withdraw_event=[&](const StableId& id,const StableId& code){
            PersonalEvent e;
            e.actor_id=id;
            e.action="withdraw";
            e.code=code;
            next.last_events.push_back(e);
        };
        auto& arena=*c.arenas.find(next.battlefield_id);
        if(outcome.empty())for(auto& id:ids(plans)){
            auto& u=*next.units.find(id);
            if(id==next.controller_id||u.protected_actor||u.hp<=0||u.withdrawn_round||plans.find(id)->action!="withdraw")continue;
            StableId code="Withdrawn";
            if(!personal_at_exit(arena,u.side,u.position))code="ExitNotReached";
            else if(melee.count(id))code="WithdrawalMeleeHit";
            else {
                u.withdrawn_round=next.round;
                if(!u.field_order.empty())u.field_order.push_back("withdrawn_round");
            }
            withdraw_event(id,code);
        }
        if(outcome.empty()&&plans.find(next.controller_id)->action=="withdraw"){
            auto& controller=*next.units.find(next.controller_id);
            if(!personal_at_exit(arena,controller.side,controller.position))withdraw_event(next.controller_id,"ExitNotReached");
            else if(melee.count(next.controller_id))withdraw_event(next.controller_id,"WithdrawalMeleeHit");
            else{
                bool ready=true;
                for(auto& id:ids(next.units)){
                    auto& u=*next.units.find(id);
                    if(u.side!=controller.side||!u.protected_actor)continue;
                    if(!personal_at_exit(arena,u.side,u.position)||melee.count(id)){
                        ready=false;
                        withdraw_event(id,melee.count(id)?"WithdrawalMeleeHit":"ProtectedExitNotReached");
                    }
                }
                if(ready){
                    for(auto& id:ids(next.units)){
                        auto& u=*next.units.find(id);
                        if(id==next.controller_id||(u.side==controller.side&&u.protected_actor)){
                            u.withdrawn_round=next.round;
                            if(!u.field_order.empty())u.field_order.push_back("withdrawn_round");
                            withdraw_event(id,"Withdrawn");
                        }
                    }
                    outcome="withdrawn";
                }
            }
        }
        if(outcome.empty())outcome=personal_outcome(next,c);
        if(outcome.empty()&&next.round>=objective.max_rounds)outcome=personal_limit_outcome(next,c);
        if(objective.stamina_model=="round_v1"){
            OrderedTable<Integer> pending;
            std::set<StableId> ordinary;
            for(auto& row:plans.rows()){
                auto& u=*next.units.find(row.first);
                if(u.hp<=0||u.withdrawn_round)continue;
                Integer amount=0;
                if(row.second.action=="defend"){
                    amount=10;
                    if(auto h=heart(u,c))amount+=h->defend_recovery_bonus.value_or(0);
                }
                else if(row.second.action=="wait"&&row.second.destination==b.units.find(row.first)->position&&u.position==b.units.find(row.first)->position)amount=15;
                pending.set(row.first,amount);
            }
            for(auto& e:next.last_events)if(e.code=="Hit"&&(e.action=="attack"||e.action=="skill"||e.action=="counter")&&e.damage&&*e.damage>0){
                if(e.target_id&&pending.find(*e.target_id))pending.set(*e.target_id,value(pending,*e.target_id)+3);
                if(e.action=="attack"&&e.actor_id&&pending.find(*e.actor_id)&&plans.find(*e.actor_id)->action=="attack")ordinary.insert(*e.actor_id);
            }
            for(auto& id:ids(pending)){
                auto& u=*next.units.find(id);
                Integer amount=std::min<Integer>({20,value(pending,id)+(ordinary.count(id)?5:0),100-u.stamina});
                if(amount>0){
                    u.stamina+=amount;
                    PersonalEvent e;
                    e.actor_id=id;
                    e.action="stamina";
                    e.amount=amount;
                    e.code="Recovered";
                    next.last_events.push_back(e);
                }
            }
        }
        if(!outcome.empty())finish(next,c,outcome);
        else{
            ++next.round;
            for(auto& row:next.units.rows())if(!row.second.withdrawn_round){
                auto names=row.second.statuses.order();
                for(auto& name:names)if(*row.second.statuses.find(name)<next.round)row.second.statuses.erase(name);
            }
            auto ai=personal_make_ai(next,c);
            if(!ai.ok)return {false,{},ai.error};
            next.ai_plans=std::move(ai.value);
        }
        return {true,std::move(next),{}};
    }
}
