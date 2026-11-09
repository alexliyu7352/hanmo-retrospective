// 本人原已收信件在政治真实就任前的只读截面；不新增通知、死亡或政治授权账。
#pragma once
#include "task_message_notice.hpp"
#include <charconv>
namespace hanmo::resident {
namespace task_death_notice_history_detail {
// 仅两种真实政治零时就任边界，调用者不能以任意prefix/kind/domain扩展知情许可。
enum class AccessionBoundary { original, successor_round };
// 调用方先验证本域实际accession载荷及其tick；read须为同一World的原已收文书读。
// 空表示指定本人当时尚未收到这份精确信；未知坏来源报错，不能改选同死者另一封。
inline Outcome<std::optional<DeliveredTaskDeathNotice>> read_before_accession(
    const CommonStore& common,const TaskDeliveredDeathNoticeRead& read,const StableId& observer,
    const StableId& binding_fact_id,Integer tick,const StableId& accession_fact_id,AccessionBoundary scope){
    using Result=Outcome<std::optional<DeliveredTaskDeathNotice>>;
    auto fail=[&](const StableId& code)->Result{return {false,{},common_failure(code,accession_fact_id).error};};
    auto instance=[&](const StableId& id,const StableId& prefix)->std::optional<Integer>{const auto start=prefix+":";if(id.compare(0,start.size(),start)!=0)return {};const auto text=id.substr(start.size());Integer n=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);if(text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||n<1||n>=common.state().next_instance_id||std::to_string(n)!=text)return {};return n;};
    const bool round=scope==AccessionBoundary::successor_round;
    const StableId prefix=round?"political_successor_round_accession":"political_succession_accession",kind=round?"political_successor_round_acceded":"political_succession_acceded",domain=round?"political_successor_rounds":"political_successions";
    const auto* boundary=common.state().facts.find(accession_fact_id);const auto cutoff=instance(accession_fact_id,prefix);const auto& order=common.state().facts.order();const auto where=std::find(order.begin(),order.end(),accession_fact_id);
    if(!boundary||boundary->id!=accession_fact_id||boundary->kind!=kind||boundary->domain!=domain||!cutoff||where==order.end()||tick<0||tick>common.tick())return fail("TaskDeathNoticeAccessionBoundaryRequired");
    if(!read)return fail("TaskDeathNoticeAccessionReadRequired");
    auto known=read(observer,binding_fact_id);if(!known.ok||!known.value)return known;const auto& n=*known.value;const auto& identity=common.state().identity;
    if(n.identity.world_id!=identity.world_id||n.identity.execution_epoch!=identity.execution_epoch||n.identity.revision!=identity.revision||n.identity.tick!=identity.tick||n.scenario_id!=common.state().metadata.scenario_id||n.binding_fact_id!=binding_fact_id||n.recipient_actor_id!=observer||n.task_id.empty()||n.source_lot_id.empty()||n.death_fact_id.empty()||n.decedent_actor_id.empty()||n.sender_actor_id.empty()||n.issued_tick<0||n.received_tick<n.issued_tick||n.received_tick>common.tick())return fail("TaskDeathNoticeAccessionSourceMismatch");
    const auto bound=instance(n.binding_fact_id,"task_death_notice_bound"),handoff=instance(n.handoff_fact_id,"task_message_handoff");const auto* b=common.state().facts.find(n.binding_fact_id);const auto* h=common.state().facts.find(n.handoff_fact_id);const auto bi=std::find(order.begin(),order.end(),n.binding_fact_id),hi=std::find(order.begin(),order.end(),n.handoff_fact_id);
    if(!bound||!handoff||!b||!h||b->id!=n.binding_fact_id||b->kind!=task_death_notice_bound_kind||b->domain!="tasks"||h->id!=n.handoff_fact_id||h->kind!=task_message_handoff_kind||h->domain!="tasks"||bi==order.end()||hi==order.end()||*bound>=*handoff||bi>=hi)return fail("TaskDeathNoticeAccessionSourceOrderMismatch");
    const bool bound_before=*bound<*cutoff,handoff_before=*handoff<*cutoff;
    if(*bound==*cutoff||*handoff==*cutoff||bound_before!=(bi<where)||handoff_before!=(hi<where)||(bound_before?n.issued_tick>tick:n.issued_tick<tick)||(handoff_before?n.received_tick>tick:n.received_tick<tick))return fail("TaskDeathNoticeAccessionSourceOrderMismatch");
    if(!bound_before||!handoff_before)return {true,std::nullopt,{}};
    return known;
}
} // 具名内部截面，不增加死亡通知或政治状态。
// 原首轮就任口保持原exact头和原错误语义，后继新轮头不得混入。
inline Outcome<std::optional<DeliveredTaskDeathNotice>> read_task_death_notice_before_political_accession(
    const CommonStore& common,const TaskDeliveredDeathNoticeRead& read,const StableId& observer,
    const StableId& binding_fact_id,Integer tick,const StableId& accession_fact_id){
    return task_death_notice_history_detail::read_before_accession(common,read,observer,binding_fact_id,tick,accession_fact_id,task_death_notice_history_detail::AccessionBoundary::original);
}
// 新轮只接受实际successor-round就任；原信与收讫必须同时早于该精确事实。
inline Outcome<std::optional<DeliveredTaskDeathNotice>> read_task_death_notice_before_political_successor_round_accession(
    const CommonStore& common,const TaskDeliveredDeathNoticeRead& read,const StableId& observer,
    const StableId& binding_fact_id,Integer tick,const StableId& accession_fact_id){
    return task_death_notice_history_detail::read_before_accession(common,read,observer,binding_fact_id,tick,accession_fact_id,task_death_notice_history_detail::AccessionBoundary::successor_round);
}
}
