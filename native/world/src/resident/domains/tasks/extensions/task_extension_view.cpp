// 页面不写期限；确认仍走原本人命令和当场重新报价。已结任务仅显示曾经真实获准的历史。
#include "task_extension_view.hpp"
#include "resident/views/common_presentation.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;using A=V::Array;constexpr Integer page_size=16;
V identity(const WorldIdentity& s){StableId id;constexpr char hex[]="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
struct KnownExtension {const TaskRecord* task;const TaskExtensionPolicy* policy;const TaskExtensionFact* fact;};
}
Outcome<BoundaryValue> query_task_extensions(const CommonStore& common,const TaskState& tasks,const TaskExtensionViewAccess& read,Integer cursor){
    const auto fail=[](const StableId& code)->Outcome<V>{return {false,{},common_failure(code,"task_extensions").error};};const auto& observer=common.state().metadata.player_actor_id;if(!common.actor(observer))return fail("UnknownPlayer");if(cursor<0||cursor>std::numeric_limits<Integer>::max()-page_size)return fail("InvalidTaskExtensionCursor");auto valid=validate_task_extensions(common,tasks,read.task_catalog,read.interaction_catalog,read.catalog,read.state,read.history);if(!valid.ok)return {false,{},valid.error};
    std::vector<KnownExtension> known;
    for(auto at=tasks.tasks.order().rbegin();at!=tasks.tasks.order().rend();++at){const auto* task=tasks.tasks.find(*at);if(!task||task->assignee!=observer)continue;const TaskExtensionPolicy* policy=nullptr;for(const auto& p:read.catalog.policies)if(p.offer_id==task->offer_id)policy=&p;if(!policy)continue;const auto indexed=read.state.by_task.find(task->id);const auto* fact=indexed==read.state.by_task.end()?nullptr:read.state.facts.find(indexed->second);
        if(!fact){if(task->status!="active"&&task->status!="ready")continue;const auto* offer=read.task_catalog.offers.find(task->offer_id);if(!offer)return fail("UnknownTaskExtensionOffer");auto declaration=preview_authored_provider_source(common,read.interaction,read.interaction_catalog,read.interaction_access,observer,offer->issuer.id);if(!declaration.ok){if(declaration.error.kind!=ErrorKind::rule)return {false,{},declaration.error};continue;}const auto& source=declaration.value.meeting;if(source.dialogue_id!=policy->dialogue_id||source.node_id!=policy->node_id||source.context.role_id!=offer->report_role)continue;}
        known.push_back({task,policy,fact});
    }
    CommonPresentation names(common.state(),read.common_catalog);A rows;const auto end=std::min(known.size(),static_cast<std::size_t>(cursor+page_size));for(std::size_t i=static_cast<std::size_t>(cursor);i<end;++i){const auto& row=known[i];const auto* offer=read.task_catalog.offers.find(row.task->offer_id);auto issuer=names.display_name(offer->issuer.id);if(!issuer.ok)return {false,{},issuer.error};A actions;StableId progress,status;
        if(row.fact){status="已获一次延期";progress="原委托人 "+issuer.value+" 于半日 "+std::to_string(row.fact->granted_tick)+" 当面同意：原截止 "+std::to_string(row.fact->original_deadline_tick)+"，现截止 "+std::to_string(row.fact->deadline_tick)+"。本金、报酬和交付对象沿原约定。";}
        else {status="当前当面延期声明";progress="原委托人 "+issuer.value+" 当前同意这份私约延长 "+std::to_string(row.policy->additional_ticks)+" 个半日；每份任务只可办理一次。";auto quote=preview_task_extension(common,tasks,read.task_catalog,read.interaction,read.interaction_catalog,read.interaction_access,read.catalog,read.state,read.history,observer,{row.task->id,row.policy->id});if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};StableId summary="本人申请采用原委托人的这次有限延期；不支付额外费用、不改变原钱货和受益人。";if(quote.ok)summary+=" 截止由半日 "+std::to_string(quote.value.original_deadline_tick)+" 改为 "+std::to_string(quote.value.deadline_tick)+"。";actions.push_back(boundary::obj({{"command",V{StableId{task_extension_command_kind}}},{"params",encode_task_extension_command({row.task->id,row.policy->id})},{"text",V{"采用这次当面延期"}},{"summary",V{summary}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}}));}
        rows.push_back(boundary::obj({{"id",V{row.task->id}},{"display_name",V{offer->display_name}},{"status_text",V{status}},{"progress_text",V{progress}},{"deadline_tick",V{row.task->deadline_tick}},{"actions",V{std::move(actions)}}}));
    }
    V::Dict page{{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{observer}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(end<known.size())page.emplace_back("next_cursor",V{cursor+page_size});return {true,V{std::move(page)},{}};
}
}
