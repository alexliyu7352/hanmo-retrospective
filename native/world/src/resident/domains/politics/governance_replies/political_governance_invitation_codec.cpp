// 未启新段不写空尾；新段逐字段保存唯一关闭来源与真正玩家提交绑定。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"

namespace hanmo::resident::dynamic_economy_codec {
// 关闭类型是两个显式词，未知枚举不能降级成谢绝。
template<> struct EnumNames<GovernanceInvitationReplyKind> {
    static std::vector<std::pair<GovernanceInvitationReplyKind, StableId>> rows() {
        return {{GovernanceInvitationReplyKind::declined, "declined"}, {GovernanceInvitationReplyKind::withdrawn, "withdrawn"}};
    }
};
// 真实结果标识由Common提交尾提供，不使用事实相邻编号推测。
template<class A> void record(A& a, GovernanceInvitationReplyPlayerReceipt& v) {
    a("result_id", v.result_id); a("actor_id", v.actor_id);
    a("request_sequence", v.request_sequence); a("result_revision", v.result_revision);
}
// 原政治批准始终只作引用，资金和已受领合同不进入回复账。
template<class A> void record(A& a, GovernanceInvitationReplyRecord& v) {
    a("id", v.id); a("authority_fact_id", v.authority_fact_id); a("actor_id", v.actor_id);
    a("counterpart_actor_id", v.counterpart_actor_id); a("site_id", v.site_id); a("grant_id", v.grant_id);
    a("kind", v.kind); a("tick", v.tick); a("player_receipt", v.player_receipt);
}
// 同一访问表用于JSON和二进制，旧治理段保持原布局。
template<class A> void record(A& a, GovernanceInvitationReplyState& v) { a("version", v.version); a("replies", v.replies); }
// 本人只选择原邀请，不能自带对方、地点或回复时间。
template<class A> void record(A& a, GovernanceDeclineInvitation& v) { a("authority_fact_id", v.authority_fact_id); }
// 撤回必须提交原本人当前署权，不能借省略authority走谢绝路径。
template<class A> void record(A& a, PoliticalWithdrawGovernanceInvitation& v) { a("authority_fact_id", v.authority_fact_id); a("authority", v.authority); }
}

namespace hanmo::resident {
// 这里只处理封闭格式，原批准、生命与未接受来源由完整业务冷验逐项确认。
// 只借原record字段表输出；版本与来源准入仍由原成对codec及业务校验负责。
Status emit_governance_invitation_replies_fields(ProjectionWriter& writer,const GovernanceInvitationReplyState& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& error){return writer.reject(error.error.original.code,error.error.original.path);}
}
BoundaryValue encode_governance_invitation_replies(const GovernanceInvitationReplyState& value){return politics_projection_detail::encode_fields(value,emit_governance_invitation_replies_fields);}
Outcome<GovernanceInvitationReplyState> decode_governance_invitation_replies(const BoundaryValue& value) {
    auto unique = validate_political_takeover_payload(value); if (!unique.ok) return {false, {}, unique.error};
    try {
        GovernanceInvitationReplyState out; dynamic_economy_codec::decode(value, out, "/political_governance_invitation_replies");
        if (out.version != 1 || out.replies.size() > 65536) return {false, {}, common_failure("InvalidGovernanceInvitationReplies").error};
        return {true, std::move(out), {}};
    } catch (const boundary::DecodeFailure& e) { return {false, {}, e.error}; }
}
// 独立版本段只接受1，未知版本不按旧治理默认读取。
Status visit_governance_invitation_replies_checkpoint(CodecVisitor& visitor, GovernanceInvitationReplyState& state) {
    if (!visitor.reading() && state.version != 1) return visitor.reject("UnsupportedGovernanceInvitationRepliesVersion");
    auto result = dynamic_economy_codec::checkpoint(visitor, governance_invitation_reply_module, state);
    if (!result.ok) return result;
    return state.version == 1 && state.replies.size() <= 65536 ? common_success() : visitor.reject("InvalidGovernanceInvitationReplies");
}
// 两个命令共享一份邀请回复状态，但保留独立当前权源。
StableId governance_invitation_reply_command_kind(const GovernanceInvitationReplyCommand& command) {
    return std::holds_alternative<GovernanceDeclineInvitation>(command) ? governance_decline_command : governance_withdraw_command;
}
BoundaryValue encode_governance_invitation_reply_command(const GovernanceInvitationReplyCommand& command) {
    return std::visit([](const auto& value) { return dynamic_economy_codec::encode(value); }, command);
}
// 逐实际命令解码，额外actor、budget或payload字段被原严格访问器拒绝。
Outcome<GovernanceInvitationReplyCommand> decode_governance_invitation_reply_command(const StableId& kind, const BoundaryValue& value) {
    auto unique = validate_political_takeover_payload(value); if (!unique.ok) return {false, {}, unique.error};
    try {
        GovernanceInvitationReplyCommand out;
        if (kind == governance_decline_command) {
            GovernanceDeclineInvitation command; dynamic_economy_codec::decode(value, command, kind); out = std::move(command);
        } else if (kind == governance_withdraw_command) {
            PoliticalWithdrawGovernanceInvitation command; dynamic_economy_codec::decode(value, command, kind);
            if (command.authority.organization_id.empty() || command.authority.office_id.empty() || command.authority.actor_id.empty() || command.authority.authority_epoch < 0)
                return {false, {}, common_failure("InvalidGovernanceInvitationCommand").error};
            out = std::move(command);
        } else return {false, {}, common_failure("UnknownGovernanceInvitationCommand", kind).error};
        const auto id = std::visit([](const auto& command) { return command.authority_fact_id; }, out);
        if (id.empty() || id.size() > 4096) return {false, {}, common_failure("InvalidGovernanceInvitationCommand").error};
        return {true, std::move(out), {}};
    } catch (const boundary::DecodeFailure& e) { return {false, {}, e.error}; }
}
}
