// 只声明原收帖人的固定答复与有限原节点路径；未知/歧义/自动业务节点一律拒绝。
#include "visit_appointment_intents.hpp"
#include "visit_appointment_detail.hpp"
#include <set>
namespace hanmo::resident {namespace {
void unique(const BoundaryValue& v,const StableId& path){std::set<StableId> keys;for(const auto& f:boundary::object(v,path))if(!keys.insert(f.first).second)boundary::fail("DuplicateVisitAppointmentIntentField",path);}
const DialogueNode* node(const DialogueDefinition& d,const StableId& id){const DialogueNode* found=nullptr;for(const auto& n:d.nodes)if(n.id==id){if(found)return nullptr;found=&n;}return found;}
}
const VisitAppointmentIntent* visit_appointment_intent(const VisitAppointmentIntentCatalog& c,const StableId& id){for(const auto& i:c.intents)if(i.id==id)return &i;return nullptr;}
const VisitAppointmentIntent* visit_appointment_reception_intent(const VisitAppointmentIntentCatalog& c,const StableId& id){for(const auto& i:c.intents)if(i.reception_intent_id==id)return &i;return nullptr;}
Status validate_visit_appointment_intents(const VisitAppointmentIntentCatalog& c,const VisitAppointmentCatalog& invitations,const VisitReceptionCatalog& reception,const VisitMessageCatalog& messages,const PersonalAppointmentCatalog& appointments,const InteractionCatalog& interactions,const VisitReplyCatalog* ordinary){
    auto valid=validate_visit_reception_catalog(reception,messages,interactions);if(!valid.ok)return valid;valid=validate_visit_appointment_catalog(invitations,messages,appointments,interactions,ordinary);if(!valid.ok)return valid;
    if(c.version!=1||invitations.version!=1||c.intents.empty()||c.intents.size()>4096)return common_failure("VisitAppointmentIntentProfileRequired");std::set<StableId> ids,originals;
    for(const auto& i:c.intents){const auto* r=visit_reception_intent(reception,i.reception_intent_id);const auto* p=visit_appointment_policy(invitations,i.invitation_policy_id);const auto* m=p?visit_appointment_detail::original(messages,p->message_policy_id):nullptr;
        if(i.id.empty()||i.id.size()>256||!ids.insert(i.id).second||!originals.insert(i.reception_intent_id).second||!r||r->purpose!="receive"||!m||r->message_policy_id!=p->message_policy_id||r->actor_id!=m->recipient_actor_id||i.route.empty()||i.route.size()>16)return common_failure("InvalidVisitAppointmentIntent",i.id);
        const DialogueDefinition* d=nullptr;for(const auto& item:interactions.dialogues)if(item.id==m->dialogue_id)d=&item;if(!d)return common_failure("InvalidVisitAppointmentIntentDialogue",i.id);
        for(const auto& step:r->route){const auto* original_node=node(*d,step.node_id);if(!original_node||original_node->command)return common_failure("VisitAppointmentUnsafeIntentRoute",i.id);}
        StableId current=m->receive_node_id;std::set<StableId> seen;
        for(const auto& step:i.route){const auto* n=node(*d,current);if(!n||n->command||current==p->offer_node_id||step.node_id!=current||!seen.insert(current).second)return common_failure("InvalidVisitAppointmentIntentRoute",i.id);
            if(n->kind=="say"){if(!step.option_id.empty()||n->next.empty())return common_failure("InvalidVisitAppointmentIntentRoute",i.id);current=n->next;}
            else if(n->kind=="choice"){const DialogueOption* option=nullptr;for(const auto& o:n->options)if(o.id==step.option_id){if(option)return common_failure("InvalidVisitAppointmentIntentRoute",i.id);option=&o;}if(!option||option->next.empty())return common_failure("InvalidVisitAppointmentIntentRoute",i.id);current=option->next;}
            else return common_failure("VisitAppointmentUnsafeIntentRoute",i.id);
        }
        if(current!=p->offer_node_id||seen.count(current))return common_failure("VisitAppointmentIntentDeclarationUnreachable",i.id);
    }return common_success();
}
Outcome<VisitAppointmentIntentCatalog> decode_visit_appointment_intents(const BoundaryValue& v,const VisitAppointmentCatalog& invitations,const VisitReceptionCatalog& reception,const VisitMessageCatalog& messages,const PersonalAppointmentCatalog& appointments,const InteractionCatalog& interactions,const VisitReplyCatalog* ordinary){try{
    using namespace boundary;unique(v,"visit_appointment_intents");fields(v,{"version","intents"},{},"visit_appointment_intents");VisitAppointmentIntentCatalog c;c.version=get_int(v,"version","visit_appointment_intents");const auto& rows=array(field(v,"intents","visit_appointment_intents"),"intents");if(rows.size()>4096)fail("VisitAppointmentIntentLimit","intents");
    for(const auto& row:rows){unique(row,"intent");fields(row,{"id","reception_intent_id","invitation_policy_id","route"},{},"intent");VisitAppointmentIntent i;i.id=get_string(row,"id","intent");i.reception_intent_id=get_string(row,"reception_intent_id","intent");i.invitation_policy_id=get_string(row,"invitation_policy_id","intent");const auto& route=array(field(row,"route","intent"),"route");if(route.size()>16)fail("VisitAppointmentIntentRouteLimit",i.id);for(const auto& row:route){unique(row,"step");fields(row,{"node_id"},{"option_id"},"step");VisitReceptionStep step;step.node_id=get_string(row,"node_id","step");if(row.find("option_id"))step.option_id=get_string(row,"option_id","step");i.route.push_back(std::move(step));}c.intents.push_back(std::move(i));}
    auto valid=validate_visit_appointment_intents(c,invitations,reception,messages,appointments,interactions,ordinary);return valid.ok?Outcome<VisitAppointmentIntentCatalog>{true,std::move(c),{}}:Outcome<VisitAppointmentIntentCatalog>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
BoundaryValue encode_visit_appointment_intents(const VisitAppointmentIntentCatalog& c){using V=BoundaryValue;V::Array rows;for(const auto& i:c.intents){V::Array route;for(const auto& step:i.route){auto row=boundary::obj({{"node_id",V{step.node_id}}});if(!step.option_id.empty())row.as<V::Dict>()->emplace_back("option_id",V{step.option_id});route.push_back(std::move(row));}rows.push_back(boundary::obj({{"id",V{i.id}},{"reception_intent_id",V{i.reception_intent_id}},{"invitation_policy_id",V{i.invitation_policy_id}},{"route",V{std::move(route)}}}));}return boundary::obj({{"version",V{c.version}},{"intents",V{std::move(rows)}}});}
}
