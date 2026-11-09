// 使用真实版本与效果选择口；本小门不构造继位、Office或玩家历史。
#include "resident/domains/politics/successor_rounds/political_successor_round_profile.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}}
int main(){try{
 std::vector<std::pair<StableId,StableId>> modules={{"politics","1"},{"family_life","1"},{"task_message_notices","1"},{"task_message_handoffs","2"},{"political_successions","1"}};
 auto old=political_successor_round_selected_version(modules);check(old.ok&&old.value==0,"old original modules do not enable new round");
 modules.emplace_back(political_successor_rounds_domain,"1");auto selected=political_successor_round_selected_version(modules);check(selected.ok&&selected.value==1,"complete explicit new module admitted");
 auto bad=modules;bad.push_back(modules.back());check(!political_successor_round_selected_version(bad).ok,"duplicate refused");bad=modules;bad.back().second="2";check(!political_successor_round_selected_version(bad).ok,"unknown refused");
 for(std::size_t i=0;i+1<modules.size();++i){bad=modules;bad.erase(bad.begin()+i);check(!political_successor_round_selected_version(bad).ok,"every original source dependency required");}
 struct Shape{const char* command;const char* source;bool office;};
 for(const auto& row:std::vector<Shape>{{political_successor_round_nominate_command,"political_successor_round_nomination:7",false},{political_successor_round_answer_command,"political_successor_round_acceptance:7",false},{political_successor_round_answer_command,"political_successor_round_decline:7",false},{political_successor_round_withdraw_command,"political_successor_round_withdrawal:7",false},{political_successor_round_accede_command,"political_successor_round_accession:7",true}}){
  PoliticsEffects e;e.fact_ids={row.source};if(row.office)e.receipt_ids={"organization:8"};check(valid_political_successor_round_effect(row.command,e),"exact named source selected");auto wrong=e;wrong.fact_ids.push_back(row.source);check(!valid_political_successor_round_effect(row.command,wrong),"two sources refused");wrong=e;wrong.appointment_id="organization:8";check(!valid_political_successor_round_effect(row.command,wrong),"old assignment cannot impersonate round");wrong=e;if(row.office)wrong.receipt_ids.clear();else wrong.receipt_ids={"organization:8"};check(!valid_political_successor_round_effect(row.command,wrong),"only accession may change Office");wrong=e;wrong.fact_ids={StableId{row.source}.substr(0,StableId{row.source}.rfind(':')+1)+"07"};check(!valid_political_successor_round_effect(row.command,wrong),"noncanonical source refused");check(!valid_political_successor_round_effect("political_successor_unknown",e),"unknown command refused");
 }
 check(!is_political_successor_round_fact_kind("political_successor_round_unknown"),"unknown fact refused");std::cout<<"POLITICAL_SUCCESSOR_ROUND_LAYOUT "<<checks<<"/0\n";return 0;
}catch(const std::exception& e){std::cerr<<"failed after "<<checks<<": "<<e.what()<<'\n';return 1;}}
