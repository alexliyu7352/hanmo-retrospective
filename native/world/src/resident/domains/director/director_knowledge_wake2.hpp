#pragma once
#include "director_subject2.hpp"
#include "resident/domains/social/social_time2.hpp"
namespace hanmo::resident {
// A wake refers to the existing receiver/report receipt. It is not a new message or knowledge ledger.
SourceRef director_known_report_wake_source2(const SocialKnowledgeKey&);
Outcome<SubjectWake2> read_director_known_report_wake2(const CommonStore&,const SocialState&,const ExactResultTimeRead&,const ExactResultReadStamp&,const SocialKnowledgeKey&,const WorldMoment&);
Status validate_director_known_report_wake2(const CommonStore&,const SocialState&,const ExactResultTimeRead&,const ExactResultReadStamp&,const SubjectWake2&,const WorldMoment&);
// Only the original KnownReport rows of registered subjects are visited. Pure reads never call this writer.
Status collect_director_knowledge_wakes2(const CommonStore&,const SocialState&,const ExactResultTimeRead&,const ExactResultReadStamp&,DirectorState&,const WorldMoment&);
}
