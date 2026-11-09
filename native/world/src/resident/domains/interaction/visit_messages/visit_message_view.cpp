// 远方领取不反向通知寄件人；任何可执行按钮还要经过同一真实会话的原业务预览。
#include "visit_message_view.hpp"
#include "visit_message_life.hpp"
#include "resident/views/common_presentation.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;using A=V::Array;constexpr Integer page_size=16;
// 索引与事实已由领域冷验证核过，这里只在本人已知行内找终结来源。
const VisitMessageTerminal* terminal(const OrderedTable<VisitMessageTerminal>& rows,const StableId& id){for(const auto& row:rows.rows())if(row.second.message_fact_id==id)return &row.second;return nullptr;}
const VisitMessagePolicy* policy(const VisitMessageCatalog& catalog,const StableId& id){for(const auto& row:catalog.policies)if(row.id==id)return &row;return nullptr;}
V identity(const WorldIdentity& source){StableId id;constexpr char hex[]="0123456789abcdef";for(auto byte:source.world_id){id+=hex[byte>>4];id+=hex[byte&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{source.execution_epoch}},{"revision",V{source.revision}},{"tick",V{source.tick}}});}
}
Outcome<BoundaryValue> query_visit_messages(const CommonStore& common,const VisitMessageState& state,const VisitMessageCatalog& catalog,
    const InteractionState& interactions,const InteractionCatalog& definitions,const InteractionAccess& access,Integer cursor,Integer offer_cursor,const VisitMessageLifeContext*life){
    const auto failed=[](const StableId& code)->Outcome<V>{return {false,{},common_failure(code,"visit_messages").error};};
    if(cursor<0||offer_cursor<0||cursor>std::numeric_limits<Integer>::max()-page_size||offer_cursor>std::numeric_limits<Integer>::max()-page_size)return failed("InvalidVisitMessageCursor");
    auto valid=validate_visit_messages(common,state,catalog,definitions,life?&life->clock:nullptr);if(!valid.ok)return {false,{},valid.error};if(state.version>=2){if(!life||&life->common!=&common||&life->state!=&state||&life->policies!=&catalog||&life->interactions!=&interactions)return {false,{},common_failure("VisitMessageLifeQuerySourcesRequired",{},ErrorKind::unsupported_capability).error};valid=validate_visit_message_life_sources(*life);if(!valid.ok)return {false,{},valid.error};}
    const auto actor=common.state().metadata.player_actor_id;if(!common.actor(actor))return failed("UnknownPlayer");
    CommonPresentation names(common.state(),*definitions.common);const InteractionSession* meeting=nullptr;
    for(const auto& row:interactions.interactions)if(row.actor_id==actor){if(meeting)return failed("DuplicateInteraction");meeting=&row;}
    std::optional<RuleError>technical;
    // 一份明确的provider声明只支撑当前节点，不从岗位或作者表推测远方接待。
    const auto at=[&](const VisitMessagePolicy& p,const StableId& node){
        if(!meeting||meeting->phase!="say"||meeting->node_id!=node||meeting->dialogue_id!=p.dialogue_id||meeting->context.role_id!=p.role_id||meeting->context.provider_actor_id!=p.custodian_actor_id)return false;
        auto source=preview_authored_provider_source(common,interactions,definitions,access,actor,p.custodian_actor_id);
        if(!source.ok&&source.error.kind!=ErrorKind::rule)technical=source.error;return source.ok&&!source.value.meeting.context.kind&&!source.value.meeting.context.encounter_id;
    };
    const auto action=[&](const VisitMessageCommand& command,const StableId& text){auto quote=preview_visit_message(common,state,catalog,interactions,definitions,access,actor,command,life);if(!quote.ok&&quote.error.kind!=ErrorKind::rule)technical=quote.error;return boundary::obj({{"command",V{visit_message_command_kind(command)}},{"params",encode_visit_message_command(command)},{"text",V{text}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});};
    std::vector<const VisitMessagePosted*> visible;
    for(const auto& row:state.posted.rows()){
        const auto& message=row.second;
        if(message.sender_actor_id==actor){visible.push_back(&message);continue;}
        if(message.recipient_actor_id!=actor)continue;
        const auto* p=policy(catalog,message.policy_id);
        if(terminal(state.received,row.first)||(!terminal(state.withdrawn,row.first)&&p&&at(*p,p->receive_node_id)))visible.push_back(&message);
    }
    if(technical)return {false,{},*technical};if(static_cast<std::size_t>(cursor)>visible.size())return failed("InvalidVisitMessageCursor");
    A rows,offers;const auto end=std::min(visible.size(),static_cast<std::size_t>(cursor+page_size));
    for(auto offset=static_cast<std::size_t>(cursor);offset<end;++offset){const auto& message=*visible[offset];const auto* p=policy(catalog,message.policy_id);if(!p)return failed("MissingVisitMessagePolicy");
        const bool sender=message.sender_actor_id==actor;auto other=names.display_name(sender?message.recipient_actor_id:message.sender_actor_id);if(!other.ok)return {false,{},other.error};
        A actions;StableId status,text;Integer known_tick=message.contact.tick;
        if(sender){const auto* withdrawn=terminal(state.withdrawn,message.fact_id);status=withdrawn?"withdrawn":"deposited";text=withdrawn?"本人已当面撤回。":"本人已托留；尚无由此记录取得的远方领读消息。";if(withdrawn)known_tick=withdrawn->contact.tick;else if(at(*p,p->withdraw_node_id))actions.push_back(action(WithdrawVisitMessage{message.fact_id},"当面撤回这封拜帖"));}
        else if(const auto* received=terminal(state.received,message.fact_id)){status="received";known_tick=received->contact.tick;text="本人已领读：对方曾来访，希望另行会面。此帖没有约定日期，也不表示本人答应会面。";}
        else {status="awaiting_receipt";text="接待人当面告知有一封留给本人的拜帖；领取后才列为本人已读。";actions.push_back(action(ReceiveVisitMessage{message.fact_id},"本人领取并读这封拜帖"));}
        rows.push_back(boundary::obj({{"id",V{message.fact_id}},{"display_name",V{sender?"留给 "+other.value:"来自 "+other.value}},{"status",V{status}},{"text",V{text}},{"known_tick",V{known_tick}},{"actions",V{std::move(actions)}}}));
    }
    std::vector<const VisitMessagePolicy*> offered;for(const auto& p:catalog.policies)if(p.sender_actor_id==actor&&at(p,p.deposit_node_id))offered.push_back(&p);
    if(technical)return {false,{},*technical};if(static_cast<std::size_t>(offer_cursor)>offered.size())return failed("InvalidVisitMessageCursor");const auto offer_end=std::min(offered.size(),static_cast<std::size_t>(offer_cursor+page_size));
    for(auto offset=static_cast<std::size_t>(offer_cursor);offset<offer_end;++offset){const auto& p=*offered[offset];auto name=names.display_name(p.recipient_actor_id);if(!name.ok)return {false,{},name.error};offers.push_back(boundary::obj({{"id",V{p.id}},{"display_name",V{"给 "+name.value+" 留拜帖"}},{"text",V{StableId{"只托留本人来访与另约会面的意向；不传递其他消息，不收费，也不代替收件人同意预约。"}}},{"actions",V{A{action(LeaveVisitMessage{p.id},"确认当面托留")}}}}));}
    if(technical)return {false,{},*technical};V::Dict page{{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{actor}},{"cursor",V{cursor}},{"offer_cursor",V{offer_cursor}},{"rows",V{std::move(rows)}},{"offers",V{std::move(offers)}}};
    if(end<visible.size())page.emplace_back("next_cursor",V{static_cast<Integer>(end)});if(offer_end<offered.size())page.emplace_back("next_offer_cursor",V{static_cast<Integer>(offer_end)});return {true,V{std::move(page)},{}};
}
}
