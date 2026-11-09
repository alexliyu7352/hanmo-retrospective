// 独立接待module只选择精确PoliticsEffects，不借旧feature位放行新命令。
#include "resident/kernel/command_types.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);}
// 原Common JSON只保存精确事实头，业务载荷必须留在独立接待账。
Outcome<CommonState> fact(const StableId& kind,const StableId& domain,bool extra=false){CommonState s;s.metadata.ruleset_id=dynamic_ruleset_id;s.next_instance_id=3;s.facts.insert("political_office_reception:1",{"political_office_reception:1",kind,domain,{}});auto value=encode_common_state(s);std::get<BoundaryValue::Dict>(value.data).emplace_back("results",BoundaryValue{BoundaryValue::Dict{}});auto* record=value.find("facts")->find("political_office_reception:1");std::get<BoundaryValue::Dict>(record->data).emplace_back("domain",BoundaryValue{domain});if(extra)std::get<BoundaryValue::Dict>(record->data).emplace_back("actor_id",BoundaryValue{"forged"});return decode_common_seed(value,CommonCatalog{},nullptr,CommonSeedValidation::defer_until_bundle);}
// 用真正typed writer/reader检查上下文选择，旧结果不新增字段或字节。
Outcome<CodecVisitor::Buffer> encode(const StableId& kind,Integer version,std::uint32_t features,ReceiptData data){auto value=std::make_shared<CommitResult>();value->kind=kind;value->result_id="result:3";value->revision=1;auto detail=std::make_shared<ReceiptDetails>();detail->data=std::move(data);value->details=detail;std::shared_ptr<const CommitResult> ptr=value;CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.political_office_reception_codec_version=version;writer.schema_features=features;auto r=visit_resident_receipt(writer,ptr);if(r.ok)r=writer.finish();return r.ok?Outcome<CodecVisitor::Buffer>{true,std::move(bytes),{}}:Outcome<CodecVisitor::Buffer>{false,{},r.error};}
Status decode(const CodecVisitor::Buffer& bytes,Integer version){CodecVisitor reader(bytes);reader.political_office_reception_codec_version=version;std::shared_ptr<const CommitResult> value;auto r=visit_resident_receipt(reader,value);return r.ok?reader.finish():r;}
}
int main(){try{
 const StableId kind="political_assume_office_reception";
 check(fact("political_office_reception_assumed","political_office_receptions").ok,"exact fact rejected");
 check(!fact("political_office_reception_assumed","politics").ok,"new fact borrows old politics");
 check(!fact("political_office_reception_assumed","political_office_receptions",true).ok,"extra Common payload accepted");
 check(!fact("political_office_reception_unknown","political_office_receptions").ok,"unknown fact borrows new domain");
 PoliticsEffects e{{},{"political_office_reception:1"},{}};auto bytes=encode(kind,1,0,e);check(bytes.ok,"selected new effect rejected");check(decode(bytes.value,1).ok,"selected reader rejected");check(!decode(bytes.value,0).ok,"new bytes accepted without module");
 check(!encode(kind,0,~std::uint32_t{0},e).ok,"old feature bits grant new command");check(!encode(kind,2,~std::uint32_t{0},e).ok,"unknown version grants new command");check(!encode(kind,1,0,Done{}).ok,"Done impersonates source");
 e.receipt_ids={"organization:2"};check(!encode(kind,1,0,e).ok,"office change impersonates reception");e.receipt_ids.clear();e.fact_ids.clear();check(!encode(kind,1,0,e).ok,"missing source accepted");e.fact_ids={"political_term_handover_acceptance:1"};check(!encode(kind,1,0,e).ok,"other political source accepted");
 auto old=encode("wait",0,0,Done{});auto selected=encode("wait",1,0,Done{});check(old.ok&&selected.ok&&old.value==selected.value,"old typed bytes changed");
 std::cout<<checks<<" reception format checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
