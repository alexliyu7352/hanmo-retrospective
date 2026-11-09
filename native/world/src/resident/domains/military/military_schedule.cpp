// 复用原军政一步规则与原passages广度优先序，不在提案中移动人物、发粮或设定世界寿命。
#include "military_schedule.hpp"
#include "military_field_command_source.hpp"
#include "naval_fleet_schedule.hpp"
#include "military_lifecycle.hpp"
#include "siege_world.hpp"
#include <algorithm>
#include <deque>
#include <map>
namespace hanmo::resident {
namespace {
// 查找只借当前领域权威，不为缺失计划补造定义。
template<class T> const T* find(const std::vector<T>& rows,const StableId& id){for(auto& r:rows)if(r.id==id)return &r;return nullptr;}
// 原通道广度优先只返回第一步；无路时保持空提案。
std::optional<TravelCommandPayload> walk(const CommonCatalog& cat,const StableId& start,const StableId& target){if(start.empty()||start==target)return {};std::deque<StableId> pending{start};std::map<StableId,StableId> first{{start,""}};while(!pending.empty()){auto here=pending.front();pending.pop_front();auto place=cat.places.find(here);if(!place)return {};for(auto& next:place->passages){if(first.count(next))continue;first[next]=here==start?next:first[here];if(next==target)return TravelCommandPayload{MoveLocalCommand{first[next]}};pending.push_back(next);}}return {};}
// 同城只取首段，跨城先实际走到城口；没有路径保留等待，不伪造一次瞬移。
std::optional<ResidentMilitaryProposal> at_or_walk(const CommonRead& c,const CommonCatalog& cat,const TravelCatalog& travel,const InteractionAccess& access,const StableId& actor,const StableId& target,const MilitaryCommand& command){auto place=cat.places.find(target);if(!place)return {};auto p=c.actor(actor);if(!p)return {};auto location=access.resolve_location?access.resolve_location(actor):p->location;if(auto in=std::get_if<SettlementLocation>(&location)){if(in->place_id==target)return ResidentMilitaryProposal{actor,command};if(in->settlement_id==place->settlement_id){auto step=walk(cat,in->place_id,target);if(step)return ResidentMilitaryProposal{actor,*step};return {};}auto settlement=travel.settlements.find(in->settlement_id);if(!settlement)return {};if(in->place_id!=settlement->entry_place){auto step=walk(cat,in->place_id,settlement->entry_place);if(step)return ResidentMilitaryProposal{actor,*step};return {};}}
    if(std::holds_alternative<SettlementLocation>(location)||std::holds_alternative<WildernessLocation>(location))return ResidentMilitaryProposal{actor,TravelCommandPayload{TravelCommand{place->settlement_id,{}}}};return {};
}
// 上下文来自当前岗位真人，仍由中央正式preview核当面服务和占用。
Outcome<InteractionContext> context_for(const CommonCatalog& common,const InteractionAccess& access,const StableId& role_id){auto role=common.roles.find(role_id);auto assignment=access.role?access.role(role_id):std::nullopt;if(!role||!assignment)return {false,{},common_failure("UnknownInteractionTarget").error};return {true,{role->place_id,assignment->actor_id,role_id,{},{}},{}};}
// 既有即时军令命令只带真实实例ID，不提前执行返回或交货。
MilitaryCommand campaign_command(const StableId& kind,const CampaignRecord& campaign){CampaignManage command;command.kind=kind;command.campaign_id=campaign.id;return command;}
}
// 每位空闲NPC只提一步；返营与回报复用正式军令选择器，普通无路等待、未绑能力仍显式失败。
Outcome<std::vector<ResidentMilitaryProposal>> military_schedule_proposals(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const CommonCatalog& common,const TravelCatalog& travel,const InteractionAccess& access,const StableId& player,const MilitaryAccess& military_access,const NavalAssetState* naval,const NavalAssetCatalog* naval_catalog,bool naval_combat_enabled,const NavalCatalog* combat_catalog){
    std::vector<ResidentMilitaryProposal> result;std::vector<const MilitaryPlan*> plans;for(auto& plan:cat.plans)plans.push_back(&plan);std::sort(plans.begin(),plans.end(),[](auto a,auto b){return a->actor_id<b->actor_id;});
    for(auto plan:plans){
        auto id=plan->actor_id;if(id==player)continue;auto actor=c.actor(id);if(!actor||!actor->alive||!actor->available||!actor->action_id.empty()||(access.battle_occupied&&access.battle_occupied(id)))continue;
        bool inherited_army=false;if(cat.field_command_policy)for(const auto& army:s.armies)if(current_military_field_army_assignment(s,cat,army.id,id)){inherited_army=true;break;}if(inherited_army)continue;
        const CampaignRecord* campaign=nullptr;for(auto& v:s.campaigns)if(v.actor_id==id&&v.template_id==plan->order_id)campaign=&v;if(campaign&&campaign->status=="closed")continue;
        std::optional<ResidentMilitaryProposal> proposed;
        if(campaign&&combat_catalog&&naval){auto fleet=naval_fleet_plan_proposal(c,s,cat,common,travel,military_access,*naval,*combat_catalog,*plan,*campaign);if(!fleet.ok)return {false,{},fleet.error};if(fleet.value.handled){if(fleet.value.proposal)result.push_back(std::move(*fleet.value.proposal));continue;}}
        if(campaign){
            auto pending=military_pending_siege(s,campaign->army_id);
            if(!pending.empty()){
                auto siege=find(s.sieges,pending);
                if(siege&&siege->phase=="result"&&siege_for_actor(s,player)!=pending)result.push_back({id,MilitaryCommand{SiegeClose{pending,{}}}});
                continue;
            }
            const NavalFleetRecord* fleet=nullptr;
            if(naval)for(const auto& f:naval->fleets)if(f.status=="active"&&f.army_id==campaign->army_id){if(fleet)return {false,{},common_failure("NavalArmyAssignedTwice").error};fleet=&f;}
            // 原一人一提案先求真实解编报价；拒绝只等待，不能反复下必拒陆战命令。
            auto disband=[&]()->Status {if(!fleet||!naval_catalog)return common_failure("NavalWorldCapabilityRequired");NavalCommand command=NavalDisband{"npc-release:"+fleet->id,fleet->id};auto quote=preview_naval_command(c,s,cat,military_access,*naval,*naval_catalog,id,command,naval_combat_enabled);if(!quote.ok)return quote.error.kind==ErrorKind::rule?common_success():Status{false,{},quote.error};result.push_back({id,std::move(command)});return common_success();};
            if(campaign->status=="operating"&&fleet){auto status=disband();if(!status.ok)return {false,{},status.error};continue;}
            if(campaign->status=="returning"){
                auto choice=military_return_destination(c,s,cat,military_access,campaign->id);
                if(!choice.ok){if(choice.error.kind!=ErrorKind::rule)return {false,{},choice.error};continue;}
                auto camp=find(cat.garrisons,choice.value.return_garrison_id);if(!camp)return {false,{},common_failure("UnknownGarrison",choice.value.return_garrison_id).error};
                if(fleet){const auto* at=std::get_if<SettlementLocation>(&actor->location);if(at&&at->settlement_id==choice.value.destination_settlement_id){auto status=disband();if(!status.ok)return {false,{},status.error};continue;}}
                ArmyLogistics command;command.kind="army_return";command.army_id=campaign->army_id;command.garrison_id=camp->id;
                proposed=at_or_walk(c,common,travel,access,id,camp->place_id,command);
                if(proposed)result.push_back(std::move(*proposed));
                continue;
            }
            if(campaign->status=="awaiting_report"){
                auto roles=military_report_roles(c,s,cat,military_access,campaign->id);if(roles.empty())continue;
                auto role_id=roles.front();auto location=access.resolve_location?access.resolve_location(id):actor->location;auto city=std::get_if<SettlementLocation>(&location);
                if(city)for(auto& candidate:roles){auto role=common.roles.find(candidate);auto place=role?common.places.find(role->place_id):nullptr;if(place&&place->settlement_id==city->settlement_id){role_id=candidate;break;}}
                auto context=context_for(common,access,role_id);if(!context.ok)return {false,{},context.error};
                CampaignManage command;command.kind="campaign_report";command.campaign_id=campaign->id;command.context=context.value;
                proposed=at_or_walk(c,common,travel,access,id,context.value.place_id,command);
                if(proposed)result.push_back(std::move(*proposed));
                continue;
            }
            auto authority=find(s.offices,campaign->office_id);
            if(!authority||!authority->enabled||authority->holder_actor_id!=id){result.push_back({id,campaign_command(campaign->army_id.empty()?"campaign_cancel":"campaign_return",*campaign)});continue;}
        }
        auto office=find(s.offices,plan->office_id);auto definition=find(cat.offices,plan->office_id);auto camp=definition?find(cat.garrisons,definition->garrison_id):nullptr;if(!office||!definition||!camp)return {false,{},common_failure("MissingMilitaryDefinitions").error};
        if(!office->enabled){
            if(std::find(common.actor_ids.begin(),common.actor_ids.end(),id)==common.actor_ids.end()||!office->appointment_id.empty())continue;
            auto context=context_for(common,access,definition->appointing_role_id);if(!context.ok)return {false,{},context.error};
            proposed=at_or_walk(c,common,travel,access,id,context.value.place_id,OfficeAccept{plan->office_id,context.value,{}});
        }else if(office->holder_actor_id==id){
            auto order=find(cat.orders,plan->order_id);if(!order)return {false,{},common_failure("UnknownMilitaryOrder").error};
            if(!campaign){auto context=context_for(common,access,order->issuer_role_id);if(!context.ok)return {false,{},context.error};proposed=at_or_walk(c,common,travel,access,id,context.value.place_id,CampaignAccept{plan->order_id,context.value,{},{}});}
            else if(campaign->status=="accepted")proposed=at_or_walk(c,common,travel,access,id,camp->place_id,ArmyForm{campaign->id,plan->unit_ids,{},plan->food_quantity,plan->money});
            else if(campaign->status=="operating"){
                if(campaign->objective_result)proposed=ResidentMilitaryProposal{id,campaign_command("campaign_return",*campaign)};
                else if(order->objective_kind=="deliver_supply")proposed=ResidentMilitaryProposal{id,campaign_command("campaign_deliver",*campaign)};
                else if(order->objective_kind=="occupy")proposed=ResidentMilitaryProposal{id,MilitaryCommand{SiegeStart{campaign->id,{}}}};
            }else if(campaign->status=="mustered"){auto place=common.places.find(camp->place_id);auto settlement=place?travel.settlements.find(place->settlement_id):nullptr;if(!settlement)return {false,{},common_failure("UnknownSettlement").error};proposed=at_or_walk(c,common,travel,access,id,settlement->entry_place,CampaignDepart{campaign->id,{}});}
        }
        if(proposed)result.push_back(std::move(*proposed));
    }
    // 新统帅沿原失败军令只作返营一步；不把旧计划转给新人或重开进攻目标。
    if(cat.field_command_policy){std::vector<const ArmyRecord*> armies;for(const auto& army:s.armies)if(army.status=="active")armies.push_back(&army);std::sort(armies.begin(),armies.end(),[](const auto* a,const auto* b){return a->id<b->id;});
        for(const auto* army:armies){const auto& id=army->commander_actor_id;const auto* source=current_military_field_army_assignment(s,cat,army->id,id);const auto* campaign=find(s.campaigns,army->order_id);const auto* actor=c.actor(id);
            if(!source||!campaign||campaign->status!="returning"||id==player||!actor||!actor->alive||!actor->available||!actor->action_id.empty()||(access.battle_occupied&&access.battle_occupied(id))||!military_pending_siege(s,army->id).empty())continue;
            if(std::any_of(result.begin(),result.end(),[&](const auto& row){return row.actor_id==id;}))continue;
            auto choice=military_return_destination(c,s,cat,military_access,campaign->id);if(!choice.ok){if(choice.error.kind!=ErrorKind::rule)return {false,{},choice.error};continue;}const auto* camp=find(cat.garrisons,choice.value.return_garrison_id);if(!camp)return {false,{},common_failure("UnknownGarrison",choice.value.return_garrison_id).error};
            const NavalFleetRecord* fleet=nullptr;if(naval)for(const auto& f:naval->fleets)if(f.status=="active"&&f.army_id==army->id){if(fleet)return {false,{},common_failure("NavalArmyAssignedTwice",army->id).error};fleet=&f;}
            if(!naval&&military_access.army_embarked&&military_access.army_embarked(army->id))return {false,{},common_failure("NavalWorldCapabilityRequired",army->id).error};
            if(fleet){const auto* at=std::get_if<SettlementLocation>(&actor->location);if(at&&at->settlement_id==choice.value.destination_settlement_id){if(!naval_catalog)return {false,{},common_failure("NavalWorldCapabilityRequired",army->id).error};NavalCommand command=NavalDisband{"npc-field-return:"+source->id+":"+fleet->id,fleet->id};auto quote=preview_naval_command(c,s,cat,military_access,*naval,*naval_catalog,id,command,naval_combat_enabled);if(!quote.ok){if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};continue;}result.push_back({id,std::move(command)});continue;}}
            ArmyLogistics command;command.kind="army_return";command.army_id=army->id;command.garrison_id=camp->id;auto next=at_or_walk(c,common,travel,access,id,camp->place_id,command);if(next)result.push_back(std::move(*next));
        }
    }
    return {true,std::move(result),{}};
}
}
