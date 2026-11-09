// 复用原各域唯一命令识别器，不按political前缀或Effects变体宽泛放过缓存。
#include "politics_receipt_routing.hpp"
#include "command_types.hpp"
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_codec.hpp"
#include "resident/domains/politics/succession/political_succession_codec.hpp"
#include "resident/domains/politics/head_handover/political_head_handover_codec.hpp"
#include "resident/domains/politics/successor_rounds/political_successor_round_codec.hpp"
#include "resident/domains/politics/term_handover/political_term_handover_codec.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_codec.hpp"
#include "resident/domains/politics/office_resignations/political_office_resignation_codec.hpp"
#include "resident/domains/politics/governance_agencies/political_governance_agency_codec.hpp"
namespace hanmo::resident {
const PoliticsEffects* politics_receipt_for_original(const CommitResult& r,const PoliticsReceiptRoutes& selected){
 const auto* effect=r.details?std::get_if<PoliticsEffects>(&r.details->data):nullptr;
 if(!effect)return nullptr;
 if((selected.succession&&is_political_succession_command(r.kind))||(selected.head_handover&&is_political_head_handover_command(r.kind))||(selected.successor_round&&is_political_successor_round_command(r.kind))||(selected.term_handover&&is_political_term_handover_command(r.kind))||(selected.office_reception&&is_political_office_reception_command(r.kind))||(selected.office_resignation&&is_political_office_resignation_command(r.kind))||(selected.governance_agency&&is_political_governance_agency_command(r.kind))||(selected.subordinate_appointment&&is_political_subordinate_appointment_command(r.kind)))return nullptr;
 return effect;
}
}
