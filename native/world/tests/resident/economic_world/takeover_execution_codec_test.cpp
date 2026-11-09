// 只核两个新经济冷段的字段闭集；重复字段不能以first-match吞掉第二份资产来源。
#include "resident/domains/local_economy/takeover/execution_receipts.hpp"
#include "resident/domains/local_economy/disruptions/production_disruptions_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
// 发布构建仍执行该门，不能用NDEBUG关闭坏档判断。
void check(bool good,const char* text){if(!good)throw std::runtime_error(text);}
// 检查点回写应严格保持字节，不替任何缺字段补默认值。
CodecVisitor::Buffer bytes(EconomicTakeoverExecutionState state){CodecVisitor::Buffer out;CodecVisitor visitor(out);check(visit_economic_takeover_execution_checkpoint(visitor,state).ok&&visitor.finish().ok,"encode takeover checkpoint");return out;}
}
int main(){try{
 EconomicTakeoverExecutionState state;state.payments.insert("receipt",{"receipt","arrear","registration","approval","decision",EconomicTakenPaymentScope::captured_arrear});
 auto encoded=encode_economic_takeover_execution(state);auto decoded=decode_economic_takeover_execution(encoded);check(decoded.ok&&bytes(decoded.value)==bytes(state),"exact takeover JSON roundtrip");
 auto binary=bytes(state);CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(binary));EconomicTakeoverExecutionState restored;check(visit_economic_takeover_execution_checkpoint(reader,restored).ok&&reader.finish().ok&&bytes(restored)==binary,"exact takeover binary roundtrip");
 auto duplicate=encoded;duplicate.as<BoundaryValue::Dict>()->emplace_back("version",BoundaryValue{Integer{1}});check(!decode_economic_takeover_execution(duplicate).ok,"duplicate takeover version accepted");
 duplicate=encoded;duplicate.find("payments")->find("receipt")->as<BoundaryValue::Dict>()->emplace_back("receipt_id",BoundaryValue{StableId{"other"}});check(!decode_economic_takeover_execution(duplicate).ok,"nested takeover source duplicate accepted");
 auto wrong=encoded;wrong.find("payments")->find("receipt")->find("scope")->data=StableId{"invented"};check(!decode_economic_takeover_execution(wrong).ok,"unknown payment scope accepted");
 ProductionDisruptionState disruption;disruption.registrations.insert("registered",{"registered","rain",0});auto original=encode_production_disruption_state(disruption);check(decode_production_disruption_state(original).ok,"disruption JSON roundtrip");duplicate=original;duplicate.as<BoundaryValue::Dict>()->emplace_back("version",BoundaryValue{Integer{1}});check(!decode_production_disruption_state(duplicate).ok,"duplicate disruption version accepted");
 duplicate=original;duplicate.find("registrations")->find("registered")->as<BoundaryValue::Dict>()->emplace_back("cause_id",BoundaryValue{StableId{"another"}});check(!decode_production_disruption_state(duplicate).ok,"nested disruption source duplicate accepted");
 ProductionDisruptionCatalog catalog;auto author=encode_production_disruption_catalog(catalog);author.as<BoundaryValue::Dict>()->emplace_back("profile",BoundaryValue{StableId{"another"}});check(!decode_production_disruption_catalog(author).ok,"duplicate authored policy accepted");
 std::cout<<"economic new-segment strict fields and exact cold bytes PASS\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
