// 只复用原半日/双序数学；离岗返报自己的来源不与Q01交付结果表混用。
#pragma once
#include "task_personal_report_contract.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_detail.hpp"
namespace hanmo::resident::personal_report_detail {
using personal_message_detail::serial;using personal_message_detail::before;using personal_message_detail::identity;using personal_message_detail::narrative;using personal_message_detail::context;
inline Outcome<Integer> through(const TaskPersonalReportSources& x){return personal_message_detail::through(x.delivery.common,x.delivery.phase,x.delivery.exact_clock);}
inline Integer reward(const TaskDefinition& definition,const TaskRecord& task,Integer started){return task_finish_within(started,task_handover_ticks,task.deadline_tick)?definition.reward:definition.reward/2;}
inline const TaskPersonalReportStart* start_for_action(const TaskPersonalReportState& state,const StableId& action){const TaskPersonalReportStart* found=nullptr;for(const auto& row:state.starts.rows())if(row.second.action_id==action){if(found)return nullptr;found=&row.second;}return found;}
inline const TaskPersonalReportEnd* end_for_start(const TaskPersonalReportState& state,const StableId& start){const TaskPersonalReportEnd* found=nullptr;for(const auto& row:state.endings.rows())if(row.second.start_fact_id==start){if(found)return nullptr;found=&row.second;}return found;}
inline Integer reserved_reward(const CommonStore& c,const StableId& wallet,const StableId& action){const auto* w=c.wallet(wallet);Integer value=-1;if(w)for(const auto& r:w->reserved)if(r.action_id==action){if(value!=-1)return -1;value=r.amount;}return value;}
}
