// 只核typed回执与非序列化版本上下文：无模块拒新kind，旧结果字节不随新选择变化。
#include "resident/kernel/command_types.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
std::shared_ptr<const CommitResult> sample(const StableId& kind,const ReceiptDetails& details){auto r=std::make_shared<CommitResult>();r->ok=true;r->kind=kind;r->tick=1;r->revision=1;r->result_id="result:3";r->details=std::make_shared<ReceiptDetails>(details);return r;}
Status write(std::shared_ptr<const CommitResult> source,Integer version,CodecVisitor::Buffer& bytes){CodecVisitor v(bytes);v.political_governance_agency_codec_version=version;auto s=visit_resident_receipt(v,source);return s.ok?v.finish():s;}
}
int main(){try{
 CodecVisitor::Buffer old,enabled;auto advance=sample("advance",ReceiptDetails{AdvanceDetails{},{}});check(write(advance,0,old).ok,"legacy actual clock receipt writes");check(write(advance,1,enabled).ok&&enabled==old,"new context adds zero bytes to old receipt");
 PoliticsEffects e;e.fact_ids={"political_governance_agency_issue:1"};e.receipt_ids={"politics_governance_agency_authority:2"};auto issue=sample(political_governance_agency_issue_command,ReceiptDetails{e,{}});check(!write(issue,0,old).ok,"no module cannot encode a delegated signature result");check(write(issue,1,enabled).ok,"selected module writes exact original-authority effect");
 const auto bytes=enabled;CodecVisitor reader(bytes);reader.political_governance_agency_codec_version=1;std::shared_ptr<const CommitResult> result;check(visit_resident_receipt(reader,result).ok&&reader.finish().ok&&result->kind==political_governance_agency_issue_command,"selected reader consumes same original result bytes");CodecVisitor legacy(bytes);result.reset();check(!visit_resident_receipt(legacy,result).ok,"legacy reader cannot accept new kind with old politics tag");
 check(!write(sample(political_governance_agency_issue_command,ReceiptDetails{Done{},{}}),1,old).ok,"new successful command cannot hide original authority under Done");check(political_governance_agency_effect_uses_sources(e),"exclusive signature and original authority source recognized");check(!write(sample("political_appoint",ReceiptDetails{e,{}}),1,old).ok,"old political kind cannot take the delegated effect");
 std::cout<<checks<<" agency result version assertions; no World\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
