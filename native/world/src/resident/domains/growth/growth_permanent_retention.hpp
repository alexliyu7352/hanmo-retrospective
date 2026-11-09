// Real retained permanent results and source-local capability predecessors.
#pragma once
#include "growth_capability_chain.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
enum class GrowthPermanentConsumerSlot {capability_current,condition_predecessor,training_predecessor,study_lesson_predecessor,study_adjustment_predecessor,practice_predecessor,milestone_predecessor,practice_source,milestone_source,owned_result};
RequiredSourceConsumerSlot growth_permanent_consumer_slot(GrowthPermanentConsumerSlot);
class GrowthPermanentPinSink {public:virtual ~GrowthPermanentPinSink()=default;virtual Status actual_pin(const SourcePin&)=0;};
Status visit_growth_permanent_source_pins(const GrowthPermanentSources&,const GrowthState&,GrowthPermanentConsumerSlot,GrowthPermanentPinSink&);
Status validate_growth_permanent_source_pin(const GrowthPermanentSources&,const GrowthState&,GrowthPermanentConsumerSlot,const SourcePin&);
Outcome<bool> growth_permanent_source_owner_may_release(const GrowthPermanentSources&,const GrowthState&,const SourceRef&);
}
