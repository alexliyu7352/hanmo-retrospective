// 私庄长期休整逐半日消费原私粮；缺粮和取消保留真实日期与账目，不跳世界时钟。
#include "growth_internal.hpp"
#include "growth_fixed_execution.hpp"
#include "growth_recovery_progress.hpp"
#include <algorithm>
namespace hanmo::resident::growth {
    namespace {
        std::vector<StableId> mouths(const CommonStore& common,const GrowthAccess& access,const StableId& actor_id,const RestEstateView& estate){std::vector<StableId> candidates{actor_id};
            if(access.rest_children){auto children=access.rest_children(actor_id);
                candidates.insert(candidates.end(),children.begin(),children.end());
            }std::sort(candidates.begin(),candidates.end());
            std::vector<StableId> people;
            for(const auto& person:candidates){auto* actor=common.actor(person);
                auto location=common.resolved_location(person);
                auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
                if(actor&&actor->alive&&actor->available&&(person==actor_id||std::find(estate.resident_actor_ids.begin(),estate.resident_actor_ids.end(),person)==estate.resident_actor_ids.end())&&place&&place->place_id==estate.place_id)people.push_back(person);
            }return people;
        }
    }
    Outcome<std::vector<StableId>> original_rest_mouths(const CommonStore& common,const GrowthAccess& access,const StableId& actor,const RestEstateView& estate){return {true,mouths(common,access,actor,estate),{}};}
    Outcome<GrowthQuote> preview_rest(const CommonStore& common,const GrowthState&,const GrowthAccess& access,const StableId& id,const GrowthCommand& command,const StableId& own){GrowthQuote q;
        q.command=command;
        auto* actor=common.actor(id);
        if(!actor)return {false,{},common_failure("UnknownActor").error};
        if(command.kind=="rest_cancel"){auto* action=common.action(actor->action_id);
            if(!action||action->kind!="long_rest"||action->actor_id!=id)return {false,{},common_failure("NotResting").error};
            q.data=RestCancellation{action->id};
            return {true,std::move(q),{}};
        }if(command.duration<1||command.duration>720)return {false,{},common_failure("InvalidRestDuration").error};
        auto estate=access.rest_estate?access.rest_estate(command.estate_id):std::nullopt;
        if(!estate)return {false,{},common_failure("UnknownEstate").error};
        if(estate->owner_actor_id!=id)return {false,{},common_failure("RestNeedsOwnEstate").error};
        if(!actor->alive||!actor->available||(!actor->action_id.empty()&&actor->action_id!=own))return {false,{},common_failure("ActorBusy").error};
        auto location=common.resolved_location(id);
        auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
        if(!place||place->place_id!=estate->place_id)return {false,{},common_failure("RestNotAtEstate").error};
        if(!access.rest_quiet||!access.rest_quiet(id))return {false,{},common_failure("RestNeedsQuiet").error};
        if(common.tick()>2147483647-command.duration)return {false,{},common_failure("RestClockOverflow").error};
        auto people=mouths(common,access,id,*estate);
        auto count=common.count(estate->inventory_id,estate->food_item_id,{Owner{Owner::Kind::actor,id,{}, {}}});
        if(!count.ok)return {false,{},count.error};
        if(count.value<Integer(people.size())*estate->ration_per_person)return {false,{},common_failure("RestFoodShortage").error};
        q.duration=command.duration;
        q.data=RestTerms{command.estate_id,estate->food_item_id,command.duration,estate->ration_per_person,std::move(people)};
        return {true,std::move(q),{}};
    }
    Status start_rest(CommonStore& common,GrowthState& state,const ActionCoreView& action,const GrowthQuote& quote,std::optional<Integer> actual_started_tick){if(actual_started_tick&&(*actual_started_tick<common.tick()||action.start_tick!=*actual_started_tick))return common_failure("LongRestActualStartClockMismatch",action.id);const auto& terms=std::get<RestTerms>(quote.data);
        RestFact fact;
        fact.actor_id=action.actor_id;
        fact.estate_id=terms.estate_id;
        fact.start_tick=actual_started_tick?*actual_started_tick:common.tick();
        fact.duration=terms.duration;
        fact.food_item_id=terms.food_item_id;
        fact.ration_per_person=terms.ration_per_person;
        state.actions.insert(action.id,GrowthAction{quote.command,quote,{}});
        state.facts.insert(action.id,std::move(fact));
        return common.append_fact({action.id,"long_rest","services"});
    }
    static Outcome<GrowthStep> step_rest_impl(CommonStore& common,GrowthState& state,const GrowthAccess& access,const ActionCoreView& action,const GrowthFixedStepPermit* exact){auto* payload=state.facts.find(action.id);
        auto* fact=payload?std::get_if<RestFact>(payload):nullptr;
        if(!fact)return {false,{},common_failure("InvalidRestAction").error};
        auto estate=access.rest_estate?access.rest_estate(fact->estate_id):std::nullopt;
        if(!estate)return {false,{},common_failure("UnknownEstate").error};
        auto people=mouths(common,access,action.actor_id,*estate);
        auto quantity=Integer(people.size())*fact->ration_per_person;
        auto* actor=common.actor(action.actor_id);
        auto location=common.resolved_location(action.actor_id);
        auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
        StableId reason;
        if(estate->owner_actor_id!=action.actor_id||!place||place->place_id!=estate->place_id||!actor||!actor->alive||!actor->available)reason="RestInterrupted";
        else if(exact&&(!access.rest_quiet||!access.rest_quiet(action.actor_id)))reason="RestNeedsQuiet";
        else{auto count=common.count(estate->inventory_id,fact->food_item_id,{Owner{Owner::Kind::actor,action.actor_id,{}, {}}});
            if(!count.ok)return {false,{},count.error};
            if(count.value<quantity)reason="RestFoodShortage";
        }if(!exact||reason.empty())++fact->elapsed;
        fact->last_mouths.clear();
        ActionMutation mutation;
        mutation.action=action;
        mutation.remaining_ticks=reason.empty()?(exact?exact->required_half_days()-exact->ordinal():action.remaining_ticks-1):0;
        if(reason.empty()){ResourceRecipe recipe;
            if(quantity>0)recipe.operations.push_back(ConsumeLots{estate->inventory_id,fact->food_item_id,quantity,{Owner{Owner::Kind::actor,action.actor_id,{}, {}}},{}});
            auto applied=common.apply_resources(recipe);
            if(!applied.ok)return {false,{},applied.error};
            fact->consumed+=quantity;
            fact->last_mouths=people;
            for(const auto& person:people){auto* target=common.actor(person);
                ActorMutation m;
                m.actor_id=person;
                m.fatigue=std::max<Integer>(0,target->fatigue-4);
                m.hp=std::min<Integer>(access.personal_recovery?growth_injury_hp_limit(common.state().actors.find(person)->injury):100,target->hp+1);
                auto changed=common.apply_actor(m);
                if(!changed.ok)return {false,{},changed.error};
        }}auto applied=common.apply_action(mutation);
        if(!applied.ok)return {false,{},applied.error};
        GrowthStep step;
        step.completed=*mutation.remaining_ticks==0;
        step.effect.kind="long_rest";
        step.effect.success=reason.empty();
        step.effect.code=reason;
        if(step.completed){fact->phase="completed";
            fact->completed_tick=exact?exact->at().half_day_tick:common.tick()+1;
            fact->stop_reason=reason.empty()?"duration":reason;
        }return {true,std::move(step),{}};
    }
    Outcome<GrowthStep> step_rest(CommonStore& common,GrowthState& state,const GrowthAccess& access,const ActionCoreView& action){return step_rest_impl(common,state,access,action,nullptr);}
    Outcome<GrowthStep> step_rest_at_fixed_gate(CommonStore& common,GrowthState& state,const GrowthAccess& access,const ActionCoreView& action,const GrowthFixedStepPermit& exact){auto valid=validate_growth_fixed_step_permit(exact,common,state,action);if(!valid.ok)return {false,{},valid.error};return step_rest_impl(common,state,access,action,&exact);}
    Status validate_rest(const CommonStore& common,const GrowthState& state,const GrowthAccess& access,const StableId& id,const RestFact& f){auto estate=access.rest_estate?access.rest_estate(f.estate_id):std::nullopt;
        if(id.rfind("action:",0)!=0||!common.actor(f.actor_id)||!estate||f.duration<1||f.duration>720||f.elapsed<0||f.elapsed>f.duration||f.start_tick<0||f.start_tick>common.tick()||f.consumed<0||f.consumed>2147483647)return common_failure("InvalidRestFact",id);
        if(f.food_item_id!=estate->food_item_id||f.ration_per_person!=estate->ration_per_person)return common_failure("InvalidRestFood",id);
        std::vector<StableId> seen;
        for(const auto& person:f.last_mouths){if(!common.actor(person)||std::find(seen.begin(),seen.end(),person)!=seen.end())return common_failure("InvalidRestMouths",id);
            seen.push_back(person);
        }auto* action=common.action(id);
        auto* payload=state.actions.find(id);
        if(f.phase=="active"){if(!action||!payload||f.completed_tick!=-1||!f.stop_reason.empty())return common_failure("OrphanRestAction",id);
            if(action->kind!="long_rest"||action->actor_id!=f.actor_id||action->participants!=std::vector<StableId>{f.actor_id}||action->start_tick!=f.start_tick||action->remaining_ticks!=f.duration-f.elapsed||common.tick()!=f.start_tick+f.elapsed||payload->request.estate_id!=f.estate_id||payload->request.duration!=f.duration)return common_failure("RestClockMismatch",id);
            auto location=common.resolved_location(f.actor_id);
            auto* place=location.ok?std::get_if<SettlementLocation>(&location.value):nullptr;
            if(estate->owner_actor_id!=f.actor_id||!place||place->place_id!=estate->place_id)return common_failure("RestNotAtEstate",id);
        }else if(f.phase=="completed"){if(action||payload||f.completed_tick!=f.start_tick+f.elapsed||f.completed_tick>common.tick()||(f.stop_reason!="duration"&&f.stop_reason!="cancelled"&&f.stop_reason!="RestFoodShortage"&&f.stop_reason!="RestInterrupted"))return common_failure("InvalidRestCompletion",id);
            if(f.stop_reason=="duration"&&f.elapsed!=f.duration)return common_failure("RestClockMismatch",id);
        }else return common_failure("InvalidRestPhase",id);
        return common_success();
    }
} // 庄务先供庄民，休整只供本人和未入庄的在场受监护子女，避免同半日重复口粮。

namespace hanmo::resident {Outcome<std::vector<StableId>> read_growth_rest_mouths(const CommonStore& common,const GrowthAccess& access,const StableId& actor,const RestEstateView& estate){return growth::original_rest_mouths(common,access,actor,estate);}}
