// 原双方在本地读取自己亲历邀请，有限页只给原生报价，不发现远方新责任。
#include "political_governance_invitation_replies.hpp"
#include "resident/domains/politics/politics_internal.hpp"
#include "resident/storage/common_codec.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
// 完整候选身份和本人/地点一起防止旧控件跨读档、换人或修订继续操作。
V identity(const WorldIdentity& value){StableId text;constexpr char hex[]="0123456789abcdef";for(auto b:value.world_id){text+=hex[b>>4];text+=hex[b&15];}return boundary::obj({{"world_id",V{text}},{"execution_epoch",V{value.execution_epoch}},{"revision",V{value.revision}},{"tick",V{value.tick}}});}
}
// 仅显示已复核原双方和命令，回复本身不承诺新公库金额或重置Task。
BoundaryValue encode_governance_invitation_reply_quote(const GovernanceInvitationReplyQuote& quote){return boundary::obj({{"source_identity",identity(quote.identity)},{"actor_id",V{quote.source.actor_id}},{"counterpart_actor_id",V{quote.source.counterpart_actor_id}},{"authority_fact_id",V{quote.source.authority_fact_id}},{"site_id",V{quote.source.site_id}},{"duration",V{Integer{0}}},{"command_kind",V{governance_invitation_reply_command_kind(quote.command)}},{"params",encode_governance_invitation_reply_command(quote.command)}});}
// 历史当面回复对双方保留；尚未关闭的原邀请过期或已受领后不再给答复按钮。
Outcome<BoundaryValue> query_governance_invitation_replies(const GovernanceInvitationReplyContext& x,const StableId& place,Integer cursor){
 if(cursor<0||cursor>std::numeric_limits<Integer>::max()-16)return {false,{},common_failure("InvalidGovernanceInvitationCursor").error};
 const auto actor=x.common.state().metadata.player_actor_id;auto here=politics_detail::place(x.common,actor);
 if(!here||here->place_id!=place)return {false,{},common_failure("GovernanceInvitationLocalQueryRequired",place).error};
 auto valid=validate_governance_invitation_replies(x);if(!valid.ok)return {false,{},valid.error};V::Array rows;Integer count=0;
 for(const auto& saved:x.governance.authorities.rows()){
  const auto& a=saved.second;
  if(a.site_id!=place||(a.issuer_actor_id!=actor&&a.executor_actor_id!=actor)||a.issuer_actor_id==a.executor_actor_id)continue;
  const GovernanceInvitationReplyRecord* reply=nullptr;
  for(const auto& row:x.state.replies.rows())if(row.second.authority_fact_id==saved.first)reply=&row.second;
  // 原首长不是代理签发时的当面人；只有自己参与过的真实关闭见证才增添其知情。
  if(saved.first.rfind("politics_governance_agency_authority:",0)==0&&actor!=a.executor_actor_id&&!reply)continue;
  auto original=x.originals.governance_authority_approval(saved.first);if(!original.ok)return {false,{},original.error};
  auto accepted=x.tasks.governance_task_for_authority(saved.first);if(!accepted.ok)return {false,{},accepted.error};
  if(accepted.value||(!reply&&x.common.tick()>=a.offer_expires_tick))continue;
  if(count==std::numeric_limits<Integer>::max())return {false,{},common_failure("GovernanceInvitationPageOverflow").error};
  auto number=count++;if(number<cursor||number>=cursor+16)continue;
  V::Dict item{{"authority_fact_id",V{saved.first}},{"issuer_actor_id",V{a.issuer_actor_id}},{"executor_actor_id",V{a.executor_actor_id}},{"offer_expires_tick",V{a.offer_expires_tick}}};
  if(reply){item.emplace_back("reply_fact_id",V{reply->id});item.emplace_back("reply_kind",V{StableId{reply->kind==GovernanceInvitationReplyKind::declined?"declined":"withdrawn"}});item.emplace_back("reply_tick",V{reply->tick});}
  else{GovernanceInvitationReplyCommand command;
   if(actor==a.executor_actor_id)command=GovernanceDeclineInvitation{saved.first};
   else command=PoliticalWithdrawGovernanceInvitation{saved.first,{a.organization_id,a.office_id,a.authority_epoch,actor}};
   auto quote=preview_governance_invitation_reply(x,actor,command);item.emplace_back("command",V{governance_invitation_reply_command_kind(command)});item.emplace_back("params",encode_governance_invitation_reply_command(command));item.emplace_back("available",V{quote.ok});item.emplace_back("code",V{quote.ok?StableId{}:quote.error.original.code});
  }
  rows.emplace_back(std::move(item));
 }
 if(cursor>count)return {false,{},common_failure("InvalidGovernanceInvitationCursor").error};
 V::Dict page{{"ok",V{true}},{"enabled",V{count>0}},{"source_identity",identity(x.common.state().identity)},{"observer_actor_id",V{actor}},{"place_id",V{place}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(count>cursor+16)page.emplace_back("next_cursor",V{cursor+16});return {true,V{std::move(page)}, {}};
}
}
