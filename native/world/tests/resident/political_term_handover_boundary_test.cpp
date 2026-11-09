// 独立继位module选择原PoliticsEffects，不增32位feature或改变旧typed字段。
#include "resident/kernel/command_types.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);}
// 通过真实Common JSON入口核五种头和唯一domain，额外业务载荷不可藏在Common。
Outcome<CommonState> fact(const StableId& kind,const StableId& domain,bool extra=false){CommonState s;s.metadata.ruleset_id=dynamic_ruleset_id;s.next_instance_id=3;s.facts.insert("political_term_handover_offer:1",{"political_term_handover_offer:1",kind,domain,{}});auto value=encode_common_state(s);std::get<BoundaryValue::Dict>(value.data).emplace_back("results",BoundaryValue{BoundaryValue::Dict{}});auto* record=value.find("facts")->find("political_term_handover_offer:1");std::get<BoundaryValue::Dict>(record->data).emplace_back("domain",BoundaryValue{domain});if(extra)std::get<BoundaryValue::Dict>(record->data).emplace_back("candidate",BoundaryValue{"forged"});return decode_common_seed(value,CommonCatalog{},nullptr,CommonSeedValidation::defer_until_bundle);}
// 使用真正typed结果writer/reader；这里只测试格式选择，政治本账冷验另核真实来源。
Outcome<CodecVisitor::Buffer> encode(const StableId& kind,Integer version,std::uint32_t features,ReceiptData data){auto value=std::make_shared<CommitResult>();value->kind=kind;value->result_id="result:3";value->revision=1;auto detail=std::make_shared<ReceiptDetails>();detail->data=std::move(data);value->details=detail;std::shared_ptr<const CommitResult> ptr=value;CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.political_term_handover_codec_version=version;writer.schema_features=features;auto r=visit_resident_receipt(writer,ptr);if(r.ok)r=writer.finish();return r.ok?Outcome<CodecVisitor::Buffer>{true,std::move(bytes),{}}:Outcome<CodecVisitor::Buffer>{false,{},r.error};}
Status decode(const CodecVisitor::Buffer& bytes,Integer version){CodecVisitor reader(bytes);reader.political_term_handover_codec_version=version;std::shared_ptr<const CommitResult> value;auto r=visit_resident_receipt(reader,value);return r.ok?reader.finish():r;}
}
int main(){try{
 for(const auto* kind:{"political_term_handover_offered","political_term_handover_accepted","political_term_handover_declined","political_term_handover_withdrawn"}){check(fact(kind,political_term_handover_domain).ok,"exact fact rejected");check(!fact(kind,"politics").ok,"new kind borrows old politics");check(!fact(kind,political_term_handover_domain,true).ok,"extra fact payload accepted");}
 check(!fact("political_successor_unknown",political_term_handover_domain).ok,"unknown kind borrows new domain");
 for(const auto* kind:{political_term_handover_offer_command,political_term_handover_answer_command,political_term_handover_withdraw_command}){PoliticsEffects e{{},{"political_term_handover_offer:1"},{}};if(StableId{kind}==political_term_handover_answer_command){e.fact_ids={"political_term_handover_acceptance:1"};e.receipt_ids={"organization:2"};}else if(StableId{kind}==political_term_handover_withdraw_command)e.fact_ids={"political_term_handover_withdrawal:1"};auto bytes=encode(kind,1,0,e);check(bytes.ok,"selected new effect rejected");check(decode(bytes.value,1).ok,"selected new reader rejected");check(!decode(bytes.value,0).ok,"new bytes accepted without module");check(!encode(kind,0,~std::uint32_t{0},e).ok,"all old feature bits grant new command");check(!encode(kind,2,~std::uint32_t{0},e).ok,"unknown module version grants new command");check(!encode(kind,1,0,Done{}).ok,"empty Done impersonates source");e.fact_ids.clear();check(!encode(kind,1,0,e).ok,"missing exact source accepted");}
 // 所有旧feature无关时，新context不向旧Done添加任何字节。
 auto old=encode("wait",0,0,Done{});auto selected=encode("wait",1,0,Done{});check(old.ok&&selected.ok&&old.value==selected.value,"old typed bytes changed");
 std::cout<<checks<<" succession format checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
