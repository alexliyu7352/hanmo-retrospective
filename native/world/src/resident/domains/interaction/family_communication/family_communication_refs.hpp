// 原 B 的唯一事实地址；轻头供本人命令回执使用，不包含世界根或可写权限。
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
// 原表达唯一 Common 地址；不是 B 的命令结果，也不保存第二份表达。
struct FamilyExpressionRef2 {
  SourceRef exact_source_ref;
  Integer source_schema = 2;
};

struct FamilyPostRef1 {
  StableId post_fact_id;
};
} // namespace hanmo::resident
