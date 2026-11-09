// 原来源引用只保领域、具名kind和实际ID；schema由使用者独立声明，不构造第二事实账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <string_view>
namespace hanmo::resident {
struct SourceRef { StableId domain,kind,id; };
// Borrowed first-copy input only; not a new durable source or wire schema.
struct SourceRefBuildView { std::string_view domain,kind,id; };
// 比较只用于同一受信来源注册表中的键，不以字典顺序推断实际发生顺序。
inline bool same_source_ref(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
inline bool valid_source_ref(const SourceRef& a){return !a.domain.empty()&&!a.kind.empty()&&!a.id.empty();}
}
