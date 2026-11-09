// 原版本选择和实际效果口的纯边界门，不构造玩家履历或Office授权。
#include "resident/domains/politics/term_handover/political_term_handover_profile.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}}
int main(){try{
 std::vector<std::pair<StableId,StableId>> modules={{"politics","1"},{"family_life","1"},{"task_message_notices","1"},{"task_message_handoffs","2"},{"political_successions","1"},{"political_successor_rounds","1"}};
 auto old=political_term_handover_selected_version(modules);check(old.ok&&old.value==0,"old rounds do not select living transfer");modules.emplace_back(political_term_handover_domain,"1");auto selected=political_term_handover_selected_version(modules);check(selected.ok&&selected.value==1,"explicit paired term admitted");
 auto bad=modules;bad.push_back(modules.back());check(!political_term_handover_selected_version(bad).ok,"duplicate new marker refused");bad=modules;bad.back().second="2";check(!political_term_handover_selected_version(bad).ok,"unknown version refused");
 for(std::size_t i=0;i+1<modules.size();++i){bad=modules;bad.erase(bad.begin()+i);check(!political_term_handover_selected_version(bad).ok,"each original prerequisite retained");}
 struct Shape{const char* kind;const char* source;bool office;};
 for(const auto& row:std::vector<Shape>{{political_term_handover_offer_command,"political_term_handover_offer:7",false},{political_term_handover_answer_command,"political_term_handover_acceptance:7",true},{political_term_handover_answer_command,"political_term_handover_decline:7",false},{political_term_handover_withdraw_command,"political_term_handover_withdrawal:7",false}}){
  PoliticsEffects e;e.fact_ids={row.source};if(row.office)e.receipt_ids={"organization:8"};check(valid_political_term_handover_effect(row.kind,e),"exact own source accepted");auto wrong=e;wrong.fact_ids.push_back(row.source);check(!valid_political_term_handover_effect(row.kind,wrong),"duplicate business source refused");wrong=e;wrong.appointment_id="organization:8";check(!valid_political_term_handover_effect(row.kind,wrong),"old appointment field refused");wrong=e;if(row.office)wrong.receipt_ids.clear();else wrong.receipt_ids={"organization:8"};check(!valid_political_term_handover_effect(row.kind,wrong),"Office change only accompanies acceptance");wrong=e;wrong.fact_ids={StableId(row.source).substr(0,StableId(row.source).rfind(':')+1)+"07"};check(!valid_political_term_handover_effect(row.kind,wrong),"noncanonical source refused");check(!valid_political_term_handover_effect("political_term_handover_unknown",e),"unknown command refused");
 }
 PoliticsEffects old_effect;old_effect.fact_ids={"political_head_handover_acceptance:7"};old_effect.receipt_ids={"organization:8"};check(!valid_political_term_handover_effect(political_term_handover_answer_command,old_effect),"first-head transfer is not later-term source");check(!is_political_term_handover_fact_kind("political_term_handover_unknown"),"unknown fact refused");std::cout<<"POLITICAL_TERM_HANDOVER_LAYOUT "<<checks<<"/0\n";return 0;
 }catch(const std::exception& e){std::cerr<<"failed after "<<checks<<": "<<e.what()<<'\n';return 1;}}
