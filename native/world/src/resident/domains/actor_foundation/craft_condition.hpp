// C02's actual complete N gate is the only ordinary craft exertion source.
#pragma once
#include "character_condition_read.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
class GrowthCapabilityRevisionWrite;
struct CraftConditionApplication {
 CheckpointMemoryCharge text_memory;
 ConditionReceiptRefV2 receipt_ref;
 CheckpointVector<BoundActorConditionWrite> actual_writes;
 bool already_applied=false;
};
// Call in the original gate's maker / accepted-helper order. Every fresh body
// write is immediately adopted by the same candidate's capability writer.
// The owning World transaction rolls back gate, body, capability and resources.
Outcome<CraftConditionApplication> apply_verified_craft_gate_condition(
 CommonStore&,AuthoredActorFoundationState&,const CharacterConditionSources&,
 const CraftJobReadContext&,const SourceRef& actual_gate,const StableId& actor,
 const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&,
 GrowthCapabilityRevisionWrite&);
Status validate_craft_gate_condition_receipt(const CharacterConditionSources&,const ConditionReceiptV2&);
Status validate_craft_gate_condition_coverage(const CharacterConditionSources&);
// Exact original owner ref for retention. No fabricated Common condition head.
Outcome<SourceRef> read_craft_condition_original_source(const CharacterConditionSources&,const ConditionReceiptRefV2&);
class CraftConditionSourcePinSink {
public:virtual ~CraftConditionSourcePinSink()=default;
 virtual Status actual_pin(const SourcePin&)=0;
};
RequiredSourceConsumerSlot craft_condition_source_consumer_slot();
Status visit_craft_condition_source_pins(const CharacterConditionSources&,CraftConditionSourcePinSink&);
Status validate_craft_condition_source_pin(const CharacterConditionSources&,const SourcePin&);
}
