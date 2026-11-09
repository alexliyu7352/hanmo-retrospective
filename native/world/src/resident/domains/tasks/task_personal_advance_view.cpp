// 查询只显示当前玩家原合同；已付本金与未偿本金按真实事实计算，不读取他人的私人余额。
#include "task_personal_advance_view.hpp"
#include "task_personal_advance_rules.hpp"
#include <limits>
namespace hanmo::resident {
Outcome<BoundaryValue> query_task_personal_advances(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const TaskPersonalAdvanceState& state,Integer cursor){
    using V=BoundaryValue;const auto& actor_id=common.state().metadata.player_actor_id;const auto* actor=common.actor(actor_id);if(!actor)return {false,{},common_failure("UnknownPlayer").error};
    std::vector<StableId> ids;for(const auto& row:state.terms.rows())if(row.second.lender_actor_id==actor_id)ids.push_back(row.first);
    if(cursor<0||static_cast<std::size_t>(cursor)>ids.size())return {false,{},common_failure("InvalidTaskAdvanceCursor").error};
    V::Array rows;Integer index=cursor;for(;static_cast<std::size_t>(index)<ids.size()&&rows.size()<16;++index){
        const auto& id=ids[static_cast<std::size_t>(index)];const auto* terms=state.terms.find(id);const auto* task=tasks.tasks.find(terms->task_id);if(!task||task->assignee!=actor_id)return {false,{},common_failure("TaskAdvanceSourceMismatch",id).error};
        auto summary=read_task_personal_advance_summary(common,tasks,state,id);if(!summary.ok)return {false,{},summary.error};
        const auto* contract=tasks.contracts.find(id);auto available=preview_task_personal_advance(common,tasks,catalog,state,actor_id,task->id,1);Integer maximum=0;
        if(available.ok){auto funds=common.spendable(terms->personal_wallet_id);if(!funds.ok)return {false,{},funds.error};const auto* escrow=common.wallet(terms->escrow_wallet_id);maximum=std::min({terms->policy.maximum_cash-summary.value.paid,funds.value,std::numeric_limits<Integer>::max()-escrow->balance});}
        const StableId status=summary.value.repayment_fact_id?"repaid":contract->execution_stopped&&summary.value.paid?"awaiting_repayment":summary.value.paid?"advanced":"authorized";
        V::Dict row{{"task_id",V{task->id}},{"contract_id",V{id}},{"display_name",V{contract->frozen_terms.definition.display_name}},{"status",V{status}},{"paid",V{summary.value.paid}},{"repaid",V{summary.value.repaid}},{"principal_due",V{summary.value.paid-summary.value.repaid}},{"authorized_maximum",V{terms->policy.maximum_cash}},{"available_maximum",V{maximum}},{"payment_fact_ids",boundary::string_array(summary.value.payment_fact_ids)},{"can_advance",V{available.ok}},{"blocked_code",V{available.ok?StableId{}:available.error.original.code}},{"command",V{StableId{"task_personal_advance"}}},{"amount_required",V{true}},{"explanation",V{StableId{"垫款出自本人私钱，限于原合同授权；清算先还本金，再结原报酬和余款。资金不足或原主体待合法接管时，偿还责任继续保留。"}}}};
        if(summary.value.repayment_fact_id)row.emplace_back("repayment_fact_id",V{*summary.value.repayment_fact_id});rows.emplace_back(V{std::move(row)});
    }
    StableId world_id;const char* hex="0123456789abcdef";for(auto byte:common.state().identity.world_id){world_id.push_back(hex[byte>>4]);world_id.push_back(hex[byte&15]);}
    V::Dict out{{"ok",V{true}},{"observer_actor_id",V{actor_id}},{"source_identity",boundary::obj({{"world_id",V{world_id}},{"execution_epoch",V{common.state().identity.execution_epoch}},{"revision",V{common.revision()}},{"tick",V{common.tick()}}})},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};
    if(static_cast<std::size_t>(index)<ids.size())out.emplace_back("next_cursor",V{index});return {true,V{std::move(out)}, {}};
}
}
