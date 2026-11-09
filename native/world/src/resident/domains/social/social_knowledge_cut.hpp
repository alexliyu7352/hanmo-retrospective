#pragma once
#include "social_time2.hpp"
#include "social_time2_version.hpp"
namespace hanmo::resident {
// Called only at successful original learn, before the candidate can be published.
// Existing knowledge is never rewritten and restore never calls this writer.
Status capture_social_knowledge_cut(const CommonStore&,Integer physical_version,SocialSourceTimeBinding&);
// Own row shape, actual report source and retained Common prefix anchor.
Status validate_social_knowledge_cut(const CommonStore&,const DynamicSocialState&,const SocialSourceTimeBinding&);
// The original channel owner supplies its actual immutable Common witness.
// This does not validate that owner's business permission or manufacture a fact.
Status validate_social_knowledge_cut_source(const CommonStore&,const SocialSourceTimeBinding&,const SourceRef& actual_source);
Status validate_social_knowledge_cut_before_source(const CommonStore&,const SocialSourceTimeBinding&,const SourceRef& actual_consumer);
}
