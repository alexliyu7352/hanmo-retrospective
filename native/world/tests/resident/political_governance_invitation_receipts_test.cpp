// 新重建的实际来源测试；原政治签发/关闭writer与Common缓存分配均使用原接口。
#define main governance_intent_fixture_only
#include "political_governance_intents_test.cpp"
#undef main
#include "resident/domains/politics/governance_replies/political_governance_invitation_replies.hpp"
#include "resident/domains/family/family_model.hpp"
namespace {
// 原双方站在原治所，测试只包装玩家提交尾，不伪造政治邀请或已接受合同。
struct ReplyFixture : Fixture {
 GovernanceInvitationReplyState replies;FamilyState family;
 PoliticsGovernanceAccess authority{common,politics,governance,gc,org,oc,activity};
 GovernanceTaskRead task_read{common,tasks,governance_tasks};
 ReplyFixture(){common.state().metadata.player_actor_id="worker";family.founder_actor_id="worker";}
 GovernanceInvitationReplyContext context(){return {common,*cc,org,oc,governance,authority,task_read,activity,replies,{},[this](Integer tick,const StableId& fact){return read_governance_invitation_player_before_fact(common,family,nullptr,tick,fact);}};}
 // Common真实分配result并进入缓存/history之后，本域才写永久精确关联。
 std::shared_ptr<CommitResult> result(Integer request){auto r=std::make_shared<CommitResult>();r->kind=governance_decline_command;r->tick=common.tick();r->revision=++common.state().identity.revision;r->result_id=take(common.next_id("result"));common.state().request_sequences.set("worker",request);common.state().results.insert("worker:"+std::to_string(request),r);common.state().history.push_back({r->tick,r->kind,"worker",r->result_id,{}});return r;}
 // 每条回复都关闭一次真实新邀请；同一次原请求重放不进入此业务writer。
 StableId close(Integer request){auto a=issue();GovernanceInvitationReplyCommand command=GovernanceDeclineInvitation{a.authority_fact_id};auto x=context();auto q=take(preview_governance_invitation_reply(x,"worker",command));auto before=replies;auto id=take(apply_governance_invitation_reply_in_candidate(common,replies,x,"worker",command,q));check(take(governance_invitation_new_player_source(before,replies,"worker",governance_decline_command,common.tick()))==id,"candidate selects only actual new source");auto receipt=result(request);check(bind_governance_invitation_player_result(common,replies,id,"worker",request,*receipt,[](const CommitResult&){return true;}).ok,"actual cache/history binds result");return id;}
 Status valid(){auto x=context();auto source=validate_governance_invitation_replies(x);return source.ok?validate_governance_invitation_player_results(common,replies,[](const CommitResult&){return true;}):source;}
};
}
int main(){try{
 ReplyFixture f;auto first=f.close(1);auto second=f.close(2);check(f.valid().ok,"two true same-tick closures keep distinct bindings");
 auto saved=f.replies;auto common=f.common.state();
 f.common.state().results.erase("worker:1");f.common.state().history.erase(f.common.state().history.begin(),f.common.state().history.end()-1);
 check(f.valid().ok,"pruning the old cache/history keeps exact second permanent binding");
 f.replies.replies.erase(second);f.common.state().facts.erase(second);
 auto x=f.context();check(validate_governance_invitation_replies(x).ok,"remaining older source is semantically valid");
 check(!validate_governance_invitation_player_results(f.common,f.replies,[](const CommitResult&){return true;}).ok,"first retained Done cannot borrow old reply when new payload and header removed");
 f.replies=saved;f.common.state()=common;
 f.replies.replies.find(second)->player_receipt.reset();check(!f.valid().ok,"player source cannot lose binding and become NPC");
 auto cached=*f.common.state().results.find("worker:2");auto copy=std::make_shared<CommitResult>(*cached);
 check(!bind_governance_invitation_player_result(f.common,f.replies,second,"worker",2,*copy,[](const CommitResult&){return true;}).ok,"same-looking result copy is not actual cache object");
 check(!f.replies.replies.find(second)->player_receipt,"failed binder leaves no partial binding");
 check(bind_governance_invitation_player_result(f.common,f.replies,second,"worker",2,*cached,[](const CommitResult&){return true;}).ok,"actual retained cache object may finish original bind");
 check(!governance_invitation_new_player_source(f.replies,f.replies,"worker",governance_decline_command,0).ok,"cached replay has no new source");
 check(!validate_governance_invitation_player_results(f.common,f.replies,[](const CommitResult&){return false;}).ok,"wrong typed receipt cannot borrow Done feature");
 f.replies.replies.find(second)->player_receipt->result_id=f.replies.replies.find(first)->player_receipt->result_id;check(!f.valid().ok,"two replies cannot share one result");
 // 原Family typed接续只作为独立控制读夹具；不冒自然同意或完整World流程。
 ReplyFixture handed;auto historical=handed.close(1);auto control=take(handed.common.next_id("family"));FamilyChoice choice;choice.kind="family_succession";choice.guardian_actor_id="worker";choice.heir_actor_id="issuer";choice.tick=handed.common.tick();handed.family.facts.insert(control,choice);handed.family.successions.push_back(control);take(handed.common.append_fact({control,"family_succession","family"}));handed.common.state().metadata.player_actor_id="issuer";
 check(handed.valid().ok,"later same-tick true control change preserves old player closure");
 auto heads=handed.common.state().facts;handed.common.state().facts={};for(const auto& row:heads.rows()){if(row.first==historical)handed.common.state().facts.insert(control,*heads.find(control));if(row.first!=control)handed.common.state().facts.insert(row.first,row.second);}
 check(!handed.valid().ok,"control header cannot cross closure while original instance allocation remains earlier/later");handed.common.state().facts=heads;
 handed.common.state().results={};handed.common.state().history.clear();handed.family.successions.clear();handed.family.facts={};handed.common.state().facts.erase(control);handed.family.founder_actor_id="issuer";
 check(!handed.valid().ok,"cropped genuine-looking result binding cannot make original NPC into historical player");
 auto unavailable=handed.context();unavailable.player_before_fact={};check(!validate_governance_invitation_replies(unavailable).ok,"new profile requires named historical control reader");
 check(!read_governance_invitation_player_before_fact(handed.common,handed.family,nullptr,0,handed.governance.authorities.order().front()).ok,"arbitrary true authority fact cannot become closure control boundary");
 std::cout<<checks<<" checks; source fixture only, no World run claimed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
