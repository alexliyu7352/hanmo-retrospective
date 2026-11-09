// Original source dependencies of a retained complete skill qualification.
#pragma once
#include "growth_skill_qualification_v2.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
class GrowthSkillQualificationPinSink {public:virtual ~GrowthSkillQualificationPinSink()=default;virtual Status actual_pin(const SourcePin&)=0;};
Status visit_growth_skill_qualification_source_pins(const CharacterConditionSources&,const GrowthState&,const GrowthSkillQualificationV2&,const ConsumerRef&,GrowthSkillQualificationPinSink&);
}
