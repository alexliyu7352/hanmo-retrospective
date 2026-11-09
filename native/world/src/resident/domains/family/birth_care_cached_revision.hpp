// 原出生/照护来源已永久保存写入revision；冷缓存再核其唯一实际提交，不新增存档字段。
#pragma once
#include "family_birth.hpp"
#include "child_care_relief.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident::birth_care_cache {
// 只接受原cache对象；一条提交只分配一次revision，同tick较早来源不能充当后来请求的回执。
inline Status actual_revision(const CommonStore& c,const CommitResult& result,Integer source_revision,const StableId& source){
 if(source_revision<0||result.revision<1||result.revision>c.revision()||source_revision!=result.revision-1)return common_failure("BirthCareCachedRevisionMismatch",source);Integer objects=0;for(const auto& row:c.state().results.rows())if(row.second){if(row.second.get()==&result)++objects;else if(row.second->revision==result.revision||row.second->result_id==result.result_id)return common_failure("BirthCareCachedResultAmbiguous",source);}return objects==1?common_success():common_failure("BirthCareActualCachedResultRequired",source);
}
// 完整Birth业务史与本次具体回执形状必须已经核准；这里只消费该原载荷的revision，不推断业务种类。
inline Status birth_revision(const CommonStore& c,const FamilyBirthState& state,const CommitResult& result,const StableId& source){
 Integer count=0,revision=-1;if(const auto* p=state.proposals.find(source)){++count;revision=p->source.revision;}if(const auto* p=state.replies.find(source)){++count;revision=p->revision;}if(const auto* p=state.closures.find(source)){++count;revision=p->revision;}if(const auto* p=state.care_starts.find(source)){++count;revision=p->revision;}if(const auto* p=state.care.find(source)){++count;revision=p->revision;}return count==1?actual_revision(c,result,revision,source):common_failure("BirthCareCachedSourceRequired",source);
}
// 接手照护保原逐源一次cache门和Advance多完成；此门只补原永久revision对应关系。
inline Status relief_revision(const CommonStore& c,const ChildCareReliefState& state,const CommitResult& result,const StableId& source){
 Integer count=0,revision=-1;if(const auto* p=state.agreements.find(source)){++count;revision=p->source.revision;}if(const auto* p=state.closures.find(source)){++count;revision=p->revision;}if(const auto* p=state.starts.find(source)){++count;revision=p->revision;}if(const auto* p=state.care.find(source)){++count;revision=p->revision;}return count==1?actual_revision(c,result,revision,source):common_failure("BirthCareCachedSourceRequired",source);
}
}
