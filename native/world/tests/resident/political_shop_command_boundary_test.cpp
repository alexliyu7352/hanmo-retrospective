// 仅真实中央命令/回执编解码门；不把格式通过冒充法院权源或完整World办理。
#include "resident/kernel/command_types.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_profile.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
std::shared_ptr<const CommitResult> done(const StableId& kind){auto value=std::make_shared<CommitResult>();value->ok=true;value->kind=kind;value->tick=0;value->revision=1;value->result_id="result:8";value->details=std::make_shared<ReceiptDetails>(ReceiptDetails{Done{}});return value;}
CodecVisitor::Buffer bytes(std::shared_ptr<const CommitResult> value,std::uint32_t flags){CodecVisitor::Buffer out;CodecVisitor writer(out);writer.schema_features=flags;check(visit_resident_receipt(writer,value).ok&&writer.finish().ok,"actual receipt writer succeeds only with permitted shape");return out;}
}
int main(){try{
 const ActingAuthority authority{"county","magistracy",0,"magistrate"};
 const std::vector<PoliticalShopLicensingCommand> commands{
  PoliticalRequestShopPermission{"court","premises"},
  PoliticalRegisterWanted{authority,"court","subject","case"},
  PoliticalReleaseWanted{authority,"political_wanted:3"},
  PoliticalRevokeShopPermit{authority,"political_shop_permit:4"}};
 for(const auto& command:commands){
  const auto kind=political_shop_licensing_command_kind(command);const auto params=encode_political_shop_licensing_command(command);auto decoded=decode_resident_command(kind,params);
  check(decoded.ok&&decoded.value.kind==kind&&std::holds_alternative<PoliticalShopLicensingCommand>(decoded.value.data),"actual central parser dispatches exact four legal variants");
  check(compare_boundary(params,encode_political_shop_licensing_command(std::get<PoliticalShopLicensingCommand>(decoded.value.data))).exact(),"all original actor/authority/premises arguments survive central decode");
  auto duplicated=params;duplicated.as<BoundaryValue::Dict>()->push_back(duplicated.as<BoundaryValue::Dict>()->front());check(!decode_resident_command(kind,duplicated).ok,"central entry preserves recursive duplicate-key rejection");
  auto value=done(kind);auto json=encode_resident_receipt({true,value,{}});check(json.ok&&decode_resident_receipt(json.value).ok,"new Done JSON has actual empty-details shape");
  CodecVisitor::Buffer rejected;CodecVisitor absent(rejected);check(!visit_resident_receipt(absent,value).ok,"missing legal feature refuses new receipt");
  CodecVisitor::Buffer old_bit;CodecVisitor legacy(old_bit);legacy.schema_features=8u;check(!visit_resident_receipt(legacy,value).ok,"old broad politics bit cannot authorize legal receipt");
  const auto wire=bytes(value,political_shop_licensing_checkpoint_feature);std::shared_ptr<const CommitResult> restored;CodecVisitor reader(wire);reader.schema_features=political_shop_licensing_checkpoint_feature;check(visit_resident_receipt(reader,restored).ok&&reader.finish().ok&&restored&&restored->kind==kind&&restored->details&&std::holds_alternative<Done>(restored->details->data),"explicit legal feature reads original new Done variant");
  CodecVisitor stripped(wire);restored.reset();check(!visit_resident_receipt(stripped,restored).ok,"legal receipt cannot load under missing feature");
  auto wrong=std::make_shared<CommitResult>(*value);wrong->details=std::make_shared<ReceiptDetails>(ReceiptDetails{ActionStarted{"action:4"}});std::shared_ptr<const CommitResult> bad=wrong;CodecVisitor::Buffer other;CodecVisitor other_writer(other);other_writer.schema_features=political_shop_licensing_checkpoint_feature;check(!visit_resident_receipt(other_writer,bad).ok,"legal feature cannot authorize ActionStarted or another receipt tag");check(!encode_resident_receipt({true,bad,{}}).ok,"wrong legal receipt JSON tag rejected");
 }
 for(const StableId kind:{"political_issue_shop_permit","political_record_shop_clearance"})check(!political_shop_licensing_command_kind(kind)&&!decode_resident_command(kind,boundary::obj({})).ok,"Grant-only declarations cannot be invoked as legal commands");
 const auto old=done("task_personal_advance");check(bytes(old,0)==bytes(old,political_shop_licensing_checkpoint_feature),"enabling legal pair adds no bytes to prior Done receipt");
 std::cout<<checks<<" political shop boundary checks, 0 failures; codec only\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
