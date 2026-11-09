// 实际中央入口只承认具名日租命令、原InteractionEffect与独立feature；不代本人许可或资金门。
#include "resident/kernel/command_types.hpp"
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_boundary.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
std::shared_ptr<const CommitResult> receipt(const StableId& kind,const StableId& source){auto r=std::make_shared<CommitResult>();r->kind=kind;r->tick=3;r->revision=8;r->result_id="result:30";InteractionEffect e;e.settlement_result=source;r->details=std::make_shared<ReceiptDetails>(ReceiptDetails{e,{}});return r;}
CodecVisitor::Buffer wire(std::shared_ptr<const CommitResult> r,std::uint32_t flags){CodecVisitor::Buffer bytes;CodecVisitor v(bytes);v.schema_features=flags;check(visit_resident_receipt(v,r).ok&&v.finish().ok,"actual typed writer");return bytes;}
}
int main(){try{
 for(const auto& command:std::vector<PrivateShopDailyRentCommand>{PrivateShopPayRentArrear{"private_shop:1","private_shop_rent_assessed:9","wallet.me",{},{}},PrivateShopResumeRent{"private_shop:1"}}){
  const auto kind=private_shop_daily_rent_command_kind(command);auto params=encode_private_shop_daily_rent_command(command);auto decoded=decode_resident_command(kind,params);check(decoded.ok&&std::holds_alternative<PrivateShopDailyRentCommand>(decoded.value.data),"central decode exact typed command");check(compare_boundary(params,encode_private_shop_daily_rent_command(std::get<PrivateShopDailyRentCommand>(decoded.value.data))).exact(),"original explicit debt and payer unchanged");
  auto duplicate=params;duplicate.as<BoundaryValue::Dict>()->push_back(duplicate.as<BoundaryValue::Dict>()->front());check(!decode_resident_command(kind,duplicate).ok,"duplicate key refused at real parser");
  const auto source=std::holds_alternative<PrivateShopPayRentArrear>(command)?"private_shop_rent_paid:24":"private_shop_rent_resumed:24";auto r=receipt(kind,source);auto json=encode_resident_receipt({true,r,{}});check(json.ok&&decode_resident_receipt(json.value).ok,"precise zero-time JSON receipt roundtrip");
  for(const std::uint32_t flags:{0u,262144u,4194304u}){CodecVisitor::Buffer out;CodecVisitor v(out);v.schema_features=flags;auto value=r;check(!visit_resident_receipt(v,value).ok,"old Shop and executor bits cannot grant rent receipt");}
  auto bytes=wire(r,private_shop_daily_rent_checkpoint_feature);CodecVisitor reader(bytes);reader.schema_features=private_shop_daily_rent_checkpoint_feature;std::shared_ptr<const CommitResult> restored;check(visit_resident_receipt(reader,restored).ok&&reader.finish().ok&&restored->kind==kind,"selected feature restores exact typed receipt");
  CodecVisitor missing(bytes);restored.reset();check(!visit_resident_receipt(missing,restored).ok,"stripped feature rejects typed read");
  for(const auto& details:std::vector<ReceiptDetails>{ReceiptDetails{Done{}},ReceiptDetails{ActionStarted{"action:24"}},ReceiptDetails{InteractionEffect{"","private_shop_rent_assessed:24"}},ReceiptDetails{InteractionEffect{"foreign","private_shop_rent_paid:24"}}}){auto wrong=std::make_shared<CommitResult>(*r);wrong->details=std::make_shared<ReceiptDetails>(details);std::shared_ptr<const CommitResult> value=wrong;CodecVisitor::Buffer out;CodecVisitor v(out);v.schema_features=private_shop_daily_rent_checkpoint_feature;check(!visit_resident_receipt(v,value).ok,"wrong tag, assessed source or extra effect refused");check(!encode_resident_receipt({true,value,{}}).ok,"wrong receipt JSON also refused");}
 }
 auto old=std::make_shared<CommitResult>();old->kind="task_personal_advance";old->result_id="result:4";old->details=std::make_shared<ReceiptDetails>(ReceiptDetails{Done{}});check(wire(old,0)==wire(old,private_shop_daily_rent_checkpoint_feature),"old Done bytes stay identical");
 check(!decode_resident_command("private_shop_resume_rent_extra",boundary::obj({})).ok,"unknown lookalike has no prefix admission");
 std::cout<<checks<<" daily rent command boundary checks, 0 failures; format only\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
