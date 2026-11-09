// 原返营代办权源共用读口；只核身份，现场营地与动作占用仍由具体命令核验。
#pragma once
#include "resident/domains/military/military_model.hpp"
namespace hanmo::resident {
// 沿原规则：活统帅仅本人；统帅死亡才允许该现公职或该军令报告岗位代办。
inline Outcome<MilitaryReturnAuthority> military_return_authority(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const ArmyRecord& army,const StableId& actor){
 if(army.commander_actor_id==actor)return {true,{"commander",actor,{}},{}};
 const auto fail=[](){return Outcome<MilitaryReturnAuthority>{false,{},common_failure("ArmyCommandRequired").error};};
 const auto* commander=c.actor(army.commander_actor_id);if(!commander||commander->alive)return fail();
 const CampaignRecord* campaign=nullptr;for(const auto& p:s.campaigns)if(p.id==army.order_id)campaign=&p;if(!campaign)return fail();
 const OfficeRecord* office=nullptr;for(const auto& o:s.offices)if(o.id==campaign->office_id)office=&o;
 const OfficeDefinition* definition=nullptr;for(const auto& d:cat.offices)if(d.id==campaign->office_id)definition=&d;
 const auto* person=c.actor(actor);StableId organization;
 if(definition){for(const auto& g:cat.garrisons)if(g.id==definition->garrison_id)organization=g.organization_id;for(const auto& h:cat.strongholds)if(h.garrison_id==definition->garrison_id)for(const auto& current:s.strongholds)if(current.id==h.id)organization=current.owner_organization_id;}
 if(person&&person->alive&&person->available&&office&&definition&&office->enabled&&office->holder_actor_id==actor&&organization==definition->organization_id)return {true,{"office",office->id,{}},{}};
 const MilitaryOrderDefinition* order=nullptr;for(const auto& o:cat.orders)if(o.id==campaign->template_id)order=&o;
 const auto* role=order&&a.common_catalog?a.common_catalog->roles.find(order->report_role_id):nullptr;
 const auto current=role&&a.interaction_access&&a.interaction_access->role?a.interaction_access->role(role->id):std::nullopt;
 if(role&&role->organization_id&&*role->organization_id==army.organization_id&&current&&current->enabled&&current->actor_id==actor)return {true,{"report_role",role->id,{}},{}};
 return fail();
}
}
