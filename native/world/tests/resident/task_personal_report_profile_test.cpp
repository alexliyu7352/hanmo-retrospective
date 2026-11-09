// 新handoff4严格独立来源配对；缺一依赖不降成旧报告，旧1..3不会获得私人返报许可。
#include "resident/domains/tasks/personal_reports/task_personal_report_profile.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_profile.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void need(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}}
int main(){try{using Modules=std::vector<std::pair<StableId,StableId>>;Modules v{{task_personal_report_module,"1"},{"task_personal_message_deliveries","1"},{"task_message_handoffs","4"},{"task_static_assets","1"},{"family_life","1"},{"known_personal_dialogues","1"}};auto selected=task_personal_report_selected_version(v);need(selected.ok&&selected.value==1,"explicit report and original six-way source pair");need(task_personal_message_selected_version(v).ok,"handoff4 preserves original delivery source pair");{auto missing=v;missing.erase(missing.begin());need(!task_personal_message_selected_version(missing).ok,"handoff4 without report source never permits empty-role report");}{auto mismatch=v;mismatch[2].second="3";need(!task_personal_message_selected_version(mismatch).ok,"old handoff3 rejects new report source");}need(task_personal_report_selected_version({}).ok&&task_personal_report_selected_version({}).value==0,"old unselected profile stays empty");
 for(const auto* version:{"1","2","3"}){auto old=task_personal_report_selected_version({{"task_message_handoffs",version}});need(old.ok&&old.value==0,"old handoff never activates report sidecar");}
 for(std::size_t i=0;i<v.size();++i){auto broken=v;broken.erase(broken.begin()+i);need(!task_personal_report_selected_version(broken).ok,"missing original dependency rejects");}
 for(const auto* excluded:{"task_message_notices","task_message_returns"}){auto broken=v;broken.push_back({excluded,"1"});need(!task_personal_report_selected_version(broken).ok,"unclosed notice or return combinations reject");}
 {auto bad=v;bad[0].second="2";need(!task_personal_report_selected_version(bad).ok,"unknown new version rejects");}{auto bad=v;bad.push_back(v.front());need(!task_personal_report_selected_version(bad).ok,"duplicate identity rejects");}
 using V=BoundaryValue;auto request=boundary::obj({{"task_id",V{StableId{"task:7"}}}});need(decode_task_personal_report(task_personal_report_command,request).ok,"only original task id is request input");request.as<V::Dict>()->push_back({"reward",V{Integer{1}}});need(!decode_task_personal_report(task_personal_report_command,request).ok,"caller cannot choose original reward");
 std::cout<<checks<<" private sender profile and request cases; full World deferred\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
