// 客户预约仍是客户的钱；页面只列本人冻结责任与已发生两腿，不从今天余额推成交。
#include "task_shop_order_view.hpp"
#include "task_shop_order_estate_state.hpp"
#include "resident/views/common_presentation.hpp"
#include <limits>
namespace hanmo::resident {namespace {
using V=BoundaryValue;using A=V::Array;
constexpr Integer page_size=16;
V identity(const WorldIdentity& s){StableId id;constexpr char hex[]="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
V action(TaskShopOrderCommand command,const StableId& text,const StableId& summary,const Status& available){return boundary::obj({{"command",V{task_shop_order_command_kind(command)}},{"params",encode_task_shop_order_command(command)},{"text",V{text}},{"summary",V{summary}},{"available",V{available.ok}},{"code",V{available.ok?StableId{}:available.error.original.code}}});}
StableId item_name(const TaskCatalog& catalog,const StableId& id){const auto* text=catalog.presentation_names.find("items/"+id);return text?*text:id;}
StableId terms(const PrivateShopOrderTerms& t,const StableId& item){return item+" × "+std::to_string(t.quantity)+"；成交总价 "+std::to_string(t.contract_price)+" 钱。原店库存交给原客户，货款实入原店钱包；交付最迟半日 "+std::to_string(t.deadline_tick)+"。";}
Status status(const Outcome<TaskShopOrderDeliveryQuote>& q){return q.ok?common_success():Status{false,{},q.error};}
}
Outcome<BoundaryValue> query_task_shop_orders(const CommonStore& common,const TaskState& tasks,const TaskShopOrderState& state,const TaskShopOrderViewAccess& read,Integer cursor,const StableId& order_id){
    const auto fail=[](const StableId& code)->Outcome<V>{return {false,{},common_failure(code,"task_shop_orders").error};};const auto& observer=common.state().metadata.player_actor_id;
    if(!common.actor(observer))return fail("UnknownPlayer");if(cursor<0||cursor>std::numeric_limits<Integer>::max()-page_size)return fail("InvalidTaskShopOrderCursor");
    auto valid=validate_task_shop_order_execution(common,tasks,state,read.orders);if(!valid.ok)return {false,{},valid.error};CommonPresentation names(common.state(),read.common_catalog);A rows,offers;
    const auto mine=state.by_proprietor.find(observer);const std::size_t count=mine==state.by_proprietor.end()?0:mine->second.size();const auto end=std::min(count,static_cast<std::size_t>(cursor+page_size));
    for(std::size_t i=static_cast<std::size_t>(cursor);i<end;++i){
        // 最新本人受领先列，避免完成首屏的旧单把刚受领/刚交付的责任挤到遥远分页。
        const auto* b=state.bindings.find(mine->second[count-1-i]);const auto* task=b?tasks.tasks.find(b->task_id):nullptr;if(!b||!task||b->terms.proprietor_actor_id!=observer||task->assignee!=observer)return fail("InvalidTaskShopOrderViewIndex");
        auto client=names.display_name(b->terms.original_client.id),item=Outcome<StableId>{true,item_name(read.task_catalog,b->terms.item_id),{}};if(!client.ok)return {false,{},client.error};if(!item.ok)return {false,{},item.error};const auto summary=terms(b->terms,item.value);StableId state_text,progress,last_failure;Integer last_failure_tick=-1;A actions;
        if(task->status=="active"){
            const TaskShopOrderAction* work=nullptr;for(const auto& row:state.actions.rows())if(row.second.task_id==task->id)work=&row.second;
            state_text=work?"正在当面交货":"本人已受领";progress="客户全价 "+std::to_string(b->terms.contract_price)+" 钱仍在原钱包预约，尚未支付。";
            for(const auto& failed:state.interruptions.rows())if(failed.second.source.task_id==task->id){last_failure=failed.second.code;last_failure_tick=failed.second.tick;}
            if(!last_failure.empty())progress+=" 上次实际交付在半日 "+std::to_string(last_failure_tick)+" 中断，钱货尚未成交；仍须本人继续或取消。";
            if(work){const auto* original=common.action(work->action_id);if(!original)return fail("MissingTaskShopOrderAction");progress+=" 原交付还需 "+std::to_string(original->remaining_ticks)+" 个半日。";}
            else if(common.tick()>=b->terms.deadline_tick)progress+=" 已无足够期限完成交付，可本人取消未交订单。";
            auto delivery=preview_task_shop_order_delivery(common,tasks,state,read.orders,read.interaction,read.interaction_catalog,read.interaction_access,observer,task->id);if(!delivery.ok&&delivery.error.kind!=ErrorKind::rule)return {false,{},delivery.error};actions.push_back(action({TaskShopOrderCommand::Kind::deliver,task->id},"当面交货并收取货款",summary+" 实际交付需一个半日。",status(delivery)));
            auto cancel=preview_task_shop_order_cancellation(common,tasks,state,read.orders,observer,task->id);if(!cancel.ok&&cancel.error.kind!=ErrorKind::rule)return {false,{},cancel.error};actions.push_back(action({TaskShopOrderCommand::Kind::cancel,task->id},"取消未交订单","释放原客户尚未支付的全部预约；货物仍在原店，没有现金退款。",cancel));
        }else{
            const TaskShopOrderSettlement* receipt=nullptr;for(const auto& row:state.settlements.rows())if(row.second.task_id==task->id)receipt=&row.second;if(!receipt)return fail("MissingTaskShopOrderSettlement");
            state_text=receipt->outcome=="delivered"?"实际成交已结清":receipt->termination=="deadline_elapsed"?"原期限已过，未交订单已清算":receipt->termination==task_shop_order_estate_termination?"原授权执行人已取消未交订单":"本人已取消未交订单";progress="于半日 "+std::to_string(receipt->tick)+" 办妥。"+(receipt->outcome=="delivered"?"已交 "+std::to_string(receipt->delivered_quantity)+" 件，实际收取 "+std::to_string(receipt->paid_price)+" 钱。":"原客户预约已释放，未交货、未付款，现金退款为零。");
        }
        rows.push_back(boundary::obj({{"id",V{task->id}},{"order_id",V{b->order_id}},{"display_name",V{"客户："+client.value}},{"status",V{task->status}},{"status_text",V{state_text}},{"terms_text",V{summary}},{"progress_text",V{progress}},{"last_failure_code",V{last_failure}},{"actions",V{std::move(actions)}}}));
    }
    if(!order_id.empty()){
        auto quote=preview_task_shop_order_acceptance(common,tasks,state,read.task_catalog,read.common_catalog,read.orders,read.acceptance_meeting,observer,order_id);if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};
        StableId title="当前选中的待接店单",summary="须由本人在原客户真实声明处重新核对条款。";if(quote.ok){auto client=names.display_name(quote.value.terms.original_client.id),item=Outcome<StableId>{true,item_name(read.task_catalog,quote.value.terms.item_id),{}};if(!client.ok)return {false,{},client.error};if(!item.ok)return {false,{},item.error};title="客户："+client.value;summary=terms(quote.value.terms,item.value);}
        A actions;actions.push_back(action({TaskShopOrderCommand::Kind::accept,order_id},"本人受领这份店单",summary+" 接受只预约客户全价；成功交货才收款。",quote.ok?common_success():Status{false,{},quote.error}));offers.push_back(boundary::obj({{"id",V{order_id}},{"display_name",V{title}},{"status_text",V{quote.ok?"尚未受领":"当前无法受领"}},{"terms_text",V{summary}},{"actions",V{std::move(actions)}}}));
    }
    V::Dict page{{"ok",V{true}},{"source_identity",identity(common.state().identity)},{"observer_actor_id",V{observer}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}},{"offers",V{std::move(offers)}}};if(end<count)page.emplace_back("next_cursor",V{cursor+page_size});return {true,V{std::move(page)},{}};
}
}
