// 新课程的先天来源是永久类型，World读戳仅用于临时报价。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
// 中央预留，只在显式family_innate_learning/1启用，旧Growth字节不追加。
constexpr std::uint32_t innate_study_checkpoint_feature=131072u;
// 永久课程来源，不保存会随命令变化的World revision；读戳另由临时Read持有。
struct InnateStudySource {
    StableId actor_id,skill_id,origin_kind,origin_fact_id;
    Integer observed_tick=0,bonus_bp=0;
    OrderedTable<Integer> known_levels;
    FieldOrder field_order;
};
struct InnateStudyRead { WorldIdentity source_identity;InnateStudySource source; };
// 等值比较保留未知与已知0的区别，不要求JSON字段表现顺序相同。
bool innate_study_source_equal(const InnateStudySource&,const InnateStudySource&);
Outcome<Integer> innate_study_lesson_xp(Integer original_base_xp,Integer bonus_bp);
}
