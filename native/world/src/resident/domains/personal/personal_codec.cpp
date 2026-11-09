// 个人战冷边界逐字段codec；类型化记录保原字段顺序，不保任意extras或第二世界字典。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "personal_rules_internal.hpp"
#include "personal_effect_projection.hpp"
#include "personal_projection.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include <array>
#include "resident/domains/conditions/domain_codec_util.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <set>
namespace hanmo::resident {
    // The original JSON fields drive both the real encoder and the borrowed shape sink.
    namespace personal_projection {
        Status fields(ProjectionWriter&,const PersonalObstacle&);
        Status fields(ProjectionWriter&,const PersonalSpawn&);
        Status fields(ProjectionWriter&,const PersonalExit&);
        Status fields(ProjectionWriter&,const PersonalArena&);
        Status fields(ProjectionWriter&,const PersonalWeapon&);
        Status fields(ProjectionWriter&,const PersonalArea&);
        Status fields(ProjectionWriter&,const PersonalSkillRequirement&);
        Status fields(ProjectionWriter&,const PersonalFacing&);
        Status fields(ProjectionWriter&,const PersonalObjective&);
        Status fields(ProjectionWriter&,const PersonalPlan&);
        Status fields(ProjectionWriter&,const PersonalHitLevels&);
        Status fields(ProjectionWriter&,const PersonalUnit&);
        Status fields(ProjectionWriter&,const PersonalEvent&);
        Status fields(ProjectionWriter&,const PersonalBattle&);
        Status fields(ProjectionWriter&,const PersonalFact&);
        Status fields(ProjectionWriter&,const PersonalSource&);
        Status fields(ProjectionWriter&,const PersonalTerms&);
        Status fields(ProjectionWriter&,const PersonalSkill&);
        inline Status fields(ProjectionWriter& out,const InteractionContext& x){return emit_interaction_context_fields(out,x);}
        template<class T>Status emit(ProjectionWriter&,std::string_view,const T&);
        template<class T,class A>Status emit(ProjectionWriter&,std::string_view,const std::vector<T,A>&);
        template<class T>Status emit(ProjectionWriter&,std::string_view,const OrderedTable<T>&);
        inline Status emit(ProjectionWriter& out,std::string_view name,const StableId& x){return out.field(name,x);}
        inline Status emit(ProjectionWriter& out,std::string_view name,Integer x){return out.field(name,x);}
        inline Status emit(ProjectionWriter& out,std::string_view name,bool x){return out.field(name,x);}
        inline Status emit(ProjectionWriter& out,std::string_view name,const PersonalPoint& x){const std::array<Integer,2> point{x.x,x.y};return out.sequence(name,point,[](auto& w,Integer n){return w.field("",n);});}
        template<class T>Status emit(ProjectionWriter& out,std::string_view name,const T& x){return out.object(name,[&](auto& w){return fields(w,x);});}
        template<class T,class A>Status emit(ProjectionWriter& out,std::string_view name,const std::vector<T,A>& rows){return out.sequence(name,rows,[](auto& w,const auto& row){return emit(w,"",row);});}
        template<class T>Status emit(ProjectionWriter& out,std::string_view name,const OrderedTable<T>& rows){return out.object(name,[&](auto& w){for(const auto& row:rows.rows()){auto status=emit(w,row.first,row.second);if(!status.ok)return status;}return w.status();});}
        template<class T>BoundaryValue encode(const T& x){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=emit(writer,"",x);if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
        Status fields(ProjectionWriter& out,const PersonalObstacle& x){
            emit(out,"x",x.x);
            emit(out,"y",x.y);
            emit(out,"width",x.width);
            emit(out,"height",x.height);
            emit(out,"blocks_movement",x.blocks_movement);
            emit(out,"blocks_sight",x.blocks_sight);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalSpawn& x){
            emit(out,"side",x.side);
            emit(out,"position",x.position);
            emit(out,"heading",x.heading);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalExit& x){
            emit(out,"side",x.side);
            emit(out,"position",x.position);
            emit(out,"radius",x.radius);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalArena& x){
            emit(out,"id",x.id);
            emit(out,"name_key",x.name_key);
            emit(out,"width",x.width);
            emit(out,"height",x.height);
            emit(out,"visual_tag",x.visual_tag);
            emit(out,"obstacles",x.obstacles);
            emit(out,"spawns",x.spawns);
            emit(out,"exits",x.exits);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalWeapon& x){
            emit(out,"category",x.category);
            emit(out,"minimum_range",x.minimum_range);
            emit(out,"maximum_range",x.maximum_range);
            emit(out,"arc_degrees",x.arc_degrees);
            emit(out,"damage",x.damage);
            emit(out,"stamina_cost",x.stamina_cost);
            emit(out,"recovery_rounds",x.recovery_rounds);
            emit(out,"move_distance",x.move_distance);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalArea& x){
            emit(out,"shape",x.shape);
            if(x.half_width)emit(out,"half_width",*x.half_width);
            if(x.arc_degrees)emit(out,"arc_degrees",*x.arc_degrees);
            emit(out,"max_targets",x.max_targets);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalSkillRequirement& x){
            emit(out,"skill_id",x.skill_id);
            emit(out,"level",x.level);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalFacing& x){
            emit(out,"side_bp",x.side_bp);
            emit(out,"back_bp",x.back_bp);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalObjective& x){
            emit(out,"id",x.id);
            emit(out,"name_key",x.name_key);
            emit(out,"kind",x.kind);
            emit(out,"max_rounds",x.max_rounds);
            emit(out,"injury_on_defeat",x.injury_on_defeat);
            emit(out,"allow_withdrawal",x.allow_withdrawal);
            if(x.ai_model)emit(out,"ai_model",*x.ai_model);
            if(x.escort_model)emit(out,"escort_model",*x.escort_model);
            if(x.stamina_model)emit(out,"stamina_model",*x.stamina_model);
            if(x.hit_model)emit(out,"hit_model",*x.hit_model);
            if(x.initial_stamina)emit(out,"initial_stamina",*x.initial_stamina);
            if(x.facing_damage)emit(out,"facing_damage",*x.facing_damage);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalPlan& x){
            emit(out,"destination",x.destination);
            emit(out,"heading",x.heading);
            emit(out,"action",x.action);
            emit(out,"target_id",x.target_id);
            emit(out,"skill_id",x.skill_id);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalHitLevels& x){
            emit(out,"martial",x.martial);
            emit(out,"archery",x.archery);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalUnit& x){
            emit(out,"actor_id",x.actor_id);
            emit(out,"side",x.side);
            emit(out,"position",x.position);
            emit(out,"heading",x.heading);
            emit(out,"radius",x.radius);
            emit(out,"hp",x.hp);
            emit(out,"initial_hp",x.initial_hp);
            emit(out,"stamina",x.stamina);
            emit(out,"weapon_id",x.weapon_id);
            emit(out,"martial",x.martial);
            emit(out,"learned",x.learned);
            emit(out,"heart_id",x.heart_id);
            emit(out,"statuses",x.statuses);
            emit(out,"recovery_until",x.recovery_until);
            emit(out,"protected",x.protected_actor);
            if(x.skill_levels)emit(out,"skill_levels",*x.skill_levels);
            if(x.armor_id)emit(out,"armor_id",*x.armor_id);
            if(x.armor_defense)emit(out,"armor_defense",*x.armor_defense);
            if(x.withdrawn_round)emit(out,"withdrawn_round",*x.withdrawn_round);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalEvent& x){
        if(x.field_order.empty()){
            if(x.action=="move"){
            if(x.actor_id)emit(out,"actor_id",*x.actor_id);
            if(x.action)emit(out,"action",*x.action);
            if(x.position)emit(out,"position",*x.position);
            if(x.code)emit(out,"code",*x.code);
                return out.status();
            }
            if(x.action=="stamina"){
            if(x.actor_id)emit(out,"actor_id",*x.actor_id);
            if(x.action)emit(out,"action",*x.action);
            if(x.amount)emit(out,"amount",*x.amount);
            if(x.code)emit(out,"code",*x.code);
                return out.status();
            }
            if(x.hit_bp||x.roll){
            if(x.actor_id)emit(out,"actor_id",*x.actor_id);
            if(x.target_id)emit(out,"target_id",*x.target_id);
            if(x.action)emit(out,"action",*x.action);
            if(x.skill_id)emit(out,"skill_id",*x.skill_id);
            if(x.damage)emit(out,"damage",*x.damage);
            if(x.hit_bp)emit(out,"hit_bp",*x.hit_bp);
            if(x.roll)emit(out,"roll",*x.roll);
            if(x.code)emit(out,"code",*x.code);
                return out.status();
            }
        }
            if(x.actor_id)emit(out,"actor_id",*x.actor_id);
            if(x.target_id)emit(out,"target_id",*x.target_id);
            if(x.action)emit(out,"action",*x.action);
            if(x.skill_id)emit(out,"skill_id",*x.skill_id);
            if(x.damage)emit(out,"damage",*x.damage);
            if(x.code)emit(out,"code",*x.code);
            if(x.status)emit(out,"status",*x.status);
            if(x.expires_round)emit(out,"expires_round",*x.expires_round);
            if(x.amount)emit(out,"amount",*x.amount);
            if(x.hit_bp)emit(out,"hit_bp",*x.hit_bp);
            if(x.roll)emit(out,"roll",*x.roll);
            if(x.position)emit(out,"position",*x.position);
        return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalBattle& x){
            emit(out,"id",x.id);
            out.text("kind","personal");
            emit(out,"controller_id",x.controller_id);
            emit(out,"battlefield_id",x.battlefield_id);
            emit(out,"objective_id",x.objective_id);
            emit(out,"source",x.source);
            emit(out,"created_tick",x.created_tick);
            emit(out,"completed_tick",x.completed_tick);
            emit(out,"round",x.round);
            emit(out,"phase",x.phase);
            emit(out,"units",x.units);
            emit(out,"ai_plans",x.ai_plans);
            emit(out,"outcome",x.outcome);
            emit(out,"last_events",x.last_events);
            emit(out,"result_id",x.result_id);
            emit(out,"applied",x.applied);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalFact& x){
            out.text("kind","personal_battle");
            emit(out,"battle_id",x.battle_id);
            emit(out,"result_id",x.result_id);
            emit(out,"tick",x.tick);
            emit(out,"outcome",x.outcome);
            emit(out,"participants",x.participants);
            emit(out,"defeated",x.defeated);
            emit(out,"source",x.source);
            if(x.withdrawn)emit(out,"withdrawn",*x.withdrawn);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalSource& x){
            out.field("kind",x.kind);
            if(x.kind=="role"&&x.context)emit(out,"context",*x.context);
            else if(x.kind=="encounter")out.field("encounter_id",x.encounter_id);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalTerms& x){
            out.field("battlefield_id",x.battlefield_id);out.field("objective_id",x.objective_id);
            if(domain_codec::had(x.field_order,"fee"))out.field("fee",x.fee);
            if(domain_codec::had(x.field_order,"wallet_id"))out.field("wallet_id",x.wallet_id);
            return out.order_fields(x.field_order);
        }
        Status fields(ProjectionWriter& out,const PersonalSkill& x){
            emit(out,"id",x.id);
            emit(out,"name_key",x.name_key);
            emit(out,"description_key",x.description_key);
            emit(out,"kind",x.kind);
            emit(out,"weapons",x.weapons);
            emit(out,"stamina_cost",x.stamina_cost);
            emit(out,"power_bp",x.power_bp);
            emit(out,"defense_bp",x.defense_bp);
            emit(out,"move_bonus",x.move_bonus);
            emit(out,"status",x.status);
            emit(out,"status_rounds",x.status_rounds);
            emit(out,"practice_ticks",x.practice_ticks);
            if(x.required_skill_level)emit(out,"required_skill_level",*x.required_skill_level);
            if(x.requires_stationary)emit(out,"requires_stationary",*x.requires_stationary);
            if(x.hit_bonus_bp)emit(out,"hit_bonus_bp",*x.hit_bonus_bp);
            if(x.defend_recovery_bonus)emit(out,"defend_recovery_bonus",*x.defend_recovery_bonus);
            if(x.guard_facing)emit(out,"guard_facing",*x.guard_facing);
            if(x.area)emit(out,"area",*x.area);
            if(x.maximum_range_bonus)out.object("range_modifier",[&](auto& w){w.field("maximum_bonus",*x.maximum_range_bonus);return w.order_fields(x.range_order);});
            if(x.counter_damage_bp)out.object("counter",[&](auto& w){w.field("damage_bp",*x.counter_damage_bp);return w.order_fields(x.counter_order);});
            return out.order_fields(x.field_order);
        }
    }
    namespace personal_codec {
        using V=BoundaryValue;
        using D=V::Dict;
        using A=V::Array;
        V enc(const PersonalObstacle&);
        void dec(const V&,PersonalObstacle&,const StableId&);
        Status visit(CodecVisitor&,PersonalObstacle&);
        V enc(const PersonalSpawn&);
        void dec(const V&,PersonalSpawn&,const StableId&);
        Status visit(CodecVisitor&,PersonalSpawn&);
        V enc(const PersonalExit&);
        void dec(const V&,PersonalExit&,const StableId&);
        Status visit(CodecVisitor&,PersonalExit&);
        V enc(const PersonalArena&);
        void dec(const V&,PersonalArena&,const StableId&);
        Status visit(CodecVisitor&,PersonalArena&);
        V enc(const PersonalWeapon&);
        void dec(const V&,PersonalWeapon&,const StableId&);
        Status visit(CodecVisitor&,PersonalWeapon&);
        V enc(const PersonalArea&);
        void dec(const V&,PersonalArea&,const StableId&);
        Status visit(CodecVisitor&,PersonalArea&);
        V enc(const PersonalSkillRequirement&);
        void dec(const V&,PersonalSkillRequirement&,const StableId&);
        Status visit(CodecVisitor&,PersonalSkillRequirement&);
        V enc(const PersonalFacing&);
        void dec(const V&,PersonalFacing&,const StableId&);
        Status visit(CodecVisitor&,PersonalFacing&);
        V enc(const PersonalObjective&);
        void dec(const V&,PersonalObjective&,const StableId&);
        Status visit(CodecVisitor&,PersonalObjective&);
        V enc(const PersonalPlan&);
        void dec(const V&,PersonalPlan&,const StableId&);
        Status visit(CodecVisitor&,PersonalPlan&);
        V enc(const PersonalHitLevels&);
        void dec(const V&,PersonalHitLevels&,const StableId&);
        Status visit(CodecVisitor&,PersonalHitLevels&);
        V enc(const PersonalUnit&);
        void dec(const V&,PersonalUnit&,const StableId&);
        Status visit(CodecVisitor&,PersonalUnit&);
        V enc(const PersonalEvent&);
        void dec(const V&,PersonalEvent&,const StableId&);
        Status visit(CodecVisitor&,PersonalEvent&);
        V enc(const PersonalBattle&);
        void dec(const V&,PersonalBattle&,const StableId&);
        Status visit(CodecVisitor&,PersonalBattle&);
        V enc(const PersonalFact&);
        void dec(const V&,PersonalFact&,const StableId&);
        Status visit(CodecVisitor&,PersonalFact&);
        V enc(const PersonalSource&);
        void dec(const V&,PersonalSource&,const StableId&);
        Status visit(CodecVisitor&,PersonalSource&);
        V enc(const PersonalSkill&);
        void dec(const V&,PersonalSkill&,const StableId&);
        Status visit(CodecVisitor&,PersonalSkill&);
        V enc(const PersonalTerms&);
        void dec(const V&,PersonalTerms&,const StableId&,bool allow_cargo=false);
        Status visit(CodecVisitor&,PersonalTerms&);
        V enc(const InteractionContext&);
        void dec(const V&,InteractionContext&,const StableId&);
        Status visit(CodecVisitor&,InteractionContext&);
        V enc(const PersonalPoint&);
        void dec(const V&,PersonalPoint&,const StableId&);
        Status visit(CodecVisitor&,PersonalPoint&);
        // 标量和有序集合只用于冷边界；每个记录仍须下方明确schema。
        V enc(const StableId& x){
            return V{x};
        }
        V enc(Integer x){
            return V{x};
        }
        V enc(bool x){
            return V{x};
        }
        void dec(const V& v,StableId& x,const StableId& p){
            x=boundary::string(v,p);
        }
        void dec(const V& v,Integer& x,const StableId& p){
            x=boundary::integer(v,p);
        }
        void dec(const V& v,bool& x,const StableId& p){
            x=boundary::boolean(v,p);
        }
        Status visit(CodecVisitor& c,StableId& x){
            return c.field("value",x);
        }
        Status visit(CodecVisitor& c,Integer& x){
            return c.field("value",x);
        }
        Status visit(CodecVisitor& c,bool& x){
            return c.field("value",x);
        }
        template<class T> V enc(const std::vector<T>& rows){
            A result;
            for(auto& row:rows)result.push_back(enc(row));
            return V{result};
        }
        template<class T> void dec(const V& v,std::vector<T>& rows,const StableId& p){
            rows.clear();
            for(auto& item:boundary::array(v,p)){
                T value;
                dec(item,value,p+"/"+std::to_string(rows.size()));
                rows.push_back(std::move(value));
            }
        }
        template<class T> Status visit(CodecVisitor& c,std::vector<T>& rows){
            return c.sequence("rows",rows,[](auto& codec,auto& row){
                return visit(codec,row);
            });
        }
        template<class T> V enc(const OrderedTable<T>& rows){
            D result;
            for(auto& row:rows.rows())result.emplace_back(row.first,enc(row.second));
            return V{result};
        }
        template<class T> void dec(const V& v,OrderedTable<T>& rows,const StableId& p){
            rows={};
            for(auto& item:boundary::object(v,p)){
                T value;
                dec(item.second,value,p+"/"+item.first);
                rows.insert(item.first,std::move(value));
            }
        }
        template<class T> Status visit(CodecVisitor& c,OrderedTable<T>& rows){
            std::vector<std::pair<StableId,T>> values;
            if(!c.reading())values=rows.rows();
            auto r=c.sequence("rows",values,[](auto& codec,auto& row){
                codec.field("key",row.first);
                return codec.object("record",[&](auto& nested){
                    return visit(nested,row.second);
                });
            });
            if(c.reading()&&r.ok){
                rows={};
                for(auto& row:values)if(!rows.insert(row.first,std::move(row.second)))return c.reject("DuplicatePersonalKey",row.first);
            }
            return r;
        }
        // 已有上下文复用唯一交互codec，不复制另一套会面身份。
        V enc(const InteractionContext& x){return personal_projection::encode(x);}
        void dec(const V& v,InteractionContext& x,const StableId& p){
            x=interaction_codec::decode_context(v,p);
        }
        Status visit(CodecVisitor& c,InteractionContext& x){
            return interaction_codec::visit_context(c,x);
        }
        // 两整数数组是唯一规则坐标；拒绝浮点与bool。
        V enc(const PersonalPoint& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalPoint& x,const StableId& p){
            auto& a=boundary::array(v,p);
            if(a.size()!=2)boundary::fail("InvalidPersonalPoint",p);
            x={boundary::integer(a[0],p),boundary::integer(a[1],p)};
        }
        Status visit(CodecVisitor& c,PersonalPoint& x){
            c.field("x",x.x);
            return c.field("y",x.y);
        }
        // PersonalObstacle完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalObstacle& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalObstacle& x,const StableId& p){
            boundary::fields(v,{"x","y","width","height","blocks_movement","blocks_sight"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"x",p),x.x,p+"/x");
            dec(boundary::field(v,"y",p),x.y,p+"/y");
            dec(boundary::field(v,"width",p),x.width,p+"/width");
            dec(boundary::field(v,"height",p),x.height,p+"/height");
            dec(boundary::field(v,"blocks_movement",p),x.blocks_movement,p+"/blocks_movement");
            dec(boundary::field(v,"blocks_sight",p),x.blocks_sight,p+"/blocks_sight");
        }
        Status visit(CodecVisitor& c,PersonalObstacle& x){
            c.object("x",[&](auto& codec){
                return visit(codec,x.x);
            });
            c.object("y",[&](auto& codec){
                return visit(codec,x.y);
            });
            c.object("width",[&](auto& codec){
                return visit(codec,x.width);
            });
            c.object("height",[&](auto& codec){
                return visit(codec,x.height);
            });
            c.object("blocks_movement",[&](auto& codec){
                return visit(codec,x.blocks_movement);
            });
            c.object("blocks_sight",[&](auto& codec){
                return visit(codec,x.blocks_sight);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalSpawn完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalSpawn& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalSpawn& x,const StableId& p){
            boundary::fields(v,{"side","position","heading"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"side",p),x.side,p+"/side");
            dec(boundary::field(v,"position",p),x.position,p+"/position");
            dec(boundary::field(v,"heading",p),x.heading,p+"/heading");
        }
        Status visit(CodecVisitor& c,PersonalSpawn& x){
            c.object("side",[&](auto& codec){
                return visit(codec,x.side);
            });
            c.object("position",[&](auto& codec){
                return visit(codec,x.position);
            });
            c.object("heading",[&](auto& codec){
                return visit(codec,x.heading);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalExit完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalExit& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalExit& x,const StableId& p){
            boundary::fields(v,{"side","position","radius"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"side",p),x.side,p+"/side");
            dec(boundary::field(v,"position",p),x.position,p+"/position");
            dec(boundary::field(v,"radius",p),x.radius,p+"/radius");
        }
        Status visit(CodecVisitor& c,PersonalExit& x){
            c.object("side",[&](auto& codec){
                return visit(codec,x.side);
            });
            c.object("position",[&](auto& codec){
                return visit(codec,x.position);
            });
            c.object("radius",[&](auto& codec){
                return visit(codec,x.radius);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalArena完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalArena& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalArena& x,const StableId& p){
            boundary::fields(v,{"id","name_key","width","height","visual_tag","obstacles","spawns","exits"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"id",p),x.id,p+"/id");
            dec(boundary::field(v,"name_key",p),x.name_key,p+"/name_key");
            dec(boundary::field(v,"width",p),x.width,p+"/width");
            dec(boundary::field(v,"height",p),x.height,p+"/height");
            dec(boundary::field(v,"visual_tag",p),x.visual_tag,p+"/visual_tag");
            dec(boundary::field(v,"obstacles",p),x.obstacles,p+"/obstacles");
            dec(boundary::field(v,"spawns",p),x.spawns,p+"/spawns");
            dec(boundary::field(v,"exits",p),x.exits,p+"/exits");
        }
        Status visit(CodecVisitor& c,PersonalArena& x){
            c.object("id",[&](auto& codec){
                return visit(codec,x.id);
            });
            c.object("name_key",[&](auto& codec){
                return visit(codec,x.name_key);
            });
            c.object("width",[&](auto& codec){
                return visit(codec,x.width);
            });
            c.object("height",[&](auto& codec){
                return visit(codec,x.height);
            });
            c.object("visual_tag",[&](auto& codec){
                return visit(codec,x.visual_tag);
            });
            c.object("obstacles",[&](auto& codec){
                return visit(codec,x.obstacles);
            });
            c.object("spawns",[&](auto& codec){
                return visit(codec,x.spawns);
            });
            c.object("exits",[&](auto& codec){
                return visit(codec,x.exits);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalWeapon完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalWeapon& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalWeapon& x,const StableId& p){
            boundary::fields(v,{"category","minimum_range","maximum_range","arc_degrees","damage","stamina_cost","recovery_rounds","move_distance"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"category",p),x.category,p+"/category");
            dec(boundary::field(v,"minimum_range",p),x.minimum_range,p+"/minimum_range");
            dec(boundary::field(v,"maximum_range",p),x.maximum_range,p+"/maximum_range");
            dec(boundary::field(v,"arc_degrees",p),x.arc_degrees,p+"/arc_degrees");
            dec(boundary::field(v,"damage",p),x.damage,p+"/damage");
            dec(boundary::field(v,"stamina_cost",p),x.stamina_cost,p+"/stamina_cost");
            dec(boundary::field(v,"recovery_rounds",p),x.recovery_rounds,p+"/recovery_rounds");
            dec(boundary::field(v,"move_distance",p),x.move_distance,p+"/move_distance");
        }
        Status visit(CodecVisitor& c,PersonalWeapon& x){
            c.object("category",[&](auto& codec){
                return visit(codec,x.category);
            });
            c.object("minimum_range",[&](auto& codec){
                return visit(codec,x.minimum_range);
            });
            c.object("maximum_range",[&](auto& codec){
                return visit(codec,x.maximum_range);
            });
            c.object("arc_degrees",[&](auto& codec){
                return visit(codec,x.arc_degrees);
            });
            c.object("damage",[&](auto& codec){
                return visit(codec,x.damage);
            });
            c.object("stamina_cost",[&](auto& codec){
                return visit(codec,x.stamina_cost);
            });
            c.object("recovery_rounds",[&](auto& codec){
                return visit(codec,x.recovery_rounds);
            });
            c.object("move_distance",[&](auto& codec){
                return visit(codec,x.move_distance);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalArea完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalArea& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalArea& x,const StableId& p){
            boundary::fields(v,{"shape","max_targets"},{"half_width","arc_degrees"},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"shape",p),x.shape,p+"/shape");
            if(auto f=v.find("half_width")){
                x.half_width.emplace();
                dec(*f,*x.half_width,p+"/half_width");
            }
            else x.half_width.reset();
            if(auto f=v.find("arc_degrees")){
                x.arc_degrees.emplace();
                dec(*f,*x.arc_degrees,p+"/arc_degrees");
            }
            else x.arc_degrees.reset();
            dec(boundary::field(v,"max_targets",p),x.max_targets,p+"/max_targets");
        }
        Status visit(CodecVisitor& c,PersonalArea& x){
            c.object("shape",[&](auto& codec){
                return visit(codec,x.shape);
            });
            c.optional("half_width",x.half_width,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("arc_degrees",x.arc_degrees,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.object("max_targets",[&](auto& codec){
                return visit(codec,x.max_targets);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalSkillRequirement完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalSkillRequirement& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalSkillRequirement& x,const StableId& p){
            boundary::fields(v,{"skill_id","level"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"skill_id",p),x.skill_id,p+"/skill_id");
            dec(boundary::field(v,"level",p),x.level,p+"/level");
        }
        Status visit(CodecVisitor& c,PersonalSkillRequirement& x){
            c.object("skill_id",[&](auto& codec){
                return visit(codec,x.skill_id);
            });
            c.object("level",[&](auto& codec){
                return visit(codec,x.level);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalFacing完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalFacing& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalFacing& x,const StableId& p){
            boundary::fields(v,{"side_bp","back_bp"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"side_bp",p),x.side_bp,p+"/side_bp");
            dec(boundary::field(v,"back_bp",p),x.back_bp,p+"/back_bp");
        }
        Status visit(CodecVisitor& c,PersonalFacing& x){
            c.object("side_bp",[&](auto& codec){
                return visit(codec,x.side_bp);
            });
            c.object("back_bp",[&](auto& codec){
                return visit(codec,x.back_bp);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalObjective完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalObjective& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalObjective& x,const StableId& p){
            boundary::fields(v,{"id","name_key","kind","max_rounds","injury_on_defeat","allow_withdrawal"},{"ai_model","escort_model","stamina_model","hit_model","initial_stamina","facing_damage"},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"id",p),x.id,p+"/id");
            dec(boundary::field(v,"name_key",p),x.name_key,p+"/name_key");
            dec(boundary::field(v,"kind",p),x.kind,p+"/kind");
            dec(boundary::field(v,"max_rounds",p),x.max_rounds,p+"/max_rounds");
            dec(boundary::field(v,"injury_on_defeat",p),x.injury_on_defeat,p+"/injury_on_defeat");
            dec(boundary::field(v,"allow_withdrawal",p),x.allow_withdrawal,p+"/allow_withdrawal");
            if(auto f=v.find("ai_model")){
                x.ai_model.emplace();
                dec(*f,*x.ai_model,p+"/ai_model");
            }
            else x.ai_model.reset();
            if(auto f=v.find("escort_model")){
                x.escort_model.emplace();
                dec(*f,*x.escort_model,p+"/escort_model");
            }
            else x.escort_model.reset();
            if(auto f=v.find("stamina_model")){
                x.stamina_model.emplace();
                dec(*f,*x.stamina_model,p+"/stamina_model");
            }
            else x.stamina_model.reset();
            if(auto f=v.find("hit_model")){
                x.hit_model.emplace();
                dec(*f,*x.hit_model,p+"/hit_model");
            }
            else x.hit_model.reset();
            if(auto f=v.find("initial_stamina")){
                x.initial_stamina.emplace();
                dec(*f,*x.initial_stamina,p+"/initial_stamina");
            }
            else x.initial_stamina.reset();
            if(auto f=v.find("facing_damage")){
                x.facing_damage.emplace();
                dec(*f,*x.facing_damage,p+"/facing_damage");
            }
            else x.facing_damage.reset();
        }
        Status visit(CodecVisitor& c,PersonalObjective& x){
            c.object("id",[&](auto& codec){
                return visit(codec,x.id);
            });
            c.object("name_key",[&](auto& codec){
                return visit(codec,x.name_key);
            });
            c.object("kind",[&](auto& codec){
                return visit(codec,x.kind);
            });
            c.object("max_rounds",[&](auto& codec){
                return visit(codec,x.max_rounds);
            });
            c.object("injury_on_defeat",[&](auto& codec){
                return visit(codec,x.injury_on_defeat);
            });
            c.object("allow_withdrawal",[&](auto& codec){
                return visit(codec,x.allow_withdrawal);
            });
            c.optional("ai_model",x.ai_model,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("escort_model",x.escort_model,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("stamina_model",x.stamina_model,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("hit_model",x.hit_model,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("initial_stamina",x.initial_stamina,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("facing_damage",x.facing_damage,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalPlan完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalPlan& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalPlan& x,const StableId& p){
            boundary::fields(v,{"destination","heading","action","target_id","skill_id"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"destination",p),x.destination,p+"/destination");
            dec(boundary::field(v,"heading",p),x.heading,p+"/heading");
            dec(boundary::field(v,"action",p),x.action,p+"/action");
            dec(boundary::field(v,"target_id",p),x.target_id,p+"/target_id");
            dec(boundary::field(v,"skill_id",p),x.skill_id,p+"/skill_id");
        }
        Status visit(CodecVisitor& c,PersonalPlan& x){
            c.object("destination",[&](auto& codec){
                return visit(codec,x.destination);
            });
            c.object("heading",[&](auto& codec){
                return visit(codec,x.heading);
            });
            c.object("action",[&](auto& codec){
                return visit(codec,x.action);
            });
            c.object("target_id",[&](auto& codec){
                return visit(codec,x.target_id);
            });
            c.object("skill_id",[&](auto& codec){
                return visit(codec,x.skill_id);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalHitLevels完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalHitLevels& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalHitLevels& x,const StableId& p){
            boundary::fields(v,{"martial","archery"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"martial",p),x.martial,p+"/martial");
            dec(boundary::field(v,"archery",p),x.archery,p+"/archery");
        }
        Status visit(CodecVisitor& c,PersonalHitLevels& x){
            c.object("martial",[&](auto& codec){
                return visit(codec,x.martial);
            });
            c.object("archery",[&](auto& codec){
                return visit(codec,x.archery);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalUnit完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalUnit& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalUnit& x,const StableId& p){
            boundary::fields(v,{"actor_id","side","position","heading","radius","hp","initial_hp","stamina","weapon_id","martial","learned","heart_id","statuses","recovery_until","protected"},{"skill_levels","armor_id","armor_defense","withdrawn_round"},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"actor_id",p),x.actor_id,p+"/actor_id");
            dec(boundary::field(v,"side",p),x.side,p+"/side");
            dec(boundary::field(v,"position",p),x.position,p+"/position");
            dec(boundary::field(v,"heading",p),x.heading,p+"/heading");
            dec(boundary::field(v,"radius",p),x.radius,p+"/radius");
            dec(boundary::field(v,"hp",p),x.hp,p+"/hp");
            dec(boundary::field(v,"initial_hp",p),x.initial_hp,p+"/initial_hp");
            dec(boundary::field(v,"stamina",p),x.stamina,p+"/stamina");
            dec(boundary::field(v,"weapon_id",p),x.weapon_id,p+"/weapon_id");
            dec(boundary::field(v,"martial",p),x.martial,p+"/martial");
            dec(boundary::field(v,"learned",p),x.learned,p+"/learned");
            dec(boundary::field(v,"heart_id",p),x.heart_id,p+"/heart_id");
            dec(boundary::field(v,"statuses",p),x.statuses,p+"/statuses");
            dec(boundary::field(v,"recovery_until",p),x.recovery_until,p+"/recovery_until");
            dec(boundary::field(v,"protected",p),x.protected_actor,p+"/protected");
            if(auto f=v.find("skill_levels")){
                x.skill_levels.emplace();
                dec(*f,*x.skill_levels,p+"/skill_levels");
            }
            else x.skill_levels.reset();
            if(auto f=v.find("armor_id")){
                x.armor_id.emplace();
                dec(*f,*x.armor_id,p+"/armor_id");
            }
            else x.armor_id.reset();
            if(auto f=v.find("armor_defense")){
                x.armor_defense.emplace();
                dec(*f,*x.armor_defense,p+"/armor_defense");
            }
            else x.armor_defense.reset();
            if(auto f=v.find("withdrawn_round")){
                x.withdrawn_round.emplace();
                dec(*f,*x.withdrawn_round,p+"/withdrawn_round");
            }
            else x.withdrawn_round.reset();
        }
        Status visit(CodecVisitor& c,PersonalUnit& x){
            c.object("actor_id",[&](auto& codec){
                return visit(codec,x.actor_id);
            });
            c.object("side",[&](auto& codec){
                return visit(codec,x.side);
            });
            c.object("position",[&](auto& codec){
                return visit(codec,x.position);
            });
            c.object("heading",[&](auto& codec){
                return visit(codec,x.heading);
            });
            c.object("radius",[&](auto& codec){
                return visit(codec,x.radius);
            });
            c.object("hp",[&](auto& codec){
                return visit(codec,x.hp);
            });
            c.object("initial_hp",[&](auto& codec){
                return visit(codec,x.initial_hp);
            });
            c.object("stamina",[&](auto& codec){
                return visit(codec,x.stamina);
            });
            c.object("weapon_id",[&](auto& codec){
                return visit(codec,x.weapon_id);
            });
            c.object("martial",[&](auto& codec){
                return visit(codec,x.martial);
            });
            c.object("learned",[&](auto& codec){
                return visit(codec,x.learned);
            });
            c.object("heart_id",[&](auto& codec){
                return visit(codec,x.heart_id);
            });
            c.object("statuses",[&](auto& codec){
                return visit(codec,x.statuses);
            });
            c.object("recovery_until",[&](auto& codec){
                return visit(codec,x.recovery_until);
            });
            c.object("protected",[&](auto& codec){
                return visit(codec,x.protected_actor);
            });
            c.optional("skill_levels",x.skill_levels,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("armor_id",x.armor_id,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("armor_defense",x.armor_defense,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("withdrawn_round",x.withdrawn_round,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalEvent完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalEvent& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalEvent& x,const StableId& p){
            boundary::fields(v,{},{"actor_id","target_id","action","skill_id","damage","code","status","expires_round","amount","hit_bp","roll","position"},p);
            x.field_order=boundary::order(v);
            if(auto f=v.find("actor_id")){
                x.actor_id.emplace();
                dec(*f,*x.actor_id,p+"/actor_id");
            }
            else x.actor_id.reset();
            if(auto f=v.find("target_id")){
                x.target_id.emplace();
                dec(*f,*x.target_id,p+"/target_id");
            }
            else x.target_id.reset();
            if(auto f=v.find("action")){
                x.action.emplace();
                dec(*f,*x.action,p+"/action");
            }
            else x.action.reset();
            if(auto f=v.find("skill_id")){
                x.skill_id.emplace();
                dec(*f,*x.skill_id,p+"/skill_id");
            }
            else x.skill_id.reset();
            if(auto f=v.find("damage")){
                x.damage.emplace();
                dec(*f,*x.damage,p+"/damage");
            }
            else x.damage.reset();
            if(auto f=v.find("code")){
                x.code.emplace();
                dec(*f,*x.code,p+"/code");
            }
            else x.code.reset();
            if(auto f=v.find("status")){
                x.status.emplace();
                dec(*f,*x.status,p+"/status");
            }
            else x.status.reset();
            if(auto f=v.find("expires_round")){
                x.expires_round.emplace();
                dec(*f,*x.expires_round,p+"/expires_round");
            }
            else x.expires_round.reset();
            if(auto f=v.find("amount")){
                x.amount.emplace();
                dec(*f,*x.amount,p+"/amount");
            }
            else x.amount.reset();
            if(auto f=v.find("hit_bp")){
                x.hit_bp.emplace();
                dec(*f,*x.hit_bp,p+"/hit_bp");
            }
            else x.hit_bp.reset();
            if(auto f=v.find("roll")){
                x.roll.emplace();
                dec(*f,*x.roll,p+"/roll");
            }
            else x.roll.reset();
            if(auto f=v.find("position")){
                x.position.emplace();
                dec(*f,*x.position,p+"/position");
            }
            else x.position.reset();
        }
        Status visit(CodecVisitor& c,PersonalEvent& x){
            c.optional("actor_id",x.actor_id,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("target_id",x.target_id,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("action",x.action,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("skill_id",x.skill_id,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("damage",x.damage,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("code",x.code,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("status",x.status,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("expires_round",x.expires_round,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("amount",x.amount,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("hit_bp",x.hit_bp,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("roll",x.roll,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.optional("position",x.position,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalBattle完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalBattle& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalBattle& x,const StableId& p){
            boundary::fields(v,{"id","kind","controller_id","battlefield_id","objective_id","source","created_tick","completed_tick","round","phase","units","ai_plans","outcome","last_events","result_id","applied"},{},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"id",p),x.id,p+"/id");
            if(boundary::get_string(v,"kind",p)!="personal")boundary::fail("InvalidPersonalKind",p);
            dec(boundary::field(v,"controller_id",p),x.controller_id,p+"/controller_id");
            dec(boundary::field(v,"battlefield_id",p),x.battlefield_id,p+"/battlefield_id");
            dec(boundary::field(v,"objective_id",p),x.objective_id,p+"/objective_id");
            dec(boundary::field(v,"source",p),x.source,p+"/source");
            dec(boundary::field(v,"created_tick",p),x.created_tick,p+"/created_tick");
            dec(boundary::field(v,"completed_tick",p),x.completed_tick,p+"/completed_tick");
            dec(boundary::field(v,"round",p),x.round,p+"/round");
            dec(boundary::field(v,"phase",p),x.phase,p+"/phase");
            dec(boundary::field(v,"units",p),x.units,p+"/units");
            dec(boundary::field(v,"ai_plans",p),x.ai_plans,p+"/ai_plans");
            dec(boundary::field(v,"outcome",p),x.outcome,p+"/outcome");
            dec(boundary::field(v,"last_events",p),x.last_events,p+"/last_events");
            dec(boundary::field(v,"result_id",p),x.result_id,p+"/result_id");
            dec(boundary::field(v,"applied",p),x.applied,p+"/applied");
        }
        Status visit(CodecVisitor& c,PersonalBattle& x){
            c.object("id",[&](auto& codec){
                return visit(codec,x.id);
            });
            c.object("controller_id",[&](auto& codec){
                return visit(codec,x.controller_id);
            });
            c.object("battlefield_id",[&](auto& codec){
                return visit(codec,x.battlefield_id);
            });
            c.object("objective_id",[&](auto& codec){
                return visit(codec,x.objective_id);
            });
            c.object("source",[&](auto& codec){
                return visit(codec,x.source);
            });
            c.object("created_tick",[&](auto& codec){
                return visit(codec,x.created_tick);
            });
            c.object("completed_tick",[&](auto& codec){
                return visit(codec,x.completed_tick);
            });
            c.object("round",[&](auto& codec){
                return visit(codec,x.round);
            });
            c.object("phase",[&](auto& codec){
                return visit(codec,x.phase);
            });
            c.object("units",[&](auto& codec){
                return visit(codec,x.units);
            });
            c.object("ai_plans",[&](auto& codec){
                return visit(codec,x.ai_plans);
            });
            c.object("outcome",[&](auto& codec){
                return visit(codec,x.outcome);
            });
            c.object("last_events",[&](auto& codec){
                return visit(codec,x.last_events);
            });
            c.object("result_id",[&](auto& codec){
                return visit(codec,x.result_id);
            });
            c.object("applied",[&](auto& codec){
                return visit(codec,x.applied);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // PersonalFact完整字段读写；未知字段与缺必要字段均拒绝。
        V enc(const PersonalFact& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalFact& x,const StableId& p){
            boundary::fields(v,{"kind","battle_id","result_id","tick","outcome","participants","defeated","source"},{"withdrawn"},p);
            x.field_order=boundary::order(v);
            if(boundary::get_string(v,"kind",p)!="personal_battle")boundary::fail("InvalidPersonalKind",p);
            dec(boundary::field(v,"battle_id",p),x.battle_id,p+"/battle_id");
            dec(boundary::field(v,"result_id",p),x.result_id,p+"/result_id");
            dec(boundary::field(v,"tick",p),x.tick,p+"/tick");
            dec(boundary::field(v,"outcome",p),x.outcome,p+"/outcome");
            dec(boundary::field(v,"participants",p),x.participants,p+"/participants");
            dec(boundary::field(v,"defeated",p),x.defeated,p+"/defeated");
            dec(boundary::field(v,"source",p),x.source,p+"/source");
            if(auto f=v.find("withdrawn")){
                x.withdrawn.emplace();
                dec(*f,*x.withdrawn,p+"/withdrawn");
            }
            else x.withdrawn.reset();
        }
        Status visit(CodecVisitor& c,PersonalFact& x){
            c.object("battle_id",[&](auto& codec){
                return visit(codec,x.battle_id);
            });
            c.object("result_id",[&](auto& codec){
                return visit(codec,x.result_id);
            });
            c.object("tick",[&](auto& codec){
                return visit(codec,x.tick);
            });
            c.object("outcome",[&](auto& codec){
                return visit(codec,x.outcome);
            });
            c.object("participants",[&](auto& codec){
                return visit(codec,x.participants);
            });
            c.object("defeated",[&](auto& codec){
                return visit(codec,x.defeated);
            });
            c.object("source",[&](auto& codec){
                return visit(codec,x.source);
            });
            c.optional("withdrawn",x.withdrawn,[](auto& codec,auto& value){
                return visit(codec,value);
            });
            c.object("field_order",[&](auto& codec){
                return visit(codec,x.field_order);
            });
            return c.status();
        }
        // 真实来源按kind封闭分区；道路来源不能捏造岗位上下文。
        V enc(const PersonalSource& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalSource& x,const StableId& p){
            x.kind=boundary::get_string(v,"kind",p);
            x.field_order=boundary::order(v);
            if(x.kind=="role"){
                boundary::fields(v,{"kind","context"},{},p);
                x.context.emplace();
                dec(boundary::field(v,"context",p),*x.context,p+"/context");
            }
            else if(x.kind=="encounter"){
                boundary::fields(v,{"kind","encounter_id"},{},p);
                x.encounter_id=boundary::get_string(v,"encounter_id",p);
            }
            else boundary::fail("InvalidBattleSource",p);
        }
        Status visit(CodecVisitor& c,PersonalSource& x){
            c.field("kind",x.kind);
            c.field("encounter_id",x.encounter_id);
            c.optional("context",x.context,[](auto& n,auto& v){
                return visit(n,v);
            });
            return c.object("field_order",[&](auto& n){
                return visit(n,x.field_order);
            });
        }
        // 作者来源费用默认零，只写原来存在的字段。
        V enc(const PersonalTerms& x){return personal_projection::encode(x);}
        // 货物政策只在路遇作者条款读取，缺省与切磋绝不产生处置许可。
        void dec(const V& v,PersonalTerms& x,const StableId& p,bool allow_cargo){
            if(allow_cargo)boundary::fields(v,{"battlefield_id","objective_id"},{"fee","wallet_id","cargo_policy"},p);
            else boundary::fields(v,{"battlefield_id","objective_id"},{"fee","wallet_id"},p);
            x.field_order=boundary::order(v);
            x.battlefield_id=boundary::get_string(v,"battlefield_id",p);
            x.objective_id=boundary::get_string(v,"objective_id",p);
            if(auto f=v.find("fee"))x.fee=boundary::integer(*f,p);
            if(auto f=v.find("wallet_id"))x.wallet_id=boundary::string(*f,p);
            if(auto value=v.find("cargo_policy")){
                auto path=p+"/cargo_policy";
                boundary::fields(*value,{"id","version","task_template_id","item_id","outcome","recipient","allocation","unavailable_recipient","transfer_cap","destroy_cap"},{},path);
                TaskCargoPolicy policy;
                policy.id=boundary::get_string(*value,"id",path);
                policy.version=boundary::get_string(*value,"version",path);
                policy.task_template_id=boundary::get_string(*value,"task_template_id",path);
                policy.item_id=boundary::get_string(*value,"item_id",path);
                policy.outcome=boundary::get_string(*value,"outcome",path);
                policy.recipient=boundary::get_string(*value,"recipient",path);
                policy.allocation=boundary::get_string(*value,"allocation",path);
                policy.unavailable_recipient=boundary::get_string(*value,"unavailable_recipient",path);
                policy.transfer_cap=boundary::get_int(*value,"transfer_cap",path);
                policy.destroy_cap=boundary::get_int(*value,"destroy_cap",path);
                auto valid = validate_task_cargo_policy(policy);
                if (!valid.ok)
                  boundary::fail(valid.error.original.code, path);
                // 政策采用原检查点
                // factory，保留真实对象及控制块凭据供存储图遍历。
                x.cargo_policy = checkpoint_make_shared<const TaskCargoPolicy>(
                    std::move(policy));
            }
        }
        Status visit(CodecVisitor& c,PersonalTerms& x){
            c.field("battlefield_id",x.battlefield_id);
            c.field("objective_id",x.objective_id);
            c.field("fee",x.fee);
            c.field("wallet_id",x.wallet_id);
            return c.object("field_order",[&](auto& n){
                return visit(n,x.field_order);
            });
        }
        // 技能扩展是封闭typed字段，范围和反击的原嵌套顺序单独保存。
        V enc(const PersonalSkill& x){return personal_projection::encode(x);}
        void dec(const V& v,PersonalSkill& x,const StableId& p){
            boundary::fields(v,{"id","name_key","description_key","kind","weapons","stamina_cost","power_bp","defense_bp","move_bonus","status","status_rounds","practice_ticks"},{"required_skill_level","requires_stationary","hit_bonus_bp","defend_recovery_bonus","guard_facing","area","range_modifier","counter"},p);
            x.field_order=boundary::order(v);
            dec(boundary::field(v,"id",p),x.id,p+"/id");
            dec(boundary::field(v,"name_key",p),x.name_key,p+"/name_key");
            dec(boundary::field(v,"description_key",p),x.description_key,p+"/description_key");
            dec(boundary::field(v,"kind",p),x.kind,p+"/kind");
            dec(boundary::field(v,"weapons",p),x.weapons,p+"/weapons");
            dec(boundary::field(v,"stamina_cost",p),x.stamina_cost,p+"/stamina_cost");
            dec(boundary::field(v,"power_bp",p),x.power_bp,p+"/power_bp");
            dec(boundary::field(v,"defense_bp",p),x.defense_bp,p+"/defense_bp");
            dec(boundary::field(v,"move_bonus",p),x.move_bonus,p+"/move_bonus");
            dec(boundary::field(v,"status",p),x.status,p+"/status");
            dec(boundary::field(v,"status_rounds",p),x.status_rounds,p+"/status_rounds");
            dec(boundary::field(v,"practice_ticks",p),x.practice_ticks,p+"/practice_ticks");
            if(auto a=v.find("required_skill_level")){
                x.required_skill_level.emplace();
                dec(*a,*x.required_skill_level,p+"/required_skill_level");
            }
            if(auto a=v.find("requires_stationary")){
                x.requires_stationary.emplace();
                dec(*a,*x.requires_stationary,p+"/requires_stationary");
            }
            if(auto a=v.find("hit_bonus_bp")){
                x.hit_bonus_bp.emplace();
                dec(*a,*x.hit_bonus_bp,p+"/hit_bonus_bp");
            }
            if(auto a=v.find("defend_recovery_bonus")){
                x.defend_recovery_bonus.emplace();
                dec(*a,*x.defend_recovery_bonus,p+"/defend_recovery_bonus");
            }
            if(auto a=v.find("guard_facing")){
                x.guard_facing.emplace();
                dec(*a,*x.guard_facing,p+"/guard_facing");
            }
            if(auto a=v.find("area")){
                x.area.emplace();
                dec(*a,*x.area,p+"/area");
            }
            if(auto a=v.find("range_modifier")){
                boundary::fields(*a,{"maximum_bonus"},{},p);
                x.range_order=boundary::order(*a);
                x.maximum_range_bonus=boundary::get_int(*a,"maximum_bonus",p);
            }
            if(auto a=v.find("counter")){
                boundary::fields(*a,{"damage_bp"},{},p);
                x.counter_order=boundary::order(*a);
                x.counter_damage_bp=boundary::get_int(*a,"damage_bp",p);
            }
        }
        Status visit(CodecVisitor& c,PersonalSkill& x){
            c.object("id",[&](auto& n){
                return visit(n,x.id);
            });
            c.object("name_key",[&](auto& n){
                return visit(n,x.name_key);
            });
            c.object("description_key",[&](auto& n){
                return visit(n,x.description_key);
            });
            c.object("kind",[&](auto& n){
                return visit(n,x.kind);
            });
            c.object("weapons",[&](auto& n){
                return visit(n,x.weapons);
            });
            c.object("stamina_cost",[&](auto& n){
                return visit(n,x.stamina_cost);
            });
            c.object("power_bp",[&](auto& n){
                return visit(n,x.power_bp);
            });
            c.object("defense_bp",[&](auto& n){
                return visit(n,x.defense_bp);
            });
            c.object("move_bonus",[&](auto& n){
                return visit(n,x.move_bonus);
            });
            c.object("status",[&](auto& n){
                return visit(n,x.status);
            });
            c.object("status_rounds",[&](auto& n){
                return visit(n,x.status_rounds);
            });
            c.object("practice_ticks",[&](auto& n){
                return visit(n,x.practice_ticks);
            });
            c.object("field_order",[&](auto& n){
                return visit(n,x.field_order);
            });
            c.object("range_order",[&](auto& n){
                return visit(n,x.range_order);
            });
            c.object("counter_order",[&](auto& n){
                return visit(n,x.counter_order);
            });
            c.optional("required_skill_level",x.required_skill_level,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("requires_stationary",x.requires_stationary,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("hit_bonus_bp",x.hit_bonus_bp,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("defend_recovery_bonus",x.defend_recovery_bonus,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("guard_facing",x.guard_facing,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("area",x.area,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("maximum_range_bonus",x.maximum_range_bonus,[](auto& n,auto& value){
                return visit(n,value);
            });
            c.optional("counter_damage_bp",x.counter_damage_bp,[](auto& n,auto& value){
                return visit(n,value);
            });
            return c.status();
        }
    }
    namespace personal_detail {
        BoundaryValue encode_plan(const PersonalPlan& x){
            return personal_codec::enc(x);
        }
        BoundaryValue encode_event(const PersonalEvent& x){
            return personal_codec::enc(x);
        }
        BoundaryValue encode_arena(const PersonalArena& x){
            return personal_codec::enc(x);
        }
        BoundaryValue encode_skill(const PersonalSkill& x){
            return personal_codec::enc(x);
        }
    }
    // 冷导入只读取自己拥有的battle/fact kind，其他领域由中央继续处理。
    Outcome<PersonalState> decode_personal_seed(const BoundaryValue& root){
        try{
            PersonalState s;
            for(auto& row:boundary::object(boundary::field(root,"battles","state"),"battles")){
                auto kind=boundary::get_string(row.second,"kind","battles/"+row.first);
                if(kind!="personal")continue;
                PersonalBattle b;
                personal_codec::dec(row.second,b,"battles/"+row.first);
                if(b.id!=row.first)boundary::fail("InvalidBattleSession",row.first);
                s.battles.insert(row.first,b);
            }
            for(auto& row:boundary::object(boundary::field(root,"facts","state"),"facts")){
                if(boundary::get_string(row.second,"kind",row.first)!="personal_battle")continue;
                PersonalFact f;
                personal_codec::dec(row.second,f,"facts/"+row.first);
                s.facts.insert(row.first,f);
            }
            return {true,std::move(s),{}};
        }
        catch(const boundary::DecodeFailure& e){
            return {false,{},e.error};
        }
    }
    Status emit_personal_battle_fields(ProjectionWriter& out,const PersonalBattle& battle){return personal_projection::fields(out,battle);}
    Status emit_personal_fact_fields(ProjectionWriter& out,const PersonalFact& fact){return personal_projection::fields(out,fact);}
    Status emit_personal_state_fields(ProjectionWriter& out,const PersonalState& s){personal_projection::emit(out,"battles",s.battles);return personal_projection::emit(out,"facts",s.facts);}
    Status emit_personal_state_projection(ProjectionWriter& out,std::string_view name,const PersonalState& s){return out.object(name,[&](auto& fields){return emit_personal_state_fields(fields,s);});}
    BoundaryValue encode_personal_state(const PersonalState& s){BoundaryValue value;BoundaryProjectionWriter out(value);auto status=emit_personal_state_projection(out,"",s);if(status.ok)status=out.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    Status visit_personal_checkpoint(CodecVisitor& c,PersonalState& s){
        c.object("battles",[&](auto& n){
            return personal_codec::visit(n,s.battles);
        });
        return c.object("facts",[&](auto& n){
            return personal_codec::visit(n,s.facts);
        });
    }
}
// 此codec不改检查点外壳和存档格式；组合仍由中央负责。
namespace hanmo::resident {
    // 作者目录只编译个人域消费的有限字段；显式模型、扩展和引用必须保持原合同。
    Outcome<PersonalCatalog> decode_personal_catalog(const BoundaryValue& data){
        try{
            using namespace boundary;
            PersonalCatalog c;
            bool dynamic_world=false,road_encounters=false;
            if(auto caps=data.find("required_capabilities"))for(auto& value:array(*caps,"required_capabilities")){
                auto capability=string(value,"required_capabilities");
                if(capability=="personal_combat")c.enabled=true;
                else if(capability=="native_dynamic_world")dynamic_world=true;
                else if(capability=="road_encounters")road_encounters=true;
            }
            auto table=[&](const StableId& name,auto consume){
                if(auto v=data.find(name))for(auto& row:object(*v,name))consume(row.first,row.second);
            };
            table("items",[&](auto& id,auto& v){
                if(auto weapon=v.find("weapon")){
                    PersonalWeapon w;
                    personal_codec::dec(*weapon,w,"items/"+id+"/weapon");
                    if(w.minimum_range>w.maximum_range)fail("PERSONAL_WEAPON_RANGE",id);
                    c.weapons.insert(id,w);
                }
                if(auto armor=v.find("armor")){
                    fields(*armor,{"slot","defense"},{},id);
                    auto defense=get_int(*armor,"defense",id);
                    if(get_string(*armor,"slot",id)!="body"||defense<0||defense>100||v.find("weapon")||get_string(v,"kind",id)!="commodity")fail("PERSONAL_ARMOR_ITEM",id);
                    c.armor_defenses.insert(id,defense);
                }
            });
            table("personal_skills",[&](auto& id,auto& v){
                PersonalSkill s;
                personal_codec::dec(v,s,"personal_skills/"+id);
                if(s.guard_facing&&(s.kind!="guard"||*s.guard_facing!="front"))fail("PERSONAL_GUARD_FACING",id);
                if(s.defend_recovery_bonus&&(s.kind!="heart"||*s.defend_recovery_bonus!=5))fail("PERSONAL_DEFEND_RECOVERY",id);
                if(s.requires_stationary&&(s.kind!="strike"||!*s.requires_stationary))fail("PERSONAL_STRIKE_STATIONARY",id);
                if(s.maximum_range_bonus&&(s.kind!="strike"||*s.maximum_range_bonus<1||*s.maximum_range_bonus>2000))fail("PERSONAL_STRIKE_RANGE",id);
                if(s.hit_bonus_bp&&(s.kind!="strike"||*s.hit_bonus_bp< -10000||*s.hit_bonus_bp>10000))fail("PERSONAL_HIT_BONUS",id);
                if(s.area){
                    auto& a=*s.area;
                    if(s.kind!="strike"||s.weapons!=std::vector<StableId>{"spear"}||(a.shape!="line"&&a.shape!="sector"))fail("PERSONAL_STRIKE_AREA",id);
                    if(a.shape=="line"&&(!a.half_width||*a.half_width<1||*a.half_width>2000||a.arc_degrees||a.max_targets!=2))fail("PERSONAL_STRIKE_AREA",id);
                    if(a.shape=="sector"&&(!a.arc_degrees||*a.arc_degrees<1||*a.arc_degrees>180||a.half_width||a.max_targets<2||a.max_targets>3))fail("PERSONAL_STRIKE_AREA",id);
                }
                if(s.counter_damage_bp){
                    if(s.kind!="guard"||*s.counter_damage_bp!=4000)fail("PERSONAL_COUNTER_DEFINITION",id);
                    if(!s.required_skill_level||s.required_skill_level->skill_id!="martial"||s.required_skill_level->level!=3)fail("PERSONAL_COUNTER_REQUIREMENT",id);
                    if(s.weapons.empty())fail("PERSONAL_COUNTER_WEAPON",id);
                    for(auto& category:s.weapons)if(category!="sword"&&category!="spear")fail("PERSONAL_COUNTER_WEAPON",id);
                }
                if((s.status=="none")!=(s.status_rounds==0))fail("PERSONAL_STATUS_DURATION",id);
                std::set<StableId> seen;
                for(auto& category:s.weapons)if(!seen.insert(category).second)fail("PERSONAL_DUPLICATE_WEAPON",id);
                c.skills.insert(id,s);
            });
            table("personal_objectives",[&](auto& id,auto& v){
                PersonalObjective o;
                personal_codec::dec(v,o,"personal_objectives/"+id);
                if(o.ai_model&&o.ai_model!="scored_v1")fail("PERSONAL_AI_MODEL",id);
                if(o.escort_model&&(o.kind!="escort"||o.escort_model!="exit_v1"))fail("PERSONAL_ESCORT_MODEL",id);
                if(o.stamina_model&&o.stamina_model!="round_v1")fail("PERSONAL_STAMINA_MODEL",id);
                if(o.initial_stamina&&(*o.initial_stamina<0||*o.initial_stamina>100))fail("PERSONAL_INITIAL_STAMINA",id);
                if(o.hit_model&&o.hit_model!="skill_v1")fail("PERSONAL_HIT_MODEL",id);
                if(o.facing_damage&&(o.facing_damage->side_bp!=11000||o.facing_damage->back_bp!=12500))fail("PERSONAL_FACING_DAMAGE",id);
                c.objectives.insert(id,o);
            });
            table("personal_battlefields",[&](auto& id,auto& v){
                PersonalArena a;
                personal_codec::dec(v,a,"personal_battlefields/"+id);
                for(auto& obstacle:a.obstacles)if(obstacle.x+obstacle.width>a.width||obstacle.y+obstacle.height>a.height)fail("PERSONAL_OBSTACLE_BOUNDS",id);
                std::set<Integer> sides;
                for(std::size_t i=0;i<a.spawns.size();++i){
                    auto& s=a.spawns[i];
                    sides.insert(s.side);
                    if(!personal_point_valid(a,s.position,18))fail("PERSONAL_POINT_BLOCKED",id);
                    for(std::size_t j=0;j<i;++j)if(personal_distance(s.position,a.spawns[j].position)<36)fail("PERSONAL_POINT_OVERLAP",id);
                }
                if(sides!=std::set<Integer>{0,1})fail("PERSONAL_BOTH_SIDES",id);
                sides.clear();
                for(auto& exit:a.exits){
                    sides.insert(exit.side);
                    if(!personal_point_valid(a,exit.position,18))fail("PERSONAL_POINT_BLOCKED",id);
                }
                if(sides!=std::set<Integer>{0,1})fail("PERSONAL_BOTH_SIDES",id);
                c.arenas.insert(id,a);
            });
            table("roles",[&](auto& id,auto& v){
                if(auto spar=v.find("sparring")){
                    PersonalTerms t;
                    personal_codec::dec(*spar,t,"roles/"+id+"/sparring");
                    c.sparring.insert(id,t);
                }
            });
            table("actors",[&](auto& id,auto& v){
                if(auto conflict=v.find("road_conflict")){
                    if(conflict->find("cargo_policy")&&(!c.enabled||!dynamic_world||!road_encounters))fail("TaskCargoCapabilityRequired","actors/"+id+"/road_conflict/cargo_policy");
                    PersonalTerms t;
                    personal_codec::dec(*conflict,t,"actors/"+id+"/road_conflict",true);
                    c.road_conflicts.insert(id,t);
                }
            });
            return {true,std::move(c),{}};
        }
        catch(const boundary::DecodeFailure& e){
            return {false,{},e.error};
        }
    }
    // 边界解析保存用户意图错误，个人能力、控制者和阶段首错仍由preview决定。
    Outcome<PersonalCommand> decode_personal_command(const StableId& kind,const BoundaryValue& params){
        PersonalCommand cmd;
        cmd.kind=kind;
        try{
            using namespace boundary;
            object(params,"params");
            if(kind=="battle_plan"||kind=="battle_close"){
                if(auto id=params.find("battle_id")){
                    if(id->as<StableId>())cmd.battle_id=*id->as<StableId>();
                }
                if(kind=="battle_plan"){
                    PersonalPlan p;
                    auto action=params.find("action");
                    if(!action||!action->as<StableId>()){
                        cmd.input_error="InvalidAction";
                        return {true,cmd,{}};
                    }
                    p.action=*action->as<StableId>();
                    if(p.action!="attack"&&p.action!="defend"&&p.action!="wait"&&p.action!="skill"&&p.action!="withdraw"){
                        cmd.input_error="InvalidAction";
                        return {true,cmd,{}};
                    }
                    auto point=params.find("destination"),heading=params.find("heading");
                    if(!point||!heading||!heading->as<Integer>()){
                        cmd.input_error="InvalidPlan";
                        return {true,cmd,{}};
                    }
                    try{
                        personal_codec::dec(*point,p.destination,"params/destination");
                    }
                    catch(const DecodeFailure&){
                        cmd.input_error="InvalidPlan";
                        return {true,cmd,{}};
                    }
                    p.heading=*heading->as<Integer>();
                    auto text=[](const BoundaryValue* v){
                        if(!v)return StableId{};
                        if(auto s=v->as<StableId>())return *s;
                        if(auto n=v->as<Integer>())return std::to_string(*n);
                        if(auto b=v->as<bool>())return StableId{*b?"true":"false"};
                        return StableId{"<invalid>"};
                    };
                    p.target_id=text(params.find("target_id"));
                    p.skill_id=text(params.find("skill_id"));
                    cmd.plan=p;
                }
            }
            else if(kind=="duel_start"){
                if(auto context=params.find("context")){
                    try{
                        cmd.context=interaction_codec::decode_context(*context,"params/context");
                    }
                    catch(const DecodeFailure&){
                        cmd.context=InteractionContext{};
                        cmd.context->input_error="InvalidInteractionContext";
                    }
                }
            }
            else if(kind=="encounter_fight"){
                if(auto id=params.find("encounter_id"))if(auto s=id->as<StableId>())cmd.encounter_id=*s;
            }
            else return personal_detail::fail<PersonalCommand>("UnsupportedAction");
            return {true,cmd,{}};
        }
        catch(const boundary::DecodeFailure&){
            cmd.input_error="InvalidPlan";
            return {true,cmd,{}};
        }
    }
    // 命令重新编码只写原命令字段，不泄露预览内部报价或AI计划。
    BoundaryValue encode_personal_command(const PersonalCommand& cmd){
        using V=BoundaryValue;
        V::Dict d;
        if(cmd.kind=="battle_plan"&&cmd.plan){
            d=*personal_codec::enc(*cmd.plan).as<V::Dict>();
            d.insert(d.begin(),{"battle_id",V{cmd.battle_id}});
        }
        else if(cmd.kind=="battle_close")d.emplace_back("battle_id",V{cmd.battle_id});
        else if(cmd.kind=="duel_start"&&cmd.context)d.emplace_back("context",interaction_codec::encode_context(*cmd.context));
        else if(cmd.kind=="encounter_fight")d.emplace_back("encounter_id",V{cmd.encounter_id});
        return V{d};
    }
    // 三种回执的字段存在位保旧：开战仅ID，轮次含round，关闭含outcome。
    Status emit_personal_receipt_fields(ProjectionWriter& out,const PersonalReceipt& r){out.field("battle_id",r.battle_id);if(r.round)out.field("round",*r.round);if(r.outcome)out.field("outcome",*r.outcome);return out.status();}
    BoundaryValue encode_personal_receipt(const PersonalReceipt& r){BoundaryValue value;BoundaryProjectionWriter writer(value);auto status=writer.object("",[&](auto& out){return emit_personal_receipt_fields(out,r);});if(status.ok)status=writer.finish();if(!status.ok){boundary::DecodeFailure failure(status.error.original.code,status.error.original.path);failure.error=status.error;throw failure;}return value;}
    // 报价投影只读冻结会话；结果和候选说明均由同一纯规则生成。
    BoundaryValue encode_personal_quote(const PersonalQuote& q){
        if(q.kind=="battle_plan"&&(!q.plan||!q.tactical))boundary::fail("MissingPersonalQuote","tactical_preview");
        if(q.kind=="battle_close"&&!q.result)boundary::fail("MissingPersonalQuote","result_preview");
        using V=BoundaryValue;
        using D=V::Dict;
        if(q.kind=="battle_plan")return V{D{{"ok",V{true}},{"plan",q.plan?personal_codec::enc(*q.plan):V{D{}}},{"battle_id",V{q.battle_id}},{"duration",V{Integer{0}}},{"tactical_preview",q.tactical?encode_personal_plan_projection(*q.tactical):V{D{}}}}};
        if(q.kind=="battle_close")return V{D{{"ok",V{true}},{"duration",V{Integer{0}}},{"battle_id",V{q.battle_id}},{"result_preview",q.result?encode_personal_result_projection(*q.result):V{D{}}}}};
        return V{D{{"ok",V{true}},{"duration",V{q.duration}},{"fee",V{q.fee}},{"wallet_id",V{q.wallet_id}},{"controller_id",V{q.controller_id}},{"sides",personal_codec::enc(q.sides)},{"protected_id",V{q.protected_id}},{"battlefield_id",V{q.battlefield_id}},{"objective_id",V{q.objective_id}},{"source",personal_codec::enc(q.source)}}};
    }
}

namespace hanmo::resident {
// 回执冷读取严格保四个命令原存在位，不从当前战斗重构历史结果。
Outcome<PersonalReceipt> decode_personal_receipt(const StableId& kind,const BoundaryValue& v) {
    try {
        PersonalReceipt r;
        if(kind=="duel_start"||kind=="encounter_fight")boundary::fields(v,{"battle_id"},{},"receipt/details");
        else if(kind=="battle_plan")boundary::fields(v,{"battle_id","round","outcome"},{},"receipt/details");
        else if(kind=="battle_close")boundary::fields(v,{"battle_id","outcome"},{},"receipt/details");
        else return personal_detail::fail<PersonalReceipt>("UnsupportedAction");
        r.battle_id=boundary::get_string(v,"battle_id","receipt/details");
        if(auto p=v.find("round"))r.round=boundary::integer(*p,"receipt/details/round");
        if(auto p=v.find("outcome"))r.outcome=boundary::string(*p,"receipt/details/outcome");
        return {true,r,{}};
    } catch(const boundary::DecodeFailure& e) {return {false,{},e.error};}
}
// 原检查点访客只遍历此有类型回执，不改变外围存档封装。
Status visit_personal_receipt(CodecVisitor& c,PersonalReceipt& r) {
    c.field("battle_id",r.battle_id);
    c.optional("round",r.round,[](auto& v,auto& n){return v.field("value",n);});
    return c.optional("outcome",r.outcome,[](auto& v,auto& n){return v.field("value",n);});
}
}
