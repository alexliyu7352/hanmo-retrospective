#pragma once
#include "social_relation_execution.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
class OriginalSocialRelationPinSink {
public:virtual ~OriginalSocialRelationPinSink()=default;
 virtual Status actual_pin(const SourcePin&,Integer source_schema)=0;
};
// Original mutable base depends on every retained original application; each
// application in turn pins the complete actual business cause, never a guess.
Status visit_original_social_relation_history_pins(const OriginalSocialRelationContext&,OriginalSocialRelationPinSink&);
}
