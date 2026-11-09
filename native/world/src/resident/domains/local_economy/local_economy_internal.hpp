// 地方经济纯算术与引用检查；溢出是失败，不把数量截断后提交。
#pragma once
#include "local_economy_model.hpp"
#include <limits>
#include <set>
namespace hanmo::resident::local_economy_detail {
inline bool add(Integer a,Integer b,Integer& out){return !__builtin_add_overflow(a,b,&out);}
inline bool multiply(Integer a,Integer b,Integer& out){return !__builtin_mul_overflow(a,b,&out);}
inline bool ratio(Integer value,Integer numerator,Integer denominator,Integer remainder,Integer& quotient,Integer& next){Integer n;if(value<0||numerator<0||denominator<=0||remainder<0||remainder>=denominator||!multiply(value,numerator,n)||!add(n,remainder,n))return false;quotient=n/denominator;next=n%denominator;return true;}
inline bool has(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
inline bool same_subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
inline bool interval(Integer a,Integer b,Integer c,Integer d){return a<=d&&c<=b;}
inline Status failure(const StableId& code,const StableId& id={}){return common_failure(code,id);}
// 当期匿名存量来自 cohort，具名人物只能按真实存活记录计一个人。
inline Outcome<Integer> population(const CommonStore& common,const LocalEconomyState& state,const SubjectRef& subject){
    if(subject.kind==SubjectKind::cohort){const auto* p=state.cohorts.find(subject.id);if(!p)return {false,0,failure("UnknownPopulationCohort",subject.id).error};return {true,p->headcount,{}};}
    if(subject.kind==SubjectKind::actor){const auto* a=common.actor(subject.id);if(!a)return {false,0,failure("UnknownActor",subject.id).error};return {true,a->alive?1:0,{}};}
    return {false,0,failure("InvalidPopulationSubject",subject.id).error};
}
// 仅供本域已验来源入口调用；外部普通转移仍拒绝军事身份。
// The original reducer is shared by the writer and cold source replay.
Status reduce_original_population_labor_release(OrderedTable<LaborAllocation>&,
 const StableId& cohort,Integer remaining_capacity,Integer effective_tick,
 const std::vector<StableId>& released_allocations);
Outcome<StableId> transfer_population_counts(CommonStore&,LocalEconomyState&,const PopulationTransferRequest&,PopulationTransferLaborCapture* actual_labor_capture=nullptr);
// 原最早欠期偿付实现供任职终态复用；调用者拥有共同保存点。
Status settle_obligation_arrears(CommonStore&,LocalEconomyState&,const OrganizationRead&,const RecurringObligation&,Integer,LocalEconomyEffects&);
// 跨域只读适配器借用原状态对象；写入同一对象才能与已追加的Common事实保持一致。
// 护栏只保存回滚副本，不改变被借对象地址；失败返回后不再使用其旧表元素指针。
template<class State> class CandidateStateGuard {
public:
    explicit CandidateStateGuard(State& state):state_(state),before_(state){}
    CandidateStateGuard(const CandidateStateGuard&)=delete;
    CandidateStateGuard& operator=(const CandidateStateGuard&)=delete;
    ~CandidateStateGuard(){if(!committed_)state_=std::move(before_);}
    void commit(){committed_=true;}
private:
    State& state_;State before_;bool committed_=false;
};
// 宏观阶段具有自身保存点；外层提交失败时，中央仍负责所有领域一起回滚。
class PhaseSavepoint {
public:
    explicit PhaseSavepoint(CommonStore& common):common_(common),token_(common.begin_savepoint()){}
    ~PhaseSavepoint(){if(token_.ok&&!finished_)common_.rollback_savepoint(token_.value);}
    const Outcome<Integer>& token() const{return token_;}
    Status commit(){if(!token_.ok)return {false,{},token_.error};auto result=common_.commit_savepoint(token_.value);if(result.ok)finished_=true;return result;}
private:
    CommonStore& common_;Outcome<Integer> token_;bool finished_=false;
};
} // 辅助函数不拥有钱货、随机流或候选外可变状态。
