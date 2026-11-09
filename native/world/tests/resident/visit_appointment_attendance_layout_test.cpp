// 独立目录marker不冒持久payload；真实selected/inspect共享helper的最小反例门。
#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_layout.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
int main(){try{int checks=0;auto need=[&](bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);};
 const std::vector<std::pair<StableId,StableId>> old={{"visit_appointment_invitations","1"},{"visit_appointment_meetings","1"},{"personal_appointment_attendance","1"}};
 need(validate_visit_appointment_attendance_layout({},false).ok,"old disabled layout retained");need(validate_visit_appointment_attendance_layout(old,true).ok,"old enabled profiles without new marker retained");
 auto selected=old;selected.emplace_back("visit_appointment_attendance","1");need(validate_visit_appointment_attendance_layout(selected,true).ok,"new marker without any persistent section is legal");need(!validate_visit_appointment_attendance_layout(selected,true,true).ok,"invented persistent section rejected");need(!validate_visit_appointment_attendance_layout(selected,false).ok,"new catalog still needs actual dynamic profile");
 for(std::size_t at=0;at<old.size();++at){auto missing=selected;missing.erase(missing.begin()+at);need(!validate_visit_appointment_attendance_layout(missing,true).ok,"each original exact dependency required");auto duplicate=selected;duplicate.push_back(selected[at]);need(!validate_visit_appointment_attendance_layout(duplicate,true).ok,"duplicate dependency rejected");auto unknown=selected;unknown[at].second="2";need(!validate_visit_appointment_attendance_layout(unknown,true).ok,"unknown dependency version rejected");}
 auto duplicate=selected;duplicate.emplace_back("visit_appointment_attendance","1");need(!validate_visit_appointment_attendance_layout(duplicate,true).ok,"duplicate independent marker rejected");auto unknown=selected;unknown.back().second="2";need(!validate_visit_appointment_attendance_layout(unknown,true).ok,"unknown independent version rejected");
 std::cout<<checks<<" marker-only attendance layout checks, 0 failures; no World created\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
