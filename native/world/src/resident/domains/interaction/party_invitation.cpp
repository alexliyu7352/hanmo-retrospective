#include "resident/kernel/catalog_section_observation.hpp"
// 随行接受与本人退出沿原Common primitive写入；没有第二成员账、旅程钟或工资算法。
#include "party_invitation.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 普通准入失败保留领域码，调用方丢弃整份候选。
template<class T> Outcome<T> reject(const StableId& code){return {false,{},common_failure(code).error};}
// 旧报价不得跨读档、提交或半日继续使用。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 本人或当面领队的成员关系从唯一共同表取得，重复绑定属于坏源而非没有队伍。
Outcome<const PartyRecord*> member_party(const CommonStore& common,const StableId& actor){
    const PartyRecord* found=nullptr;
    for(const auto& row:common.state().parties.rows())if(std::find(row.second.members.begin(),row.second.members.end(),actor)!=row.second.members.end()||row.second.leader_id==actor){if(row.first!=row.second.id)return reject<const PartyRecord*>("InvalidPartyMembers");if(found)return reject<const PartyRecord*>("DuplicatePartyMember");found=&row.second;}
    if(found){
        if(found->id.empty()||found->members.size()<2||found->members.size()>16||found->members.front()!=found->leader_id||found->terms.size()+1!=found->members.size())return reject<const PartyRecord*>("InvalidPartyMembers");
        std::set<StableId> seen;
        for(const auto& id:found->members){const auto* person=common.actor(id);if(!person||!seen.insert(id).second)return reject<const PartyRecord*>("InvalidPartyMembers");
            if(id==found->leader_id){if(std::holds_alternative<FollowingLocation>(person->location)||std::holds_alternative<InArmyLocation>(person->location))return reject<const PartyRecord*>("NestedParty");}
            else{const auto* location=std::get_if<FollowingLocation>(&person->location);if(!location||location->party_id!=found->id||!found->terms.find(id))return reject<const PartyRecord*>("PartyLocationMismatch");}
        }
    }
    return {true,found,{}};
}
// 邀请双方与离队本人须有行动资格；被离开的领队可已失能，仍不准拆行动、战斗或军旅。
Status participant_available(const CommonStore& common,const InteractionAccess& access,const StableId& actor,bool require_eligibility=true){
    const auto* person=common.actor(actor);if(!person)return common_failure("UnknownActor");
    if((require_eligibility&&(!person->alive||!person->available))||!person->action_id.empty())return common_failure("ActorBusy");
    if(!access.army_id||!access.battle_occupied)return common_failure("InteractionCoordinationUnavailable");
    if(!access.army_id(actor).empty()||std::holds_alternative<InArmyLocation>(person->location))return common_failure("ArmyNotReturned");
    if(access.battle_occupied(actor))return common_failure("BattlePending");return common_success();
}
}
Status validate_party_invitation_catalog(const PartyInvitationCatalog& invitations,const InteractionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 56, "PartyInvitationCatalog", "validate_party_invitation_catalog", &invitations);

    if(!catalog.common)return common_failure("MissingInteractionCatalog");
    std::set<StableId> ids;std::set<std::tuple<StableId,StableId,StableId,StableId>> nodes;
    for(const auto& row:invitations.invitations){
        if(row.id.empty()||!ids.insert(row.id).second||row.leader_actor_id.empty()||row.invitee_actor_id.empty()||row.leader_actor_id==row.invitee_actor_id||!nodes.emplace(row.leader_actor_id,row.invitee_actor_id,row.dialogue_id,row.node_id).second)return common_failure("InvalidPartyInvitationDefinition");
        for(const auto* actor:{&row.leader_actor_id,&row.invitee_actor_id})if(std::find(catalog.common->actor_ids.begin(),catalog.common->actor_ids.end(),*actor)==catalog.common->actor_ids.end())return common_failure("UnknownPartyInvitationActor");
        const auto* role=catalog.common->roles.find(row.role_id);if(!role||role->companion_cost_per_tick<0||std::find(role->services.begin(),role->services.end(),"join_party")==role->services.end())return common_failure("InvalidPartyInvitationRole");
        const DialogueDefinition* dialogue=nullptr;for(const auto& d:catalog.dialogues)if(d.id==row.dialogue_id){if(dialogue)return common_failure("InvalidPartyInvitationDialogue");dialogue=&d;}
        if(!dialogue||dialogue->context_role!=row.role_id)return common_failure("InvalidPartyInvitationDialogue");
        const DialogueNode* node=nullptr;for(const auto& n:dialogue->nodes)if(n.id==row.node_id){if(node)return common_failure("InvalidPartyInvitationNode");node=&n;}
        if(!node||(node->kind!="say"&&node->kind!="choice"&&node->kind!="action"))return common_failure("InvalidPartyInvitationNode");
    }
    return common_success();
}
Outcome<PartyInvitationQuote> preview_party_invitation(const CommonStore& common,const InteractionState& interaction,const InteractionCatalog& catalog,const PartyInvitationCatalog& invitations,const InteractionAccess& access,const StableId& actor,const StableId& id){
    // 先取本人实际已显示节点，再解析这份邀请；猜作者ID不能探查尚未当面提出的邀请。
    const InteractionSession* visible=nullptr;for(const auto& current:interaction.interactions)if(current.actor_id==actor){if(visible)return reject<PartyInvitationQuote>("AmbiguousAuthoredNodeSource");visible=&current;}
    if(!visible||(visible->phase!="say"&&visible->phase!="choice"&&visible->phase!="action"))return reject<PartyInvitationQuote>("PartyInvitationNotKnown");
    const PartyInvitationDefinition* invitation=nullptr;for(const auto& row:invitations.invitations)if(row.id==id&&row.invitee_actor_id==actor&&row.leader_actor_id==visible->context.provider_actor_id&&row.dialogue_id==visible->dialogue_id&&row.node_id==visible->node_id&&row.role_id==visible->context.role_id){if(invitation)return reject<PartyInvitationQuote>("InvalidPartyInvitationDefinition");invitation=&row;}
    if(!invitation)return reject<PartyInvitationQuote>("PartyInvitationNotKnown");
    auto proof=preview_authored_node_source(common,interaction,catalog,access,actor,invitation->leader_actor_id);
    if(!proof.ok)return {false,{},proof.error};
    if(proof.value.dialogue_id!=invitation->dialogue_id||proof.value.node_id!=invitation->node_id||proof.value.context.role_id!=invitation->role_id||proof.value.context.kind)return reject<PartyInvitationQuote>("PartyInvitationNotKnown");
    auto valid=validate_party_invitation_catalog(invitations,catalog);if(!valid.ok)return {false,{},valid.error};
    auto service=validate_interaction(common,interaction,catalog,access,actor,proof.value.context,"join_party");if(!service.ok)return {false,{},service.error};
    for(const auto* person:{&actor,&invitation->leader_actor_id}){auto free=participant_available(common,access,*person);if(!free.ok)return {false,{},free.error};}
    auto member=member_party(common,actor);if(!member.ok)return {false,{},member.error};if(member.value)return reject<PartyInvitationQuote>("AlreadyInParty");
    const auto* own=std::get_if<SettlementLocation>(&common.actor(actor)->location);const auto* leader=std::get_if<SettlementLocation>(&common.actor(invitation->leader_actor_id)->location);
    if(!own||!leader||own->settlement_id!=leader->settlement_id||own->place_id!=leader->place_id)return reject<PartyInvitationQuote>("PartyNeedsSettlement");
    auto party=member_party(common,invitation->leader_actor_id);if(!party.ok)return {false,{},party.error};if(party.value&&party.value->leader_id!=invitation->leader_actor_id)return reject<PartyInvitationQuote>("NotPartyLeader");
    if(party.value&&party.value->members.size()>=16)return reject<PartyInvitationQuote>("PartyFull");
    for(const auto& current:interaction.interactions)if(current.id!=proof.value.session_id&&(current.actor_id==actor||current.actor_id==invitation->leader_actor_id||current.context.provider_actor_id==actor||current.context.provider_actor_id==invitation->leader_actor_id))return reject<PartyInvitationQuote>("ParticipantBusy");
    const auto* role=catalog.common->roles.find(invitation->role_id);
    return {true,{std::move(proof.value),id,party.value?party.value->id:StableId{},role->id,role->companion_cost_per_tick},{}};
}
Outcome<StableId> accept_party_invitation_in_candidate(CommonStore& common,const InteractionState& interaction,const InteractionCatalog& catalog,const PartyInvitationCatalog& invitations,const InteractionAccess& access,const StableId& actor,const StableId& id,const PartyInvitationQuote& quote){
    if(quote.source.actor_id!=actor||quote.invitation_id!=id)return reject<StableId>("PartyInvitationQuoteMismatch");
    auto still=revalidate_authored_node_source(common,interaction,catalog,access,quote.source);if(!still.ok)return {false,{},still.error};
    auto fresh=preview_party_invitation(common,interaction,catalog,invitations,access,actor,id);if(!fresh.ok)return {false,{},fresh.error};
    if(fresh.value.source.provider_actor_id!=quote.source.provider_actor_id||fresh.value.party_id!=quote.party_id||fresh.value.role_id!=quote.role_id||fresh.value.cost_per_tick!=quote.cost_per_tick)return reject<StableId>("PartyInvitationQuoteMismatch");
    auto own_before=capture_actor_social_occupancy(common,interaction,access,actor);if(!own_before.ok)return {false,{},own_before.error};auto leader_before=capture_actor_social_occupancy(common,interaction,access,quote.source.provider_actor_id);if(!leader_before.ok)return {false,{},leader_before.error};
    PartyRecord party;StableId party_id=quote.party_id;
    if(party_id.empty()){auto allocated=common.next_id("party");if(!allocated.ok)return allocated;party_id=allocated.value;party.id=party_id;party.leader_id=quote.source.provider_actor_id;party.members={party.leader_id};party.created_tick=common.tick();}
    else party=*common.state().parties.find(party_id);
    party.members.push_back(actor);party.terms.insert(actor,{quote.role_id,quote.cost_per_tick,{}});
    auto written=common.apply_party(party_id,party);if(!written.ok)return {false,{},written.error};
    ActorMutation mutation;mutation.actor_id=actor;mutation.location=FollowingLocation{party_id};auto moved=common.apply_actor(mutation);if(!moved.ok)return {false,{},moved.error};PartyMembershipAcceptance accepted;accepted.kind=PartyAcceptanceKind::authored_invitation;accepted.leader_actor_id=quote.source.provider_actor_id;accepted.role_id=quote.role_id;accepted.cost_per_tick=quote.cost_per_tick;accepted.context=quote.source.context;accepted.authored_node=PartyAcceptanceNode{quote.source.actor_id,quote.source.session_id,quote.source.dialogue_id,quote.source.node_id,quote.source.node_kind};accepted.authored_declaration_id=quote.invitation_id;auto recorded=record_captured_party_member_acceptance(access,actor,own_before.value,ActorSocialOccupancyCause::party_invitation_accepted,party_id,actor,accepted);if(!recorded.ok)return {false,{},recorded.error};recorded=record_captured_actor_social_occupancy(access,quote.source.provider_actor_id,leader_before.value,ActorSocialOccupancyCause::party_invitation_accepted,party_id,actor);if(!recorded.ok)return {false,{},recorded.error};return {true,party_id,{}};
}
Outcome<StableId> party_following_leader(const CommonStore& common,const StableId& actor){
    const auto* person=common.actor(actor);if(!person)return reject<StableId>("UnknownActor");const auto* following=std::get_if<FollowingLocation>(&person->location);if(!following)return reject<StableId>("NotPartyMember");
    auto actual=member_party(common,actor);if(!actual.ok)return {false,{},actual.error};
    if(!actual.value||actual.value->id!=following->party_id||actual.value->leader_id==actor||!common.state().parties.find(following->party_id))return reject<StableId>("DanglingParty");return {true,actual.value->leader_id,{}};
}
Outcome<StableId> party_clock_leader(const CommonStore& common,const StableId& actor){
    auto leader=party_following_leader(common,actor);if(!leader.ok)return leader;
    const auto* person=common.actor(actor);if(!person->alive||!person->available)return reject<StableId>("ActorUnavailable");
    if(!person->action_id.empty()){
        const auto* action=common.action(person->action_id);
        if(!action||std::find(action->participants.begin(),action->participants.end(),actor)==action->participants.end())return reject<StableId>("PartyMemberActionMismatch");
    }
    return leader;
}
Outcome<PartyLeaveQuote> preview_leave_party(const CommonStore& common,const InteractionAccess& access,const StableId& actor){
    auto leader=party_following_leader(common,actor);if(!leader.ok)return {false,{},leader.error};
    auto own=participant_available(common,access,actor);if(!own.ok)return {false,{},own.error};
    auto stopped=participant_available(common,access,leader.value,false);if(!stopped.ok)return {false,{},stopped.error};
    const auto* place=std::get_if<SettlementLocation>(&common.actor(leader.value)->location);if(!place)return reject<PartyLeaveQuote>("PartyNeedsSettlement");
    const auto& following=std::get<FollowingLocation>(common.actor(actor)->location);
    return {true,{common.state().identity,actor,following.party_id,leader.value,place->settlement_id,place->place_id},{}};
}
Outcome<StableId> leave_party_in_candidate(CommonStore& common,const InteractionAccess& access,const StableId& actor,const PartyLeaveQuote& quote){
    if(quote.actor_id!=actor||!same_identity(quote.source_identity,common.state().identity))return reject<StableId>("StalePartyLeaveQuote");
    auto fresh=preview_leave_party(common,access,actor);if(!fresh.ok)return {false,{},fresh.error};const auto& expected=fresh.value;
    if(quote.party_id!=expected.party_id||quote.leader_actor_id!=expected.leader_actor_id||quote.settlement_id!=expected.settlement_id||quote.place_id!=expected.place_id)return reject<StableId>("StalePartyLeaveQuote");
    auto own_before=capture_actor_social_occupancy(common,access,actor);if(!own_before.ok)return {false,{},own_before.error};auto leader_before=capture_actor_social_occupancy(common,access,quote.leader_actor_id);if(!leader_before.ok)return {false,{},leader_before.error};
    auto party=*common.state().parties.find(quote.party_id);party.members.erase(std::remove(party.members.begin(),party.members.end(),actor),party.members.end());party.terms.erase(actor);
    ActorMutation mutation;mutation.actor_id=actor;mutation.location=SettlementLocation{quote.settlement_id,quote.place_id};auto moved=common.apply_actor(mutation);if(!moved.ok)return {false,{},moved.error};
    auto written=common.apply_party(quote.party_id,party.members.size()==1?std::optional<PartyRecord>{}:std::optional<PartyRecord>{party});if(!written.ok)return {false,{},written.error};auto recorded=record_captured_actor_social_occupancy(access,actor,own_before.value,ActorSocialOccupancyCause::party_left,quote.party_id,actor);if(!recorded.ok)return {false,{},recorded.error};recorded=record_captured_actor_social_occupancy(access,quote.leader_actor_id,leader_before.value,ActorSocialOccupancyCause::party_left,quote.party_id,actor);if(!recorded.ok)return {false,{},recorded.error};return {true,quote.party_id,{}};
}
}
