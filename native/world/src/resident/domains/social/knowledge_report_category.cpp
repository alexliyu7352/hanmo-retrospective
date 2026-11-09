#include "knowledge_report_category.hpp"
namespace hanmo::resident {
Outcome<std::optional<KnowledgeCategory>> read_exact_report_category(const CommonStore&common,const Report&r,const ExactResultTimeRead&exact,const ExactResultReadStamp&stamp){
 auto valid=validate_exact_report_source(common,r,exact,stamp);if(!valid.ok)return {false,{},valid.error};
 if(r.observation.supply)return {true,KnowledgeCategory::supply,{}};
 if(r.observation.obligation)return {true,KnowledgeCategory::arrear,{}};
 if(r.payload_kind=="intelligence_probe_location")return {true,KnowledgeCategory::intelligence_probe,{}};
 if(r.payload_kind=="cultural_content")return {true,KnowledgeCategory::cultural_content,{}};
 if(r.payload_kind=="patrol_work_result"||r.payload_kind=="supervised_work_result")return {true,KnowledgeCategory::action_outcome,{}};
 auto action=exact.action_result_v2_borrowed(r.fact_id,stamp);if(!action.ok)return {false,{},action.error};
 return {true,action.value?std::optional<KnowledgeCategory>{KnowledgeCategory::action_outcome}:std::nullopt,{}};
}
}
