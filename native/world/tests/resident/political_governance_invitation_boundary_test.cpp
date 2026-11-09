// 显式profile、具名共同头与typed Done冷边界；不冒完整World行为结果。
#include "resident/kernel/command_types.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;
void check(bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);}
// 使用真实Common JSON边界验证具名kind/domain，不预写另一领域payload。
Outcome<CommonState> read_fact(const StableId& kind,const StableId& domain){CommonState state;state.metadata.ruleset_id=dynamic_ruleset_id;state.next_instance_id=3;state.facts.insert("governance_invitation_reply:1",{"governance_invitation_reply:1",kind,domain,{}});auto value=encode_common_state(state);std::get<BoundaryValue::Dict>(value.data).emplace_back("results",BoundaryValue{BoundaryValue::Dict{}});auto* fact=value.find("facts")->find("governance_invitation_reply:1");std::get<BoundaryValue::Dict>(fact->data).emplace_back("domain",BoundaryValue{domain});return decode_common_seed(value,CommonCatalog{},nullptr,CommonSeedValidation::defer_until_bundle);}
// 各variant只可由自己的feature接受；不沿宽politics位借新Done。
Status receipt(const StableId& kind,std::uint32_t feature,bool done){auto value=std::make_shared<CommitResult>();value->kind=kind;value->result_id="result:2";value->revision=1;auto details=std::make_shared<ReceiptDetails>();if(done)details->data=Done{};else details->data=ActionStarted{"action:1"};value->details=details;std::shared_ptr<const CommitResult> input=value;CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=feature;return visit_resident_receipt(writer,input);}
}
int main(){try{
 using Pair=std::pair<StableId,StableId>;std::vector<Pair> modules{{"politics_governance","1"},{"governance_tasks","1"},{"public_relief","1"},{"family_life","1"},{governance_invitation_reply_module,"1"}};
 check(validate_governance_invitation_reply_modules(modules,true).ok,"complete profile rejected");
 for(std::size_t i=0;i<modules.size();++i){auto missing=modules;missing.erase(missing.begin()+i);check(!validate_governance_invitation_reply_modules(missing,true).ok,"missing dependency accepted");}
 auto duplicate=modules;duplicate.push_back(modules.back());check(!validate_governance_invitation_reply_modules(duplicate,true).ok,"duplicate module accepted");auto unknown=modules;unknown.back().second="2";check(!validate_governance_invitation_reply_modules(unknown,true).ok,"unknown version accepted");check(!validate_governance_invitation_reply_modules(modules,false).ok,"disabled profile accepts marker");check(validate_governance_invitation_reply_modules({},false).ok,"legacy empty profile changed");
 check(read_fact(governance_invitation_reply_fact_kind,governance_invitation_reply_module).ok,"exact reply head rejected");
 for(const auto* kind:{"political_governance_unknown","governance_authority_issued","political_governance_continued"})check(!read_fact(kind,governance_invitation_reply_module).ok,"unrelated kind borrows new domain");
 for(const auto* domain:{"politics","interaction","political_governance_continuations"})check(!read_fact(governance_invitation_reply_fact_kind,domain).ok,"reply head accepts wrong domain");
 for(const auto* command:{governance_decline_command,governance_withdraw_command}){check(!receipt(command,0,true).ok,"new Done accepts absent feature");check(!receipt(command,16777216u,true).ok,"new Done borrows continuation feature");check(receipt(command,governance_invitation_reply_feature,true).ok,"new Done exact feature rejected");check(!receipt(command,governance_invitation_reply_feature,false).ok,"action receipt impersonates zero-time reply");}
 std::cout<<checks<<" checks; invitation boundary only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
