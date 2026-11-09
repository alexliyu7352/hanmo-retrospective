// 原店单所有权纯读门：实际active绑定才认原预约，陌生ID/结束任务绝不由前缀放行。
#include "resident/domains/tasks/shop_orders/task_shop_order_sources.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
int main(){try{
 int checks=0;auto need=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);};
 TaskState tasks;TaskShopOrderState orders;TaskRecord task;task.id="task:297";task.status="active";task.assignee="proprietor";tasks.tasks.insert(task.id,task);
 TaskShopOrderBinding binding;binding.fact_id="task_shop_order_accepted:299";binding.task_id=task.id;binding.reservation_id="task_shop_order_contract:298";binding.terms.proprietor_actor_id=task.assignee;orders.bindings.insert(binding.fact_id,binding);
 auto read=[&](){return task_shop_order_reservation_exists(tasks,orders,binding.reservation_id);};
 need(read(),"actual live original shop task claims its reservation");
 need(!task_shop_order_reservation_exists(tasks,orders,"task_shop_order_contract:300"),"same prefix without original binding rejected");
 need(!task_shop_order_reservation_exists(tasks,orders,"contract:298"),"different original identity rejected");
 need(!task_shop_order_reservation_exists(tasks,orders,""),"empty reservation rejected");
 for(const auto* status:{"completed","cancelled","settling","failed","accepted"}){tasks.tasks.find(task.id)->status=status;need(!read(),"only original active task owns unspent shop reservation");}
 tasks.tasks.find(task.id)->status="active";tasks.tasks.find(task.id)->settlement_id="settlement:300";need(!read(),"settled task cannot retain reservation");tasks.tasks.find(task.id)->settlement_id.clear();
 tasks.tasks.find(task.id)->assignee="other";need(!read(),"other actor cannot claim original proprietor contract");tasks.tasks.find(task.id)->assignee=task.assignee;
 tasks.tasks.find(task.id)->id="task:300";need(!read(),"row key alone cannot replace task identity");tasks.tasks.find(task.id)->id=task.id;
 tasks.tasks.erase(task.id);need(!read(),"orphan original binding rejected");
 std::cout<<checks<<" original shop reservation reader checks, 0 failures; no World created\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
