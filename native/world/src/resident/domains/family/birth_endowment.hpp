// 出生禀赋内核只计算有来源的概率和原结果，不创建人物、不抽随机、不改后天技艺。
#pragma once
#include "resident/model/common_model.hpp"
#include <array>
namespace hanmo::resident {
struct BoundaryValue;
struct InnateFamilyDefinition {
    StableId id;
    std::vector<StableId> learning_skill_ids;
    std::array<Integer,4> learning_bonus_bp{{0,500,1000,1500}};
};
// 三项依次是不继承、保留最高已知档、强化一档；所有数值均为明确游戏参数。
struct InnateInheritanceWeights { Integer absent=0,retained=0,strengthened=0; };
struct BirthEndowmentPolicy {
    StableId version="innate_birth_v1";
    std::array<Integer,4> unweighted{{9500,500,0,0}};
    InnateInheritanceWeights single{5500,4000,500},different{3500,5500,1000},same{2000,6500,1500};
    OrderedTable<InnateFamilyDefinition> families;
};
// 某位已知生物亲代也可能没有该族资料；null不是0，未知亲代不能带一个推测档位。
struct InnateParentSource { std::optional<StableId> actor_id;std::optional<Integer> level;FieldOrder field_order; };
struct InnateDistribution {
    StableId branch;
    std::array<Integer,4> basis_points{};
    bool both_parent_levels_known=false;
};
// 只保存本次出生原抽样，0..9999区间与分布一一确定；相同记录可无限验证而不重抽。
struct BirthInnateResult {
    StableId family_id,policy_version;
    InnateParentSource first,second;
    StableId branch;
    Integer draw=0,level=0;
    FieldOrder field_order;
};
Status validate_birth_endowment_policy(const BirthEndowmentPolicy&,bool extended_skills);
Outcome<InnateDistribution> preview_birth_innate_distribution(const BirthEndowmentPolicy&,const StableId& family_id,const InnateParentSource&,const InnateParentSource&);
Outcome<BirthInnateResult> resolve_birth_innate_draw(const BirthEndowmentPolicy&,const StableId& family_id,const InnateParentSource&,const InnateParentSource&,Integer original_draw);
Status validate_birth_innate_result(const BirthEndowmentPolicy&,const BirthInnateResult&);
// 学习只累计该技能已声明的先天效果，所有族合计最多15%，不包含手动表现倍率。
Outcome<Integer> birth_innate_learning_bonus(const BirthEndowmentPolicy&,const OrderedTable<Integer>& known_levels,const StableId& skill_id);
Outcome<BirthInnateResult> decode_birth_innate_result(const BoundaryValue&);
BoundaryValue encode_birth_innate_result(const BirthInnateResult&);
Status visit_birth_innate_result(CodecVisitor&,BirthInnateResult&);
}
