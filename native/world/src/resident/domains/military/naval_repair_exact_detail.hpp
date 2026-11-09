#pragma once
#include "naval_repair_exact.hpp"
#include "naval_repair_source_local.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <set>
#include <charconv>
#include <algorithm>
#include <string_view>
namespace hanmo::resident::naval_repair_exact_detail {
using namespace movement_detail;
inline SourceRef start_ref(const StableId& id){return {"military_naval","naval_repair_start",id};}
inline SourceRef result_ref(const StableId& id){return {"military_naval","naval_repair_result",id};}
// The original charge has no independent Common fact. This is its unique
// owned-field address at the actual start, never a fabricated notification.
inline SourceRef charge_ref(const StableId& id){return {"military_naval","naval_repair_charge",id};}
inline void bound(const CommonStore& c,const MilitaryState& m,const NavalState& n,const MilitaryActionTimeState& s,const ExactNavalRepairReadContext& x){if(!x.original.bound_to(c,m,n,s))fail("CrossCandidateExactNavalRepairSources");}
inline void header(const CommonStore& c,const SourceRef& source){const auto* f=c.state().facts.find(source.id);if(!f||f->id!=source.id||f->kind!=source.kind||f->domain!=source.domain)fail("ExactNavalRepairOriginalHeaderRequired",source.id);}
inline const NewNavalRepairTimeSource2& fact(const MilitaryActionTimeState& s,const StableId& id){const auto* f=s.naval_repair_time_sources.find(id);if(!f||f->timing.action_ref!=id)fail("ExactNavalRepairSourceRequired",id);return *f;}
inline NewNavalRepairTimeSource2& fact(MilitaryActionTimeState& s,const StableId& id){auto* f=s.naval_repair_time_sources.find(id);if(!f||f->timing.action_ref!=id)fail("ExactNavalRepairSourceRequired",id);return *f;}
inline const NewNavalRepairTimeSource2& result_fact(const MilitaryActionTimeState& s,const StableId& id){const NewNavalRepairTimeSource2* found=nullptr;for(const auto& row:s.naval_repair_time_sources.rows())if(row.second.original_result&&row.second.original_result->id==id){if(found)fail("DuplicateExactNavalRepairResult",id);found=&row.second;}if(!found)fail("ExactNavalRepairResultRequired",id);return *found;}
inline WorldMoment due(const NewNavalRepairTimeSource2& f,ExactArithmeticBudget b){return take(add_world_duration(f.timing.started_at,HalfDayDuration{integer(f.original_terms.duration,b)},b));}
inline bool allocated(const StableId& id,const char* prefix){const std::string_view p(prefix);if(id.size()<=p.size()||id.compare(0,p.size(),p)!=0)return false;Integer ordinal=0;const auto* begin=id.data()+p.size();auto read=std::from_chars(begin,id.data()+id.size(),ordinal);return read.ec==std::errc{}&&read.ptr==id.data()+id.size()&&ordinal>0&&std::to_string(ordinal)==std::string_view(begin,id.size()-p.size());}
inline FixedTerminalDisposition disposition(const ExactNavalRepairResult2& r){return r.success?FixedTerminalDisposition::completed:r.reason=="NavalRepairCancelled"?FixedTerminalDisposition::cancelled:FixedTerminalDisposition::interrupted;}
inline const char* stop_reason(ExactNavalRepairInterruptionKind kind){switch(kind){case ExactNavalRepairInterruptionKind::contact:return "NavalRepairContact";case ExactNavalRepairInterruptionKind::crew_changed:return "NavalRepairCrewChanged";case ExactNavalRepairInterruptionKind::service_changed:return "NavalRepairServiceChanged";}fail("InvalidExactNavalRepairInterruption");}
inline ExactNavalRepairReceipt receipt(const NewNavalRepairTimeSource2& f,bool replay=false){const auto& t=f.timing;if(f.original_result){const auto& r=*f.original_result;if(!t.completion_or_interruption_at)fail("ExactNavalRepairResultMomentRequired",r.id);return {result_ref(r.id),t.action_ref,f.original_terms.actor_id,r.reason,*t.completion_or_interruption_at,true,r.success,replay};}return {start_ref(t.action_ref),t.action_ref,f.original_terms.actor_id,{},t.started_at,false,false,replay};}
void validate_source_shape(const CommonStore&,const NavalState&,const MilitaryActionTimeState&,const ExactNavalRepairSourceContext&,const NewNavalRepairTimeSource2&);
void validate_shape(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ExactNavalRepairReadContext&,const NewNavalRepairTimeSource2&);
Status validate_current_escrow(const CommonStore&,const NavalState&,const NewNavalRepairTimeSource2&);
Status validate_fact_history(const CommonStore&,const MilitaryState&,const NavalState&,const MilitaryActionTimeState&,const ExactNavalRepairReadContext&,const NewNavalRepairTimeSource2&,const ActionElapsedState&,bool);
}
