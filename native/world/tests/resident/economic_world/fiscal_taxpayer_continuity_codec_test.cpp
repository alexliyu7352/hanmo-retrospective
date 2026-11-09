// 只验新增段字节与opt-in边界，未接阶段前不声称占领暂停已经可玩。
#include "resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity.hpp"
#include "resident/storage/common_checkpoint.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main(){
 FiscalTaxpayerContinuityState state;state.suspended_periods.insert("fiscal_period_assessed:7",{"fiscal_period_assessed:7","district","politics_fiscal:2",4,{"source"}});const auto raw=encode_fiscal_taxpayer_continuity(state);auto read=decode_fiscal_taxpayer_continuity(raw);assert(read.ok&&compare_boundary(raw,encode_fiscal_taxpayer_continuity(read.value)).exact());CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);assert(visit_fiscal_taxpayer_continuity_checkpoint(writer,state).ok&&writer.finish().ok);FiscalTaxpayerContinuityState cold;const auto& input=bytes;CodecVisitor reader(input);assert(visit_fiscal_taxpayer_continuity_checkpoint(reader,cold).ok&&reader.finish().ok&&compare_boundary(raw,encode_fiscal_taxpayer_continuity(cold)).exact());
 auto bad=raw;bad.as<BoundaryValue::Dict>()->emplace_back("version",BoundaryValue{Integer{1}});assert(!decode_fiscal_taxpayer_continuity(bad).ok);bad=raw;bad.find("version")->data=Integer{2};assert(!decode_fiscal_taxpayer_continuity(bad).ok);bad=raw;bad.as<BoundaryValue::Dict>()->emplace_back("balance",BoundaryValue{Integer{20}});assert(!decode_fiscal_taxpayer_continuity(bad).ok);
 auto profile=boundary::obj({{"enabled",BoundaryValue{true}},{"version",BoundaryValue{Integer{1}}}});assert(decode_fiscal_taxpayer_continuity_profile(profile).ok);profile.find("version")->data=true;assert(!decode_fiscal_taxpayer_continuity_profile(profile).ok);
 std::vector<std::pair<StableId,StableId>> layout{{"dynamic_world","local_society_v1"},{"fiscal_settlement","1"},{"political_takeover","1"},{"economic_takeover_execution","1"},{"task_takeover_execution","2"}};assert(validate_fiscal_taxpayer_continuity_layout(false,layout).ok&&!validate_fiscal_taxpayer_continuity_layout(true,layout).ok);layout.emplace_back(fiscal_taxpayer_continuity_module,"1");assert(validate_fiscal_taxpayer_continuity_layout(true,layout).ok&&!validate_fiscal_taxpayer_continuity_layout(false,layout).ok);layout.push_back(layout.back());assert(!validate_fiscal_taxpayer_continuity_layout(true,layout).ok);
 std::cout<<"fiscal taxpayer continuity: explicit profile, old layout, exact JSON/binary, unknown version/field and duplicate refusal PASS\n";
}
