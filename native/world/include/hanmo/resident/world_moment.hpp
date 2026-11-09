// WorldMoment/2唯一精确数值叶；不保存current、不推进世界，也不替领域生成工作成果。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <cstddef>
#include <string_view>
namespace hanmo::resident {
struct ExactArithmetic;
class CheckpointRetentionCensus;
// 调用方受信计算预算；耗尽为技术失败，可提高预算重试，绝非合法分母的玩法上界。
struct ExactArithmeticBudget { std::size_t max_limbs=4096; std::size_t max_steps=10000000; std::shared_ptr<CheckpointMemoryResource> checkpoint_memory{}; };
// 非负可增长整数，内部为最小32位小端limb；只由同一精确叶构造和运算。
class BigNat {
public:
    BigNat()=default;
private:
    CheckpointVector<std::uint32_t> limbs_;
    explicit BigNat(std::uint32_t value):limbs_(value?CheckpointVector<std::uint32_t>{value}:CheckpointVector<std::uint32_t>{}){}
    friend class CanonicalFraction;
    friend struct ExactArithmetic;
};
// 不可直接写分子/分母；正规零为0/1，其余最简非负有理数可大于等于1。
class CanonicalFraction {
public:
    CanonicalFraction()=default;
    // 值复制也用于右值，防止被移动后的分母变零而破坏数值不变量。
    CanonicalFraction(const CanonicalFraction&)=default;
    // 临时复制可能抛出，赋值不得标noexcept；完整复制成功后交换，保原值并允许聚合上层捕获。
    CanonicalFraction& operator=(const CanonicalFraction& other){CanonicalFraction temporary(other);swap(temporary);return *this;}
    // 无分配交换是唯一noexcept步骤，不误导WorldMoment等聚合赋值的异常规格。
    void swap(CanonicalFraction& other)noexcept{numerator_.limbs_.swap(other.numerator_.limbs_);denominator_.limbs_.swap(other.denominator_.limbs_);}
private:
    BigNat numerator_,denominator_{1};
    friend struct ExactArithmetic;
};
// 仅遍历原BigNat真实allocator owner；不生成十进制临时，也不改变精确数值。
void retain_canonical_fraction(CheckpointRetentionCensus&,const CanonicalFraction&);
// 半日和原TimeScale工时不可隐式互换；日历tick与个人战local round继续沿原类型。
struct HalfDayDuration { CanonicalFraction value; };
struct WorkUnits { CanonicalFraction value; };
// tick沿原Integer范围；世界自己的合法日历范围仍由原世界validator核验。
struct WorldMoment { Integer half_day_tick=0; CanonicalFraction sub_half_day; };
struct MomentSpan { WorldMoment from,to; };
// 持久分数只使用规范十进制字符串；此冷值供原owner codec复用，不另设存档段或世界状态。
struct EncodedFraction {
    CheckpointMemoryCharge memory;
    std::string numerator,denominator;
    EncodedFraction()=default;
    EncodedFraction(std::string_view n,std::string_view d):memory(n.size()+d.size()+2),numerator(n),denominator(d){}
    EncodedFraction(const EncodedFraction& other):memory(other.memory),numerator(other.numerator),denominator(other.denominator){}
    EncodedFraction(EncodedFraction&&)=default;
    EncodedFraction& operator=(EncodedFraction other)noexcept{swap(other);return *this;}
    void swap(EncodedFraction& other)noexcept{memory.swap(other.memory);numerator.swap(other.numerator);denominator.swap(other.denominator);}
};
struct EncodedWorldMoment { Integer half_day_tick=0; EncodedFraction sub_half_day; };

// 构造入口会约分；词法拒绝符号、前导零、空白、小数与指数，不偷偷近似。
Outcome<CanonicalFraction> normalize_fraction(std::string_view numerator,std::string_view denominator,ExactArithmeticBudget budget={});
// 冷读拒绝非正规原件，不能通过约分给同一值制造多个存档签名。
Outcome<CanonicalFraction> decode_canonical_fraction(const EncodedFraction&,ExactArithmeticBudget budget={});
Outcome<EncodedFraction> encode_canonical_fraction(const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<int> compare_fractions(const CanonicalFraction&,const CanonicalFraction&,ExactArithmeticBudget budget={});
// 非负精确原数值运算，调用者持有领域量纲；不以技术片数重置预算。
Outcome<CanonicalFraction> add_fractions(const CanonicalFraction&,const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<CanonicalFraction> subtract_fractions(const CanonicalFraction&,const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<CanonicalFraction> multiply_fractions(const CanonicalFraction&,const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<CanonicalFraction> divide_fractions(const CanonicalFraction&,const CanonicalFraction&,ExactArithmeticBudget budget={});
// 空间有限binary64的绝对值无损转dyadic；符号由空间有符号坐标型单独保留。
Outcome<CanonicalFraction> fraction_from_binary64(double,ExactArithmeticBudget budget={});
// 唯一round-to-nearest-ties-to-even转换；不经十进制或两个近似double相除。
Outcome<double> fraction_to_binary64(const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<Integer> floor_fraction(const CanonicalFraction&,ExactArithmeticBudget budget={});
// 非负数half-away-from-zero，沿原接触量化的明确规则。
Outcome<Integer> round_fraction_half_away(const CanonicalFraction&,ExactArithmeticBudget budget={});
// 规范化允许非负余量进位；只有最终tick写回超原Integer界才报溢出。
Outcome<WorldMoment> normalize_world_moment(Integer tick,const CanonicalFraction& offset,ExactArithmeticBudget budget={});
// 调用方必须保证同world/time-profile解释域；值本身不持身份或第二个current。
Outcome<int> compare_world_moments(const WorldMoment&,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<WorldMoment> add_world_duration(const WorldMoment&,const HalfDayDuration&,ExactArithmeticBudget budget={});
// to等于from得到零；to更早明确失败，不能饱和成零。
Outcome<HalfDayDuration> elapsed_world_duration(const WorldMoment& from,const WorldMoment& to,ExactArithmeticBudget budget={});
Outcome<HalfDayDuration> scale_units_to_duration(const WorkUnits&,Integer units_per_half_day,ExactArithmeticBudget budget={});
Outcome<WorkUnits> duration_to_scale_units(const HalfDayDuration&,Integer units_per_half_day,ExactArithmeticBudget budget={});
// 工作量只在自身量纲内加减；减成负数明确失败，不借Moment伪装工量或截零。
Outcome<WorkUnits> add_work_units(const WorkUnits&,const WorkUnits&,ExactArithmeticBudget budget={});
Outcome<WorkUnits> subtract_work_units(const WorkUnits&,const WorkUnits&,ExactArithmeticBudget budget={});
// 固定H沿原整数范围checked乘N；此入口不接受负数，也不改变已锁TimeScale。
Outcome<WorkUnits> fixed_duration_work_units(Integer half_days,Integer units_per_half_day,ExactArithmeticBudget budget={});
// 仅供原Common.remaining_ticks兼容投影；上取整若超Integer失败，业务timer仍读精确量。
Outcome<Integer> ceil_half_day_duration(const HalfDayDuration&,ExactArithmeticBudget budget={});
Outcome<MomentSpan> make_moment_span(const WorldMoment& from,const WorldMoment& to,ExactArithmeticBudget budget={});
// 仅数值嵌入已证旧N及合法offset；调用者另证原档时相与全部真实执行来源，不推断迁移资格。
Outcome<WorldMoment> embed_legacy_world_moment(Integer tick,Integer offset,Integer units_per_half_day,ExactArithmeticBudget budget={});
// Moment冷读/写都拒绝余量>=1，不把坏档或坏候选静默正规化成下一刻。
Outcome<WorldMoment> decode_world_moment(const EncodedWorldMoment&,ExactArithmeticBudget budget={});
Outcome<EncodedWorldMoment> encode_world_moment(const WorldMoment&,ExactArithmeticBudget budget={});
} // 精确运算只有拥有值，无全局clock、ID、随机或领域副作用。
