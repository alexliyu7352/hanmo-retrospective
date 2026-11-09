// 原轮初可见AI候选与固定评分；禁止读取已存ai_plans、玩家草案或未来随机。
#include "personal_rules_internal.hpp"
#include <cmath>
#include <set>
namespace hanmo::resident {
    namespace personal_detail {
        // 规范动作键沿Godot排序JSON五字段，不把随机或字段插入序作为平局裁决。
        StableId action_key(const PersonalPlan& p,PersonalPoint origin){
            auto quote=[](const StableId& s){
                StableId r="\"";
                const char* hex="0123456789abcdef";
                for(unsigned char c:s){
                    if(c=='"'||c=='\\'){
                        r+='\\';
                        r+=char(c);
                    }
                    else if(c=='\b')r+="\\b";
                    else if(c=='\f')r+="\\f";
                    else if(c=='\n')r+="\\n";
                    else if(c=='\r')r+="\\r";
                    else if(c=='\t')r+="\\t";
                    else if(c<32){
                        r+="\\u00";
                        r+=hex[c>>4];
                        r+=hex[c&15];
                    }
                    else r+=char(c);
                }
                return r+'"';
            };
            Integer rank=p.action=="withdraw"?0:p.action=="attack"?2:p.action=="skill"?3:p.action=="defend"?4:p.action=="wait"?5:9;
            if(p.action=="wait"&&p.destination!=origin)rank=1;
            return std::to_string(rank)+":{\"action\":"+quote(p.action)+",\"destination\":["+std::to_string(p.destination.x)+","+std::to_string(p.destination.y)+"],\"heading\":"+std::to_string(p.heading)+",\"skill_id\":"+quote(p.skill_id)+",\"target_id\":"+quote(p.target_id)+"}";
        }
    }
    namespace {using namespace personal_detail;
    constexpr double pi=3.1415926535897932384626433832795;
    // 原启发移动枚举固定方向，以实际身体和视线检查全部候选。
    PersonalPoint ai_destination(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const StableId& target,const PersonalSkill* skill=nullptr){
        auto& u=*b.units.find(actor);
        auto& t=*b.units.find(target);
        auto base=weapon(u,c);
        PersonalWeapon w=base?effective_weapon(*base,skill):PersonalWeapon{};
        auto& arena=*c.arenas.find(b.battlefield_id);
        auto limit=move_distance(u,c,b.round,skill);
        std::vector<PersonalBody> bodies;
        for(auto& row:b.units.rows())if(row.first!=actor&&!row.second.withdrawn_round)bodies.push_back({row.second.position,row.second.radius});
        double preferred=std::max<double>(u.radius+t.radius+4,w.minimum_range+(w.maximum_range-w.minimum_range)*0.55);
        std::vector<PersonalPoint> candidates{u.position};
        // 原Vector2计算按binary32逐步舍入；不能提升成double后改变整数候选落点。
        // 核对固定Godot ed1daf0bf core/math/vector2.cpp的normalized/limit_length/from_angle。
        float dx=static_cast<float>(u.position.x-t.position.x),dy=static_cast<float>(u.position.y-t.position.y);
        float length=std::sqrt(dx*dx+dy*dy);
        if(length>0){dx/=length;dy/=length;}
        float desired_x=static_cast<float>(t.position.x)+dx*static_cast<float>(preferred);
        float desired_y=static_cast<float>(t.position.y)+dy*static_cast<float>(preferred);
        float tx=desired_x-static_cast<float>(u.position.x),ty=desired_y-static_cast<float>(u.position.y);
        length=std::sqrt(tx*tx+ty*ty);
        if(length>0&&static_cast<float>(limit)<length){tx/=length;ty/=length;tx*=static_cast<float>(limit);ty*=static_cast<float>(limit);}
        candidates.push_back({Integer(std::round(double(u.position.x)+tx)),Integer(std::round(double(u.position.y)+ty))});
        for(int step=0;step<16;++step)for(double fraction:{0.5,1.0}){
            float angle=static_cast<float>(2*pi*step/16);
            float x=std::cos(angle)*static_cast<float>(limit),y=std::sin(angle)*static_cast<float>(limit);
            x*=static_cast<float>(fraction);y*=static_cast<float>(fraction);
            candidates.push_back({Integer(std::round(double(u.position.x)+x)),Integer(std::round(double(u.position.y)+y))});
        }
        auto best=u.position;
        double score=INFINITY;
        for(auto point:candidates){
            if(!personal_can_move(arena,u.position,point,limit,u.radius,bodies).ok)continue;
            double distance=personal_distance(point,t.position),value=std::abs(distance-preferred)+personal_distance(point,u.position)*0.02;
            if(distance<w.minimum_range)value+=500;
            if(!personal_line_of_sight(arena,point,t.position))value+=200;
            if(value<score){
                best=point;
                score=value;
            }
        }
        return best;
    }
    // 普通攻击和显式扩展技能分别生成原合法候选，不能先用普通射程截断技能。
    std::vector<PersonalPlan> attack_plans(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const StableId& target){
        auto& u=*b.units.find(actor);
        auto& t=*b.units.find(target);
        auto ordinary=ai_destination(b,c,actor,target);
        std::vector<PersonalPlan> result{plan(ordinary,personal_heading(ordinary,t.position,u.heading),"attack",target)};
        for(auto& id:ids(u.learned)){
            auto s=c.skills.find(id);
            if(!s||s->kind!="strike")continue;
            std::vector<PersonalPoint> points{ordinary};
            if(s->requires_stationary.value_or(false))points={u.position};
            else if(s->maximum_range_bonus)for(auto point:{ai_destination(b,c,actor,target,s),u.position})if(std::find(points.begin(),points.end(),point)==points.end())points.push_back(point);
            for(auto point:points)result.push_back(plan(point,personal_heading(point,t.position,u.heading),"skill",target,id));
        }
        return result;
    }
    struct BestAttack {
        PersonalPlan plan;
        PersonalAttackPreview preview;
    };
    // 严格大于才替换，沿原攻击候选顺序解决同伤害平局。
    std::optional<BestAttack> best_attack(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const StableId& target,bool expected,bool aggregate=true){
        std::optional<BestAttack> best;
        Integer highest=-1;
        for(auto& p:attack_plans(b,c,actor,target)){
            auto preview=personal_preview_attack(b,c,actor,p);
            if(!preview.ok)continue;
            Integer value=preview.value.damage;
            if(expected){
                value=0;
                if(preview.value.targets)for(auto& t:*preview.value.targets)value+=t.damage*t.hit_bp.value_or(10000)/10000;
                else value=preview.value.damage*preview.value.hit_bp.value_or(10000)/10000;
            }
            if(expected&&!aggregate)value=std::min(preview.value.damage,b.units.find(target)->hp)*preview.value.hit_bp.value_or(10000)*4/10000;
            if(value>highest){
                highest=value;
                best=BestAttack{p,preview.value};
            }
        }
        return best;
    }
    struct Incoming {
        PersonalUnit unit;
        PersonalWeapon weapon;
        const PersonalSkill* skill=nullptr;
    };
    // 仅投影当前公开能力的最强合法攻击；同步被击倒不会取消这个威胁。
    std::vector<Incoming> incoming_sources(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,PersonalPoint destination,bool expected=false){
        auto projected=b;
        projected.units.find(actor)->position=destination;
        auto& target=*projected.units.find(actor);
        std::vector<Incoming> result;
        for(auto& id:ids(projected.units)){
            auto& u=*projected.units.find(id);
            if(!enemy(target,u)||recovering(u,b.round)||!weapon(u,c))continue;
            auto attack=best_attack(projected,c,id,actor,expected,false);
            if(!attack)continue;
            auto s=c.skills.find(attack->plan.skill_id);
            auto a=u;
            a.position=attack->plan.destination;
            a.heading=attack->plan.heading;
            result.push_back({a,effective_weapon(*weapon(u,c),s),s});
        }
        return result;
    }
    // 每个入射方向分别应用守势，保留原伤害叶的截断顺序。
    Integer incoming_damage(const PersonalUnit& original,const PersonalPlan& p,const std::vector<Incoming>& sources,const PersonalCatalog& c,Integer round,const PersonalObjective& objective){
        auto target=original;
        target.position=p.destination;
        target.heading=p.heading;
        Integer total=0;
        for(auto& source:sources){
            Integer guard=10000;
            if(p.action=="defend")guard=guard_multiplier(source.unit,target,5000);
            else if(p.action=="skill"){
                auto s=c.skills.find(p.skill_id);
                if(s&&s->kind=="guard"){
                    guard=mastered(s->defense_bp,value(target.learned,s->id));
                    if(s->guard_facing=="front")guard=guard_multiplier(source.unit,target,guard);
                }
            }
            total+=damage(source.unit,target,source.weapon,source.skill,c,round,guard,objective);
        }
        return total;
    }
    // 评分威胁采用四分点期望，上限为目标当前HP；不抽样。
    Integer incoming_q(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& p){
        auto target=*b.units.find(actor);
        target.heading=p.heading;
        auto& o=*c.objectives.find(b.objective_id);
        Integer total=0;
        for(auto& source:incoming_sources(b,c,actor,target.position,true)){
            Integer guard=10000;
            if(p.action=="defend")guard=guard_multiplier(source.unit,target,5000);
            else if(p.action=="skill"){
                auto s=c.skills.find(p.skill_id);
                if(s&&s->kind=="guard"){
                    guard=mastered(s->defense_bp,value(target.learned,s->id));
                    if(s->guard_facing=="front")guard=guard_multiplier(source.unit,target,guard);
                }
            }
            auto dealt=damage(source.unit,target,source.weapon,source.skill,c,b.round,guard,o);
            Integer chance=o.hit_model=="skill_v1"?hit_chance(source.unit,target,source.weapon,source.skill):10000;
            total+=std::min(dealt,target.hp)*chance*4/10000;
        }
        return std::min(target.hp*4,total);
    }
    // 保护风险只看同侧真实保护者，不发明世界层战略或未来回合收益。
    Integer protected_risk(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan* acting=nullptr){
        Integer result=0;
        auto side=b.units.find(actor)->side;
        for(auto& id:ids(b.units)){
            auto& u=*b.units.find(id);
            if(u.side!=side||!u.protected_actor||u.hp<=0||u.withdrawn_round)continue;
            result+=incoming_q(b,c,id,id==actor&&acting?*acting:plan(u.position,u.heading,"wait"));
        }
        return result;
    }
    // 旧AI的致命风险守势覆盖只在确实能存活时采用，同损伤按成本与原规范键排序。
    PersonalPlan safer_guard(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& original){
        auto& u=*b.units.find(actor);
        auto& o=*c.objectives.find(b.objective_id);
        auto incoming=incoming_sources(b,c,actor,original.destination);
        auto danger=incoming_damage(u,original,incoming,c,b.round,o);
        if(danger<u.hp)return original;
        auto stationary=original.destination==u.position?incoming:incoming_sources(b,c,actor,u.position);
        std::vector<Integer> headings{original.heading};
        for(auto& source:stationary){
            auto h=personal_heading(u.position,source.unit.position,original.heading);
            if(std::find(headings.begin(),headings.end(),h)==headings.end())headings.push_back(h);
        }
        std::sort(headings.begin(),headings.end());
        std::vector<PersonalPlan> candidates;
        for(auto heading:headings)candidates.push_back(plan(u.position,heading,"defend"));
        for(auto& id:ids(u.learned)){
            auto s=c.skills.find(id);
            if(!s||s->kind!="guard")continue;
            headings={original.heading};
            if(s->guard_facing=="front"){
                for(auto& source:incoming){
                    auto h=personal_heading(original.destination,source.unit.position,original.heading);
                    if(std::find(headings.begin(),headings.end(),h)==headings.end())headings.push_back(h);
                }
                std::sort(headings.begin(),headings.end());
            }
            for(auto h:headings)candidates.push_back(plan(original.destination,h,"skill",{},id));
        }
        auto best=original;
        Integer best_damage=danger,best_cost=2147483647;
        StableId best_key;
        for(auto& p:candidates){
            if(!personal_validate_plan(b,c,actor,p).ok)continue;
            auto dealt=incoming_damage(u,p,p.destination==original.destination?incoming:stationary,c,b.round,o);
            if(dealt>=u.hp)continue;
            Integer cost=p.action=="skill"?c.skills.find(p.skill_id)->stamina_cost:0;
            auto key=action_key(p,u.position).substr(2);
            if(dealt<best_damage||(dealt==best_damage&&(cost<best_cost||(cost==best_cost&&key<best_key)))){
                best=p;
                best_damage=dealt;
                best_cost=cost;
                best_key=key;
            }
        }
        return best;
    }
    // 低HP候选只有实际更安全或到达无近战威胁出口才代替原计划。
    PersonalPlan safer_withdrawal(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& original){
        auto& u=*b.units.find(actor);
        auto& o=*c.objectives.find(b.objective_id);
        if(!low_hp(u)||!o.allow_withdrawal)return original;
        auto destination=personal_withdraw_destination(b,c,actor,recovering(u,b.round)?0:move_distance(u,c,b.round));
        bool exit=personal_at_exit(*c.arenas.find(b.battlefield_id),u.side,destination);
        if(destination==u.position&&!exit)return original;
        auto candidate=plan(destination,original.heading,"withdraw");
        if(!personal_validate_plan(b,c,actor,candidate).ok)return original;
        auto incoming=incoming_sources(b,c,actor,destination);
        auto risk=incoming_damage(u,candidate,incoming,c,b.round,o);
        if(risk>=u.hp)return original;
        bool melee=false;
        for(auto& source:incoming)if(source.weapon.category=="sword"||source.weapon.category=="spear")melee=true;
        if(exit&&!melee)return candidate;
        auto prior=incoming_damage(u,original,incoming_sources(b,c,actor,original.destination),c,b.round,o);
        return destination!=u.position&&risk<=prior?candidate:original;
    }
    // 固定预算先列安全候选，后列进攻卡；去重不依赖哈希迭代顺序。
    std::vector<PersonalPlan> scored_candidates(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor){
        auto& u=*b.units.find(actor);
        std::vector<PersonalPoint> points{u.position};
        std::vector<Integer> headings{u.heading};
        std::vector<PersonalPlan> attacks;
        for(auto& row:b.units.rows())if(enemy(u,row.second)){
            auto h=personal_heading(u.position,row.second.position,u.heading);
            if(std::find(headings.begin(),headings.end(),h)==headings.end())headings.push_back(h);
        }
        if(u.protected_actor&&u.side==b.units.find(b.controller_id)->side){
            auto p=personal_escort_destination(b,c,actor,recovering(u,b.round)?0:move_distance(u,c,b.round));
            if(std::find(points.begin(),points.end(),p)==points.end())points.push_back(p);
        }
        else for(auto& id:ids(b.units)){
            auto& t=*b.units.find(id);
            if(!enemy(u,t))continue;
            auto h=personal_heading(u.position,t.position,u.heading);
            if(std::find(headings.begin(),headings.end(),h)==headings.end())headings.push_back(h);
            if(weapon(u,c)&&!recovering(u,b.round)){
                auto point=ai_destination(b,c,actor,id);
                if(std::find(points.begin(),points.end(),point)==points.end())points.push_back(point);
                auto extra=attack_plans(b,c,actor,id);
                attacks.insert(attacks.end(),extra.begin(),extra.end());
            }
        }
        std::sort(headings.begin(),headings.end());
        std::vector<PersonalPlan> raw;
        for(auto h:headings)raw.push_back(plan(u.position,h,"defend"));
        for(auto p:points)raw.push_back(plan(p,u.heading,"wait"));
        if(low_hp(u)&&c.objectives.find(b.objective_id)->allow_withdrawal){
            auto point=personal_withdraw_destination(b,c,actor,recovering(u,b.round)?0:move_distance(u,c,b.round));
            if(point!=u.position||personal_at_exit(*c.arenas.find(b.battlefield_id),u.side,u.position))raw.push_back(plan(point,u.heading,"withdraw"));
        }
        for(auto& id:ids(u.learned)){
            auto s=c.skills.find(id);
            if(s&&s->kind=="guard")for(auto point:points)for(auto h:headings)raw.push_back(plan(point,h,"skill",{},id));
        }
        raw.insert(raw.end(),attacks.begin(),attacks.end());
        std::vector<PersonalPlan> result;
        std::set<StableId> seen;
        for(auto& p:raw){
            auto key=action_key(p,u.position);
            if(seen.count(key)||!personal_validate_plan(b,c,actor,p).ok)continue;
            seen.insert(key);
            result.push_back(p);
        }
        return result;
    }
    struct Evaluation {
        PersonalPlan plan;
        Integer score=0,maximum_incoming=0;
        bool survives=false;
        StableId key;
    };
    // 每个评分只移动行动者，其他人物仍处轮初位置；先算威胁再投影期望伤害。
    std::optional<Evaluation> assess(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor,const PersonalPlan& p,Integer baseline){
        auto projected=b;
        OrderedTable<PersonalPlan> plans;
        plans.insert(actor,p);
        auto moved=personal_resolve_moves(*c.arenas.find(b.battlefield_id),b.units,plans);
        if(!moved.ok)return {};
        auto& u=*projected.units.find(actor);
        u.position=*moved.value.positions.find(actor);
        u.heading=p.heading;
        auto actual=p;
        actual.destination=u.position;
        auto iq=incoming_q(projected,c,actor,actual),maximum=incoming_damage(u,actual,incoming_sources(projected,c,actor,u.position),c,b.round,*c.objectives.find(b.objective_id)),protection=protected_risk(projected,c,actor,&actual);
        Integer dq=0,cost=0;
        OrderedTable<Integer> outgoing;
        auto s=c.skills.find(p.skill_id);
        if(p.action=="skill"&&s&&s->kind=="guard")cost=s->stamina_cost;
        else if(p.action=="attack"||(s&&s->kind=="strike")){
            bool condition=!s||!s->requires_stationary.value_or(false)||(p.destination==b.units.find(actor)->position&&u.position==b.units.find(actor)->position);
            if(condition){
                auto preview=personal_preview_attack(projected,c,actor,actual);
                if(preview.ok){
                    cost=s?s->stamina_cost:weapon(u,c)->stamina_cost;
                    auto targets=preview.value.targets.value_or(std::vector<PersonalAttackTarget>{{p.target_id,preview.value.damage,preview.value.hit_bp}});
                    for(auto& target:targets){
                        auto expected=std::min(target.damage,projected.units.find(target.target_id)->hp)*target.hit_bp.value_or(10000)*4/10000;
                        dq+=expected;
                        outgoing.set(target.target_id,expected);
                    }
                }
            }
        }
        auto result=projected;
        result.units.find(actor)->hp=std::max<Integer>(0,u.hp+(0-(iq+3)/4));
        for(auto& row:outgoing.rows()){
            auto& t=*result.units.find(row.first);
            t.hp=std::max<Integer>(0,t.hp-row.second/4);
        }
        auto outcome=personal_outcome(result,c);
        bool goal=u.side==b.units.find(b.controller_id)->side?outcome=="victory":outcome=="defeat"||outcome=="protected_lost";
        if(p.action=="withdraw"&&personal_at_exit(*c.arenas.find(b.battlefield_id),u.side,u.position)&&result.units.find(actor)->hp>0){
            bool safe=true;
            for(auto& source:incoming_sources(projected,c,actor,u.position))if(source.weapon.category=="sword"||source.weapon.category=="spear")safe=false;
            if(safe)goal=true;
        }
        return Evaluation{p,(goal?400:0)+(protection<baseline?160:0)+dq-iq-cost,maximum,maximum<b.units.find(actor)->hp,action_key(p,b.units.find(actor)->position)};
    }
    // 同分唯一规范键，低HP致命候选可被真正存活的防守候选覆盖。
    Outcome<PersonalPlan> scored_plan(const PersonalBattle& b,const PersonalCatalog& c,const StableId& actor){
        auto candidates=scored_candidates(b,c,actor);
        auto baseline=protected_risk(b,c,actor);
        std::vector<Evaluation> rows;
        for(std::size_t i=0;i<std::min<std::size_t>(512,candidates.size());++i){
            auto e=assess(b,c,actor,candidates[i],baseline);
            if(e)rows.push_back(*e);
        }
        if(rows.empty())return fail<PersonalPlan>("NoLegalAICandidate");
        auto better=[](const Evaluation& a,const Evaluation& b){
            return a.score>b.score||(a.score==b.score&&a.key<b.key);
        };
        auto best=rows.front();
        for(auto& row:rows)if(better(row,best))best=row;
        auto& u=*b.units.find(actor);
        if(u.hp*100<=u.initial_hp*25&&!best.survives){
            std::optional<Evaluation> survivor;
            for(auto& row:rows){
                auto s=c.skills.find(row.plan.skill_id);
                bool defensive=row.plan.action=="defend"||row.plan.action=="withdraw"||(row.plan.action=="skill"&&s&&s->kind=="guard");
                if(defensive&&row.survives&&(!survivor||better(row,*survivor)))survivor=row;
            }
            if(survivor)best=*survivor;
        }
        return {true,best.plan,{}};
    }
    }
    // 按稳定人物顺序预先保存非玩家计划，任何查询都不读取未来承诺。
    Outcome<OrderedTable<PersonalPlan>> personal_make_ai(const PersonalBattle& b,const PersonalCatalog& c){
        auto valid=personal_detail::battle_error(b,c);
        if(!valid.ok)return {false,{},valid.error};
        OrderedTable<PersonalPlan> plans;
        for(auto& id:personal_detail::ids(b.units)){
            auto& u=*b.units.find(id);
            if(id==b.controller_id||u.hp<=0||u.withdrawn_round)continue;
            if(c.objectives.find(b.objective_id)->ai_model=="scored_v1"){
                auto scored=scored_plan(b,c,id);
                if(scored.ok)plans.insert(id,scored.value);
                continue;
            }
            auto idle=personal_detail::plan(u.position,u.heading,u.stamina<35?"wait":"defend");
            StableId target;
            double nearest=INFINITY;
            for(auto& other:personal_detail::ids(b.units)){
                auto& t=*b.units.find(other);
                if(!personal_detail::enemy(u,t))continue;
                auto distance=personal_distance(u.position,t.position);
                if(distance<nearest){
                    nearest=distance;
                    target=other;
                }
            }
            if(u.protected_actor&&u.side==b.units.find(b.controller_id)->side&&c.objectives.find(b.objective_id)->kind=="escort"){
                if(!personal_detail::recovering(u,b.round))idle.destination=personal_escort_destination(b,c,id,personal_detail::move_distance(u,c,b.round));
                if(!target.empty())idle.heading=personal_heading(idle.destination,b.units.find(target)->position,u.heading);
                if(idle.destination!=u.position)idle.action="wait";
                plans.insert(id,idle);
                continue;
            }
            if(!personal_detail::weapon(u,c)||target.empty()||personal_detail::recovering(u,b.round)){
                plans.insert(id,safer_withdrawal(b,c,id,safer_guard(b,c,id,idle)));
                continue;
            }
            auto best=best_attack(b,c,id,target,true);
            if(best)idle=best->plan;
            else{
                idle.destination=ai_destination(b,c,id,target);
                idle.heading=personal_heading(idle.destination,b.units.find(target)->position,u.heading);
                if(idle.destination!=u.position)idle.action="wait";
            }
            plans.insert(id,safer_withdrawal(b,c,id,safer_guard(b,c,id,idle)));
        }
        return {true,std::move(plans),{}};
    }
}
