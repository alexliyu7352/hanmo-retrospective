// 复用原真实政治签发夹具；本门不把有限域调用冒成玩家World或完整任务结算。
#define main governance_intent_original_fixture_main
#include "political_governance_intents_test.cpp"
#undef main
#include "resident/domains/politics/governance_replies/political_governance_invitation_replies.hpp"
#include "resident/domains/family/family_model.hpp"

// 原签发事实经唯一读口传入，Task使用S3真实未受领查询而非本域空集合假定。
int main() {
    try {
        Fixture f;
        f.common.state().metadata.player_actor_id = "worker";
        const auto original = f.issue();
        PoliticsGovernanceAccess authority(f.common, f.politics, f.governance, f.gc, f.org, f.oc, f.activity);
        GovernanceTaskRead tasks(f.common, f.tasks, f.governance_tasks);
        GovernanceInvitationReplyState replies;FamilyState family;family.founder_actor_id="worker";
        GovernanceInvitationReplyContext context{f.common, *f.cc, f.org, f.oc, f.governance,
            authority, tasks, f.activity, replies, {},[&](Integer tick,const StableId& fact){return read_governance_invitation_player_before_fact(f.common,family,nullptr,tick,fact);}};
        GovernanceInvitationReplyCommand decline = GovernanceDeclineInvitation{original.authority_fact_id};
        auto quote = take(preview_governance_invitation_reply(context, "worker", decline));
        check(quote.source.kind == GovernanceInvitationReplyKind::declined, "original recipient may decline an unaccepted real invitation");
        check(!preview_governance_invitation_reply(context, "other", decline).ok, "unrelated actor cannot inspect or close invitation");
        f.common.state().actors.find("issuer")->location = SettlementLocation{"town", "elsewhere"};
        check(!preview_governance_invitation_reply(context, "worker", decline).ok, "remote issuer does not receive invented refusal");
        f.common.state().actors.find("issuer")->location = SettlementLocation{"town", "hall"};
        const auto id = take(apply_governance_invitation_reply_in_candidate(f.common, replies, context, "worker", decline, quote));
        check(validate_governance_invitation_replies(context).ok, "actual close source passes semantic history before result binding");
        check(f.common.wallet("public")->balance == 200 && f.tasks.tasks.empty() && f.governance.authorities.size() == 1,
              "closing writes no public payment, acceptance, or replacement invitation");
        check(!preview_governance_invitation_reply(context, "worker", decline).ok, "new business request cannot reuse old close fact");
        GovernanceInvitationReplyAccess read(context);
        auto source = take(read.governance_invitation_reply(original.authority_fact_id));
        check(source && source->fact_id == id && source->actor_id == "worker", "reader returns exact original closure");
        auto encoded = encode_governance_invitation_replies(replies);
        check(compare_boundary(encoded, encode_governance_invitation_replies(take(decode_governance_invitation_replies(encoded)))).exact(),
              "strict new JSON segment preserves original source fields");
        replies.replies.find(id)->counterpart_actor_id = "other";
        check(!validate_governance_invitation_replies(context).ok, "cannot substitute another listener into permanent refusal");
        replies.replies.find(id)->counterpart_actor_id = "issuer";
        auto old_head = *f.common.state().facts.find(original.authority_fact_id);
        f.common.state().facts.erase(original.authority_fact_id); f.common.state().facts.insert(old_head.id, old_head);
        check(!validate_governance_invitation_replies(context).ok, "same-tick original invitation must precede closure in actual Common order");

        Fixture issuer;
        issuer.common.state().metadata.player_actor_id = "issuer";
        auto issued = issuer.issue();
        PoliticsGovernanceAccess issued_read(issuer.common, issuer.politics, issuer.governance, issuer.gc, issuer.org, issuer.oc, issuer.activity);
        GovernanceTaskRead original_tasks(issuer.common, issuer.tasks, issuer.governance_tasks);
        GovernanceInvitationReplyState withdrawn;FamilyState issuer_family;issuer_family.founder_actor_id="issuer";
        GovernanceInvitationReplyContext withdrawing{issuer.common, *issuer.cc, issuer.org, issuer.oc, issuer.governance,
            issued_read, original_tasks, issuer.activity, withdrawn, {},[&](Integer tick,const StableId& fact){return read_governance_invitation_player_before_fact(issuer.common,issuer_family,nullptr,tick,fact);}};
        GovernanceInvitationReplyCommand withdraw = PoliticalWithdrawGovernanceInvitation{issued.authority_fact_id, {"city", "office", 0, "issuer"}};
        check(!preview_governance_invitation_reply(withdrawing, "issuer", withdraw).ok, "old commission Grant does not silently acquire withdrawal power");
        issuer.oc.grants.find("grant")->allowed_command_kinds.push_back(governance_withdraw_command);
        auto permitted = take(preview_governance_invitation_reply(withdrawing, "issuer", withdraw));
        auto withdrawn_id = take(apply_governance_invitation_reply_in_candidate(issuer.common, withdrawn, withdrawing, "issuer", withdraw, permitted));
        check(validate_governance_invitation_replies(withdrawing).ok && withdrawn.replies.find(withdrawn_id)->grant_id == "grant",
              "explicit same original Grant and historical signer prove withdrawal");
        std::cout << checks << " checks; player result/World gates are separate\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
