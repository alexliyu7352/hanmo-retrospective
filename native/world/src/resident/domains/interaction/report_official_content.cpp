#include "report_official_content.hpp"
#include "report_disclosure.hpp"
namespace hanmo::resident {
Outcome<std::optional<OfficialContentRef>> read_report_disclosure_official_content(const ReportDisclosureReadContext&x,const StableId&id){
 if(!x.social.dynamic||!x.social.dynamic->time2)return {false,{},common_failure("OfficialReportExactKnowledgeRequired",id).error};
 const auto*report=x.social.dynamic->reports.find(id);if(!report||report->id!=id)return {false,{},common_failure("OfficialOriginalReportMissing",id).error};
 auto valid=validate_exact_report_source(x.common,*report,x.results,x.result_stamp);if(!valid.ok)return {false,{},valid.error};
 if(!x.official_contents||!x.official_contents->bound_to(x.common,x.social))return {false,{},common_failure("OriginalReportOfficialBodyReadRequired",id,ErrorKind::unsupported_capability).error};
 auto value=x.official_contents->read_report_official_content(*report,x.results,x.result_stamp);if(!value.ok)return value;
 if(value.value){valid=validate_official_content_ref(*value.value);if(!valid.ok)return {false,{},valid.error};}
 return value;
}
}
