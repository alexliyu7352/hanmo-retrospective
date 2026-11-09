#pragma once
#include "work_result_source.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/model/commitment_ref_codec.hpp"
namespace hanmo::resident::work_source_codec {
// 跨费用、目标验工与返报共同使用原 Work 来源字段，保持已选旧布局。
template<class A>Status source(A& a, SourceRef& v) {
 a.field("domain",v.domain); a.field("kind",v.kind); return a.field("id",v.id);
}
template<class A>Status authorization(A& a, WorkAuthorizationRef& v) {
 a.object("program_ref",[&](auto& c){return source(c,v.program_ref);});
 a.field("target_line_id",v.target_line_id);
 a.object("terms_revision",[&](auto& c){return commitment_codec::terms(c,v.terms_revision);});
 return a.field("accepted_task_id",v.accepted_task_id);
}
template<class A>Status result(A& a, WorkResultSource& v) {
 Integer tag=v.index(); a.field("kind",tag);
 if(!a.status().ok)return a.status();
 if(tag<0||tag>2)return a.reject("UnknownWorkResultSource");
 if(a.reading()) {
  if(tag==0){if(!prepare_checkpoint_default_values<LegacyAppeaseWork>(a).ok)return a.status();v=LegacyAppeaseWork{};}
  else if(tag==1){if(!prepare_checkpoint_default_values<PatrolResult>(a).ok)return a.status();v=PatrolResult{};}
  else {if(!prepare_checkpoint_default_values<SupervisedResult>(a).ok)return a.status();v=SupervisedResult{};}
 }
 return std::visit([&](auto& x){
  using T=std::decay_t<decltype(x)>;
  if constexpr(std::is_same_v<T,LegacyAppeaseWork>)return a.field("work_completion_fact_id",x.work_completion_fact_id);
  else return a.field("fact_id",x.fact_id);
 },v);
}
}
