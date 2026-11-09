// 同一世界候选持有成员、原工资和本人知识；拒绝不提交部分身份或付款。
#include "named_membership.hpp"
#include "native_organization_runtime.hpp"
#include "resident/domains/organization/named_membership_validation.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/domains/military/siege_world.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
// UI仅传签发身份，实时授权及精确同一grant由领域重新读取。
ActingAuthority authority(const BoundaryValue& value){using namespace boundary;fields(value,{"organization_id","office_id","authority_epoch","actor_id"},{},"authority");return {get_string(value,"organization_id","authority"),get_string(value,"office_id","authority"),get_int(value,"authority_epoch","authority"),get_string(value,"actor_id","authority")};}
// 已收任务物品仍须归还，只有等待收钱不构成人身责任。
bool cargo(const CommonStore& c,const StableId& actor,const StableId& task){const auto* person=c.actor(actor);const auto* bag=person?c.inventory(person->inventory_id):nullptr;return bag&&std::any_of(bag->lots.begin(),bag->lots.end(),[&](const LotView& lot){return lot.owner.kind==Owner::Kind::task&&lot.owner.id==task&&lot.quantity>0;});}
}
bool named_membership_command_kind(const StableId& kind){return kind=="organization_offer_membership"||kind=="organization_answer_membership"||kind=="organization_withdraw_membership";}
Outcome<CommandPayload> decode_named_membership_command(const StableId& kind,const BoundaryValue& params){try{using namespace boundary;
    if(kind=="organization_offer_membership"){fields(params,{"authority","target_actor_id","terms_id","context"},{},"params");OrganizationOfferMembershipCommand command;command.authority=authority(field(params,"authority","params"));command.target_actor_id=get_string(params,"target_actor_id","params");command.terms_id=get_string(params,"terms_id","params");command.context=interaction_codec::decode_context(field(params,"context","params"),"params/context");return {true,{kind,std::move(command)},{}};}
    if(kind=="organization_answer_membership"){fields(params,{"actor_id","offer_id","expected_offer_revision","answer","source_fact_id"},{},"params");OrganizationAnswerMembershipCommand command;command.actor_id=get_string(params,"actor_id","params");command.offer_id=get_string(params,"offer_id","params");command.expected_offer_revision=get_int(params,"expected_offer_revision","params");command.source_fact_id=get_string(params,"source_fact_id","params");const auto answer=get_string(params,"answer","params");if(answer=="accept")command.answer=MembershipDecisionKind::accept;else if(answer=="decline")command.answer=MembershipDecisionKind::decline;else if(answer=="defer")command.answer=MembershipDecisionKind::defer;else fail("InvalidMembershipAnswer","params/answer");return {true,{kind,std::move(command)},{}};}
    if(kind=="organization_withdraw_membership"){fields(params,{"authority","offer_id","expected_offer_revision"},{},"params");OrganizationWithdrawMembershipCommand command;command.authority=authority(field(params,"authority","params"));command.offer_id=get_string(params,"offer_id","params");command.expected_offer_revision=get_int(params,"expected_offer_revision","params");return {true,{kind,std::move(command)},{}};}
    return {false,{},common_failure("UnsupportedAction",kind).error};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
// 责任只读投影来自原action、会话、军务和Task，不复制另一份busy表。
Outcome<MembershipReleaseView> BundleServices::membership_release_view(const StableId& actor)const{
    const auto& w=world_;const auto* person=w.common.actor(actor);if(!person)return {false,{},common_failure("UnknownActor",actor).error};Integer actual_tick=w.common.tick();
    // 完整命令在真实阶段 M 读取责任，不能拿发布前的 Common tick 冒当前时刻。
    if(w.catalog&&w.catalog->organization&&w.catalog->organization->exact_fact_clock_enabled){auto at=w.actual_phase_moment();if(!at.ok)return {false,{},at.error};actual_tick=at.value.half_day_tick;}
    MembershipReleaseView out{actor,actual_tick,w.common.revision(),{}};
    auto block=[&](MembershipReleaseBlockerKind kind,const StableId& owner,const StableId& fact){if(std::none_of(out.blockers.begin(),out.blockers.end(),[&](const auto& b){return b.kind==kind&&b.owner_id==owner;}))out.blockers.push_back({kind,owner,fact});};
    if(!person->action_id.empty())block(MembershipReleaseBlockerKind::personal_action,person->action_id,person->action_id);
    for(const auto& meeting:w.interaction.interactions)if(meeting.actor_id==actor||meeting.context.provider_actor_id==actor)block(MembershipReleaseBlockerKind::interaction,meeting.id,meeting.id);
    if(in_open_battle(actor)||!siege_for_actor(w.military,actor).empty())block(MembershipReleaseBlockerKind::battle,actor,actor);
    for(const auto& army:w.military.armies)if(army.status=="active"&&(army.commander_actor_id==actor||std::find(army.member_actor_ids.begin(),army.member_actor_ids.end(),actor)!=army.member_actor_ids.end()))block(MembershipReleaseBlockerKind::military_service,army.id,army.formation_source);
    for(const auto& campaign:w.military.campaigns)if(campaign.actor_id==actor&&campaign.status!="closed"&&campaign.status!="cancelled")block(MembershipReleaseBlockerKind::military_service,campaign.id,campaign.id);
    for(const auto& row:w.tasks.tasks.rows()){const auto& task=row.second;if(task.assignee!=actor)continue;const bool held=cargo(w.common,actor,task.id);if(held)block(MembershipReleaseBlockerKind::entrusted_cargo,task.id,task.id);if(task.status=="accepting"||task.status=="accepted"||task.status=="active"||task.status=="in_progress"||task.status=="ready")block(MembershipReleaseBlockerKind::task_execution,task.id,task.id);}
    for(const auto& row:w.tasks.contracts.rows()){const auto& contract=row.second;if(contract.executor_actor_id!=actor&&!(contract.assignee.kind==SubjectKind::actor&&contract.assignee.id==actor))continue;if((contract.status==ContractStatus::accepted||contract.status==ContractStatus::active)&&!contract.execution_stopped)block(MembershipReleaseBlockerKind::task_execution,contract.id,contract.current_task_id.value_or(contract.id));for(const auto& id:contract.historical_task_ids)if(cargo(w.common,actor,id))block(MembershipReleaseBlockerKind::entrusted_cargo,contract.id,id);if(contract.current_task_id&&cargo(w.common,actor,*contract.current_task_id))block(MembershipReleaseBlockerKind::entrusted_cargo,contract.id,*contract.current_task_id);}
    for(const auto& row:w.organization.activities.rows()){const auto& a=row.second;if(a.status!=OrgActivityStatus::approved&&a.status!=OrgActivityStatus::active&&a.status!=OrgActivityStatus::waiting)continue;for(const auto& p:a.participation)if(p.subject.kind==SubjectKind::actor&&p.subject.id==actor&&p.binding_kind==ParticipationKind::personal_action)block(MembershipReleaseBlockerKind::personal_action,a.id,p.binding_id);}
    return {true,std::move(out),{}};
}
Outcome<MembershipOfferQuote> preview_named_membership(const WorldBundle& w,const StableId& actor,const CommandPayload& command){
    if(!w.named_membership_enabled())return {false,{},common_failure("NamedMembershipCapabilityRequired",{},ErrorKind::unsupported_capability).error};
    // 仅显式精确事实选型借真实世界钟；旧profile保留原半日路径。
    NativeOrganizationAuthorityClock clock(w);
    const auto* actual=w.catalog->organization->exact_fact_clock_enabled?&clock:nullptr;
    if(const auto* c=std::get_if<OrganizationOfferMembershipCommand>(&command.data))return preview_organization_offer_membership(w.common,w.organization,*w.catalog->organization,*w.catalog->local_economy,w.social,w.interaction,w.services,actor,*c,actual);
    if(const auto* c=std::get_if<OrganizationAnswerMembershipCommand>(&command.data))return preview_organization_answer_membership(w.common,w.organization,*w.catalog->organization,w.local_economy,*w.catalog->local_economy,w.social,w.interaction,w.services,w.services,actor,*c,actual);
    if(const auto* c=std::get_if<OrganizationWithdrawMembershipCommand>(&command.data))return preview_organization_withdraw_membership(w.common,w.organization,*w.catalog->organization,*w.catalog->local_economy,actor,*c,actual);
    return {false,{},common_failure("UnsupportedAction",command.kind).error};
}
Outcome<MembershipOfferEffects> apply_named_membership(WorldBundle& w,const StableId& actor,const CommandPayload& command){
    auto checked=preview_named_membership(w,actor,command);if(!checked.ok)return {false,{},checked.error};
    // 写入候选重新借其原钟，不保留预览世界的clock或派生时刻。
    NativeOrganizationAuthorityClock clock(w);
    const auto* actual=w.catalog->organization->exact_fact_clock_enabled?&clock:nullptr;
    if(const auto* c=std::get_if<OrganizationOfferMembershipCommand>(&command.data))return apply_organization_offer_membership(w.common,w.organization,*w.catalog->organization,*w.catalog->local_economy,w.social,w.interaction,w.services,actor,*c,actual);
    if(const auto* c=std::get_if<OrganizationAnswerMembershipCommand>(&command.data))return apply_organization_answer_membership(w.common,w.organization,*w.catalog->organization,w.local_economy,*w.catalog->local_economy,w.social,w.interaction,w.services,w.services,actor,*c,actual);
    if(const auto* c=std::get_if<OrganizationWithdrawMembershipCommand>(&command.data))return apply_organization_withdraw_membership(w.common,w.organization,*w.catalog->organization,*w.catalog->local_economy,actor,*c,actual);
    return {false,{},common_failure("UnsupportedAction",command.kind).error};
}
// 缓存回执也必须绑定真实已记录的本人决定与唯一成员来源，不能仅靠合法JSON形状。
Status validate_named_membership_world(const WorldBundle& w){
    if(w.catalog->organization->named_membership_enabled!=w.named_membership_enabled()||w.catalog->director->named_membership!=w.named_membership_enabled()||w.director.named_membership!=w.named_membership_enabled())return common_failure("NamedMembershipProfileMismatch");
    if(w.named_membership_enabled()&&!w.regional_life_enabled())return common_failure("NamedMembershipRegionalLifeRequired");
    if(!w.named_membership_enabled()){if(!w.organization.membership_offers.empty()||!w.organization.membership_decisions.empty()||!w.local_economy.membership_wage_bindings.empty()||!w.local_economy.membership_terminal_fractions.empty())return common_failure("UnexpectedNamedMembershipState");for(const auto& row:w.organization.members.rows())if(row.second.episode_source)return common_failure("UnexpectedNamedMembershipState",row.first);}
    else{auto valid=validate_named_membership(w.common,w.organization,*w.catalog->organization,w.local_economy,*w.catalog->local_economy);if(!valid.ok)return valid;}
    // 新实例在二进制恢复也核共同计数器，不能只依赖冷JSON递归扫描。
    auto instance=[&](const StableId& id,const StableId& prefix){if(id.rfind(prefix+":",0)!=0)return false;const auto digits=id.substr(prefix.size()+1);Integer value=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&value<w.common.state().next_instance_id&&std::to_string(value)==digits;};
    for(const auto& row:w.organization.membership_offers.rows())if(!instance(row.first,"membership_offer"))return common_failure("InstanceCounterCollision",row.first);
    for(const auto& row:w.organization.members.rows())if(row.second.episode_source){if(!instance(row.first,"org_member"))return common_failure("InstanceCounterCollision",row.first);for(const auto& id:row.second.duty_ids)if(!instance(id,"org_duty"))return common_failure("InstanceCounterCollision",id);}
    for(const auto& row:w.local_economy.membership_wage_bindings.rows())if(!instance(row.first,"membership_wage")||!instance(row.second.obligation_id,"membership_obligation"))return common_failure("InstanceCounterCollision",row.first);
    for(const auto& row:w.local_economy.membership_terminal_fractions.rows())if(!instance(row.first,"membership_terminal"))return common_failure("InstanceCounterCollision",row.first);
    std::set<StableId> cached_sources;
    for(const auto& row:w.common.state().results.rows())if(row.second){
        const auto& receipt=*row.second;const auto* effect=receipt.details?std::get_if<MembershipOfferEffects>(&receipt.details->data):nullptr;
        auto bad=[&](){return common_failure("MembershipReceiptSourceMismatch",row.first);};
        if(!effect){if(named_membership_command_kind(receipt.kind))return bad();continue;}
        if(!w.named_membership_enabled()||!named_membership_command_kind(receipt.kind))return bad();
        const auto* offer=w.organization.membership_offers.find(effect->offer_id);if(!offer)return bad();
        const auto split=row.first.rfind(':');if(split==StableId::npos)return bad();const auto actor=row.first.substr(0,split),digits=row.first.substr(split+1);Integer request=0;
        const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),request);const auto* sequence=w.common.state().request_sequences.find(actor);
        if(!sequence||parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||request<1||request>*sequence||std::to_string(request)!=digits)return bad();
        // 当前64条缓存必在2000条历史内；请求人与实际命令历史必须同源。
        const HistoryRecord* history=nullptr;const HistoryRecord* previous=nullptr;const HistoryRecord* before=nullptr;
        for(const auto& item:w.common.state().history){if(item.result_id==receipt.result_id){if(history)return bad();history=&item;before=previous;}previous=&item;}
        if(!history||history->actor_id!=actor||history->kind!=receipt.kind||history->tick!=receipt.tick)return bad();
        // 同tick可连续发出多份邀请；事实必须落在本命令的全局分配区间，不能整组交换回执。
        auto ordinal=[](const StableId& id){const auto at=id.rfind(':');if(at==StableId::npos)return Integer{0};Integer number=0;const auto digits=id.substr(at+1);const auto decoded=std::from_chars(digits.data(),digits.data()+digits.size(),number);return decoded.ec==std::errc{}&&decoded.ptr==digits.data()+digits.size()&&number>0&&std::to_string(number)==digits?number:Integer{0};};
        const auto upper=ordinal(receipt.result_id),lower=before?ordinal(before->result_id):Integer{0};
        if(upper<=lower||(before&&lower==0))return bad();
        std::vector<StableId> expected;
        if(receipt.kind=="organization_offer_membership"){
            if(actor!=offer->issuer_actor_id||receipt.tick!=offer->created_tick||!effect->decision_id.empty()||!effect->member_episode_id.empty())return bad();
            expected={offer->source_fact_id};
        }else if(receipt.kind=="organization_answer_membership"){
            const auto* decision=w.organization.membership_decisions.find(effect->decision_id);
            if(!decision||decision->offer_id!=offer->id||decision->target_actor_id!=actor||decision->tick!=receipt.tick)return bad();
            expected={decision->id};
            if(decision->kind==MembershipDecisionKind::accept){
                const auto* member=w.organization.members.find(effect->member_episode_id);
                if(!member||!member->episode_source||member->id!=offer->member_episode_id||member->actor_id!=actor||member->episode_source->offer_id!=offer->id||member->episode_source->decision_id!=decision->id||member->episode_source->joined_tick!=receipt.tick)return bad();
                expected.push_back(member->episode_source->joined_fact_id);
            }else if(!effect->member_episode_id.empty())return bad();
        }else{
            const auto* withdrawal=w.organization.facts.find(offer->decision_fact_id);
            if(offer->status!=MembershipOfferStatus::withdrawn||!withdrawal||withdrawal->kind!="membership_withdrawn"||withdrawal->activity_id!=offer->id||withdrawal->actor_id!=actor||withdrawal->tick!=receipt.tick||!effect->decision_id.empty()||!effect->member_episode_id.empty())return bad();
            expected={withdrawal->id};
        }
        if(effect->fact_ids!=expected||effect->receipt_ids!=expected||!cached_sources.insert(expected.front()).second)return bad();
        for(const auto& id:expected)if(!w.common.state().facts.find(id)||ordinal(id)<=lower||ordinal(id)>=upper)return bad();
    }
    return common_success();
}
}
