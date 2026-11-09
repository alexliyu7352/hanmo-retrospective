// 真marker集合、独立段和原依赖同开；不加载World、不把目录来源门当回书行为通过。
#include "resident/domains/tasks/message_returns/task_message_return_profile.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}}
int main(){try{
 std::vector<std::pair<StableId,StableId>> modules{{"task_message_handoffs","2"},{"task_static_assets","1"},{"family_life","1"},{task_message_return_module,"1"}};
 auto v=task_message_return_selected_version({});need(v.ok&&v.value==0,"old empty module set stays unselected");v=task_message_return_selected_version(modules);need(v.ok&&v.value==1,"explicit return and exact original sources selected");
 for(std::size_t n=0;n<3;++n){auto bad=modules;bad.erase(bad.begin()+n);need(!task_message_return_selected_version(bad).ok,"each original source required");}
 auto bad=modules;bad[0].second="1";need(!task_message_return_selected_version(bad).ok,"handoff-only history cannot replace actual report source");bad=modules;bad.push_back(bad.back());need(!task_message_return_selected_version(bad).ok,"duplicate marker rejects");bad=modules;bad.back().second="2";need(!task_message_return_selected_version(bad).ok,"unknown return version rejects");
 using V=BoundaryValue;auto command=decode_task_message_return_collect("task_message_return_collect",boundary::obj({{"task_id",V{StableId{"task:4"}}}}));need(command.ok&&command.value.task_id=="task:4","exact collection command");need(!decode_task_message_return_collect("task_message_return_collect",boundary::obj({{"task_id",V{StableId{"task:4"}}},{"provider",V{StableId{"forged"}}}})).ok,"caller cannot replace original recipient");need(!decode_task_message_return_collect("task_message_return_collect",boundary::obj({{"task_id",V{StableId{"task:4"}}},{"task_id",V{StableId{"task:9"}}}})).ok,"duplicate task selector rejects");
 std::cout<<checks<<" return profile checks; behavior deferred\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
