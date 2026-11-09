// 约期、完成与取消都取原本人可知事实；不知道对方去哪，也不把远方作者政策当作现已答应。
#include "personal_appointment_view.hpp"
#include "personal_appointment_detail.hpp"
#include "personal_appointment_time2.hpp"
#include "resident/views/common_presentation.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;using A=V::Array;
V identity(const WorldIdentity& s){StableId id;constexpr char hex[]="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
V action(const StableId& kind,const StableId& field,const StableId& id,const StableId& text,const StableId& summary,const Outcome<PersonalAppointmentQuote>& quote){return boundary::obj({{"command",V{kind}},{"params",boundary::obj({{field,V{id}}})},{"text",V{text}},{"summary",V{summary}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});}
}
Outcome<BoundaryValue> query_personal_appointments(const CommonStore& common,const PersonalAppointmentViewAccess& read,Integer cursor){
    constexpr Integer limit=16;const auto& actor=common.state().metadata.player_actor_id;if(!common.actor(actor))return {false,{},common_failure("UnknownPlayer").error};if(cursor<0||cursor>std::numeric_limits<Integer>::max()-limit)return {false,{},common_failure("InvalidPersonalAppointmentCursor").error};
    auto valid=validate_personal_appointments(common,read.interaction,read.interaction_catalog,read.catalog,read.state,nullptr,read.exact);if(!valid.ok)return {false,{},valid.error};CommonPresentation names(common.state(),read.common_catalog);A all;
    for(auto at=read.state.accepted.order().rbegin();at!=read.state.accepted.order().rend();++at){const auto* a=read.state.accepted.find(*at);if(!a||(a->contact.actor_id!=actor&&a->contact.provider_actor_id!=actor))continue;const auto other=a->contact.actor_id==actor?a->contact.provider_actor_id:a->contact.actor_id;auto name=names.display_name(other);if(!name.ok)return {false,{},name.error};
        auto current=read.exact?read_personal_appointment_status2(*read.exact,a->fact_id):Outcome<StableId>{true,appointment_detail::status(read.state,*a,common.tick()),{}};if(!current.ok)return {false,{},current.error};const auto&status=current.value;StableId label=status=="accepted"?"已经当面约定":status=="meeting"?"正在会面":status=="fulfilled"?"已经按约会面":status=="cancelled"?"双方已当面取消":status=="missed"?"开始窗口已过":"会面中断";
        StableId progress="于半日 "+std::to_string(a->contact.tick)+" 当面约定，在半日 "+std::to_string(a->earliest_tick)+" 至 "+std::to_string(a->latest_tick)+" 之间开始，实际会面占用双方 "+std::to_string(a->duration_ticks)+" 个半日。";
        if(status=="accepted")progress+="到原地点后仍须本人在场并明确开始；这份约定不代替行程，也不授课程或付款。";
        if(status=="missed")progress+="原窗口未实际开始，没有自动收费或关系处罚；双方可再当面约定。";
        if(const auto* finished=appointment_detail::finish(read.state,a->fact_id)){progress+=" 原会面于半日 "+std::to_string(finished->tick)+" 结束。";if(!finished->success)progress+="只记本次实际中断，不推断对方后来的去向。";}
        A actions;if(a->contact.actor_id==actor&&status=="accepted"){
            auto meet=preview_personal_appointment_meet(common,read.interaction,read.interaction_catalog,read.interaction_access,read.catalog,read.state,actor,{a->fact_id},read.exact);if(!meet.ok&&meet.error.kind!=ErrorKind::rule)return {false,{},meet.error};actions.push_back(action(personal_appointment_action_kind,"appointment_id",a->fact_id,"按约开始会面","现在在原地点与原本人开始一个半日的会面；双方忙碌，期间照常推进世界。",meet));
            auto cancel=preview_personal_appointment_cancel(common,read.interaction,read.interaction_catalog,read.interaction_access,read.catalog,read.state,actor,{a->fact_id},read.exact);if(!cancel.ok&&cancel.error.kind!=ErrorKind::rule)return {false,{},cancel.error};actions.push_back(action("personal_appointment_cancel","appointment_id",a->fact_id,"当面取消这次约定","双方此刻当面同意撤销；不产生远程消息、费用或关系惩罚。",cancel));
        }
        all.push_back(boundary::obj({{"id",V{a->fact_id}},{"display_name",V{"与 "+name.value+" 的会面"}},{"status_text",V{label}},{"status",V{status}},{"progress_text",V{progress}},{"place_id",V{a->contact.place_id}},{"earliest_tick",V{a->earliest_tick}},{"latest_tick",V{a->latest_tick}},{"actions",V{std::move(actions)}}}));
    }
    // 新邀请只有当前实际provider说到该节点才可见；不会从全目录列出远方愿意相约的人。
    for(const auto& p:read.catalog.policies){auto source=preview_authored_provider_source(common,read.interaction,read.interaction_catalog,read.interaction_access,actor,p.provider_actor_id);if(!source.ok){if(source.error.kind!=ErrorKind::rule)return {false,{},source.error};continue;}const auto& s=source.value.meeting;if(s.context.role_id!=p.role_id||s.dialogue_id!=p.dialogue_id||s.node_id!=p.accept_node_id)continue;
        auto quote=preview_personal_appointment_accept(common,read.interaction,read.interaction_catalog,read.interaction_access,read.catalog,read.state,actor,{p.id},read.exact);if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};auto name=names.display_name(p.provider_actor_id);if(!name.ok)return {false,{},name.error};A actions;actions.push_back(action("personal_appointment_accept","policy_id",p.id,"接受这次会面约定","本人接受眼前对方提出的固定窗口；只在真正开始后占用一个半日，没有费用或额外服务。",quote));
        all.push_back(boundary::obj({{"id",V{"policy:"+p.id}},{"display_name",V{name.value+" 当前提出的会面"}},{"status_text",V{"当面待本人确认"}},{"status",V{"offered"}},{"progress_text",V{"约在从现在起 "+std::to_string(p.earliest_offset)+" 至 "+std::to_string(p.latest_offset)+" 个半日之间开始，实际会面一个半日。"}},{"place_id",V{s.context.place_id}},{"actions",V{std::move(actions)}}}));
    }
    const auto end=std::min(all.size(),static_cast<std::size_t>(cursor+limit));A rows;for(std::size_t i=static_cast<std::size_t>(cursor);i<end;++i)rows.push_back(std::move(all[i]));V::Dict page{{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{actor}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(end<all.size())page.emplace_back("next_cursor",V{cursor+limit});return {true,V{std::move(page)}, {}};
}
}
