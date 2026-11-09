#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_codec.hpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_contract.hpp"
#include "resident/domains/politics/office_resignations/political_office_resignation_contract.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_contract.hpp"
#include "resident/domains/politics/term_handover/political_term_handover_contract.hpp"
#include "resident/domains/politics/successor_rounds/political_successor_round_contract.hpp"
#include "resident/domains/politics/head_handover/political_head_handover_contract.hpp"
#include "resident/domains/politics/succession/political_succession_contract.hpp"
// 真实原政治来源门、七域原识别器与同一World分派函数的必要反例；不冒完整World。
#include "resident/kernel/politics_receipt_routing.hpp"
#include "resident/kernel/politics_integration.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/domains/politics/office_resignations/political_office_resignation_receipts.hpp"
#include <iostream>
#include <stdexcept>
namespace R=hanmo::resident;
namespace {
int checks=0;
// 首红即保原理由，检查数不代玩法覆盖。
void need(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
// 这里只构造原政治effect格式，业务来源刻意不伪造为合法任免。
R::CommitResult receipt(const R::StableId& kind){R::CommitResult r;r.ok=true;r.kind=kind;r.tick=0;r.result_id="result:2";r.details=std::make_shared<R::ReceiptDetails>(R::ReceiptDetails{R::ReceiptData{R::PoliticsEffects{{},{"political_office_resignation_request:1"},{"political_office_resignation_request:1"}}}});return r;}
}
int main(){try{
 R::CommonStore common(R::CommonState{},std::make_shared<R::CommonCatalog>());R::PoliticsState state;R::OrganizationState org;
 struct Case{const char* kind;bool R::PoliticsReceiptRoutes::*owner;};
 const Case cases[]={
  {R::political_subordinate_offer_command,&R::PoliticsReceiptRoutes::subordinate_appointment},
  {R::political_subordinate_answer_command,&R::PoliticsReceiptRoutes::subordinate_appointment},
  {R::political_subordinate_withdraw_command,&R::PoliticsReceiptRoutes::subordinate_appointment},
  {R::political_subordinate_arrive_command,&R::PoliticsReceiptRoutes::subordinate_appointment},

  {R::political_succession_answer_command,&R::PoliticsReceiptRoutes::succession},
  {R::political_succession_accede_command,&R::PoliticsReceiptRoutes::succession},
  {R::political_succession_nominate_command,&R::PoliticsReceiptRoutes::succession},
  {R::political_succession_withdraw_command,&R::PoliticsReceiptRoutes::succession},
  {R::political_head_handover_offer_command,&R::PoliticsReceiptRoutes::head_handover},
  {R::political_head_handover_answer_command,&R::PoliticsReceiptRoutes::head_handover},
  {R::political_head_handover_withdraw_command,&R::PoliticsReceiptRoutes::head_handover},
  {R::political_successor_round_nominate_command,&R::PoliticsReceiptRoutes::successor_round},
  {R::political_successor_round_answer_command,&R::PoliticsReceiptRoutes::successor_round},
  {R::political_successor_round_withdraw_command,&R::PoliticsReceiptRoutes::successor_round},
  {R::political_successor_round_accede_command,&R::PoliticsReceiptRoutes::successor_round},
  {R::political_term_handover_offer_command,&R::PoliticsReceiptRoutes::term_handover},
  {R::political_term_handover_answer_command,&R::PoliticsReceiptRoutes::term_handover},
  {R::political_term_handover_withdraw_command,&R::PoliticsReceiptRoutes::term_handover},
  {R::political_office_reception_command,&R::PoliticsReceiptRoutes::office_reception},
  {R::political_office_resignation_request_command,&R::PoliticsReceiptRoutes::office_resignation},
  {R::political_office_resignation_answer_command,&R::PoliticsReceiptRoutes::office_resignation},
  {R::political_office_resignation_withdraw_command,&R::PoliticsReceiptRoutes::office_resignation},
  {R::political_governance_agency_offer_command,&R::PoliticsReceiptRoutes::governance_agency},
  {R::political_governance_agency_answer_command,&R::PoliticsReceiptRoutes::governance_agency},
  {R::political_governance_agency_revoke_command,&R::PoliticsReceiptRoutes::governance_agency},
  {R::political_governance_agency_issue_command,&R::PoliticsReceiptRoutes::governance_agency},
 };
 // 逐实际原合同命令常量核分派，未知名字始终留在旧门拒绝。
 for(const auto& x:cases){auto r=receipt(x.kind);const auto* old=std::get_if<R::PoliticsEffects>(&r.details->data);auto rejected=R::validate_politics_receipt_source(common,state,org,"p",r.kind,0,*old);need(!rejected.ok&&rejected.error.original.code=="PoliticalReceiptSourceMismatch","original source gate rejects non-original kind");need(R::politics_receipt_for_original(r,{})==old,"unselected effect remains at strict original gate");R::PoliticsReceiptRoutes owner;owner.*(x.owner)=true;need(R::politics_receipt_for_original(r,owner)==nullptr,"selected precise kind delegated to own gate");}
 R::PoliticsReceiptRoutes all{true,true,true,true,true,true,true,true};for(const auto* kind:{"political_appoint","political_answer_appointment","political_arrive","political_withdraw_appointment","political_dismiss","political_unknown","political_request_office_resignation_extra","advance"}){auto r=receipt(kind);need(R::politics_receipt_for_original(r,all)==std::get_if<R::PoliticsEffects>(&r.details->data),"old or unknown effect never silently filtered");}
 auto orphan=receipt(R::political_office_resignation_request_command);common.state().results.insert("p:1",std::make_shared<R::CommitResult>(orphan));R::PoliticalOfficeResignationState empty;auto late=R::validate_political_office_resignation_player_results(common,empty,{});need(!late.ok&&late.error.original.code=="PoliticalOfficeResignationCachedSourceMismatch","delegated extra cache still rejected by real own-domain reverse source gate");
 auto absent=receipt("political_appoint");absent.details.reset();need(!R::politics_receipt_for_original(absent,all),"no fake effect for absent details");
 std::cout<<"POLITICAL_RECEIPT_ROUTING "<<checks<<"/0 ORIGINAL_GATE_AND_ROUTING_ONLY\n";return 0;
}catch(const std::exception&e){std::cerr<<"POLITICAL_RECEIPT_ROUTING "<<checks<<"/1 "<<e.what()<<'\n';return 1;}}
