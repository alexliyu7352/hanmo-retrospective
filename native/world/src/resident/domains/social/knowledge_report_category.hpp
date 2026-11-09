#pragma once
#include "knowledge_category.hpp"
#include "social_time2.hpp"
namespace hanmo::resident {
// Category is derived from a real original result projection. This reader does
// not establish that an observer knows it and must not expose hidden existence.
Outcome<std::optional<KnowledgeCategory>> read_exact_report_category(const CommonStore&,const Report&,const ExactResultTimeRead&,const ExactResultReadStamp&);
}
