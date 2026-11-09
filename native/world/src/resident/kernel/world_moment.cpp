// S0精确数值和规范冷编码；32位limb及64位中间积不引入第三方依赖或浮点近似。
#include "hanmo/resident/world_moment.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
namespace hanmo::resident {
namespace {
// 内部异常仅终止当前纯运算，出口转原Outcome；不泄露半结果或业务成功。
struct ExactFailure { const char* code; ErrorKind kind; };
// 运算预算不写世界；按实际limb访问收费，超预算允许上层停住未发布候选。
struct Context {
    ExactArithmeticBudget budget;
    // 分配前验证所需工作区，输入长度不被当作数值合法性规则。
    void capacity(std::size_t size)const{if(size>budget.max_limbs)throw ExactFailure{"ExactArithmeticBudgetExceeded",ErrorKind::internal};}
    // 每轮耗尽前中止，避免对任意大输入无界占用计算时间。
    void spend(std::size_t steps=1){if(steps>budget.max_steps)throw ExactFailure{"ExactArithmeticBudgetExceeded",ErrorKind::internal};budget.max_steps-=steps;}
};
// 统一错误映射保技术失败和输入失败的区别，原始输入对象始终不变。
template<class T,class F> Outcome<T> guarded(ExactArithmeticBudget budget,F&& compute){
    try{ScopedCheckpointMemory memory(budget.checkpoint_memory?budget.checkpoint_memory:current_checkpoint_memory());Context context{budget};context.spend();return {true,compute(context),{}};}
    catch(const ExactFailure& failure){RuleError error;error.original={failure.code,"world_moment"};error.kind=failure.kind;return {false,{},std::move(error)};}
    catch(const std::bad_alloc&){RuleError error;error.original={"ExactArithmeticResourceExhausted","world_moment"};error.kind=ErrorKind::internal;return {false,{},std::move(error)};}
    catch(const std::length_error&){RuleError error;error.original={"ExactArithmeticResourceExhausted","world_moment"};error.kind=ErrorKind::internal;return {false,{},std::move(error)};}
}
// 原Integer不变；所有绝对时间范围检查延迟到最终写回。
constexpr auto tick_max=std::numeric_limits<Integer>::max();
constexpr auto tick_min=std::numeric_limits<Integer>::min();
}
// 本实现独占BigNat/Fraction的内部字段；公开类型不能构造非正规分数。
struct ExactArithmetic {
    static void retain(CheckpointRetentionCensus& census,const CanonicalFraction& value){census.vector(value.numerator_.limbs_);census.vector(value.denominator_.limbs_);}
    // 丢弃高位零，使相同自然数只有一个内存表示。
    static void trim(BigNat& a){while(!a.limbs_.empty()&&a.limbs_.back()==0)a.limbs_.pop_back();}
    // 小整数无损转limb，支持完整uint64自然数而不经过有符号窄化。
    static BigNat natural(Context& c,std::uint64_t value){BigNat out;c.capacity(value>UINT32_MAX?2:(value?1:0));if(value){out.limbs_.push_back(std::uint32_t(value));if(value>>32)out.limbs_.push_back(std::uint32_t(value>>32));}return out;}
    // 零检测不分配。
    static bool zero(const BigNat& a){return a.limbs_.empty();}
    // 比较先看位数，再高位到低位，不用溢出的整型交叉积。
    static int compare(Context& c,const BigNat& a,const BigNat& b){c.spend();c.capacity(a.limbs_.size());c.capacity(b.limbs_.size());if(a.limbs_.size()!=b.limbs_.size())return a.limbs_.size()<b.limbs_.size()?-1:1;for(auto i=a.limbs_.size();i>0;--i){c.spend();if(a.limbs_[i-1]!=b.limbs_[i-1])return a.limbs_[i-1]<b.limbs_[i-1]?-1:1;}return 0;}
    // 自然数加法保完整进位，工作区扩展先受预算约束。
    static BigNat add(Context& c,const BigNat& a,const BigNat& b){BigNat out;auto size=std::max(a.limbs_.size(),b.limbs_.size());c.capacity(size);out.limbs_.resize(size);std::uint64_t carry=0;for(std::size_t i=0;i<size;++i){c.spend();auto sum=carry+(i<a.limbs_.size()?a.limbs_[i]:0ULL)+(i<b.limbs_.size()?b.limbs_[i]:0ULL);out.limbs_[i]=std::uint32_t(sum);carry=sum>>32;}if(carry){c.capacity(size+1);out.limbs_.push_back(std::uint32_t(carry));}return out;}
    // 仅在a>=b时使用；负结果是调用顺序错误，不能截断。
    static BigNat subtract(Context& c,const BigNat& a,const BigNat& b){if(compare(c,a,b)<0)throw ExactFailure{"NegativeExactQuantity",ErrorKind::rule};BigNat out;c.capacity(a.limbs_.size());out.limbs_.resize(a.limbs_.size());std::uint64_t borrow=0;for(std::size_t i=0;i<a.limbs_.size();++i){c.spend();auto rhs=(i<b.limbs_.size()?std::uint64_t(b.limbs_[i]):0)+borrow;auto lhs=std::uint64_t(a.limbs_[i]);out.limbs_[i]=std::uint32_t(lhs-rhs);borrow=lhs<rhs;}trim(out);return out;}
    // 32x32乘积加目标limb及进位最大仍为UINT64_MAX，所有高位均保留。
    static BigNat multiply(Context& c,const BigNat& a,const BigNat& b){if(zero(a)||zero(b))return {};if(a.limbs_.size()>std::numeric_limits<std::size_t>::max()-b.limbs_.size())throw ExactFailure{"ExactArithmeticBudgetExceeded",ErrorKind::internal};auto size=a.limbs_.size()+b.limbs_.size();c.capacity(size);BigNat out;out.limbs_.resize(size);for(std::size_t i=0;i<a.limbs_.size();++i){std::uint64_t carry=0;for(std::size_t j=0;j<b.limbs_.size();++j){c.spend();auto value=std::uint64_t(a.limbs_[i])*b.limbs_[j]+out.limbs_[i+j]+carry;out.limbs_[i+j]=std::uint32_t(value);carry=value>>32;}out.limbs_[i+b.limbs_.size()]=std::uint32_t(carry);}trim(out);return out;}
    // 左移一位并加入最低位；用于有界二进制长除，不改变输入源。
    static void shift_bit(Context& c,BigNat& value,std::uint32_t bit){std::uint64_t carry=bit;for(auto& limb:value.limbs_){c.spend();auto next=(std::uint64_t(limb)<<1)|carry;limb=std::uint32_t(next);carry=next>>32;}if(carry){c.capacity(value.limbs_.size()+1);value.limbs_.push_back(std::uint32_t(carry));}}
    // 精确长除产生q和r；除零明确失败，分母无机器整数字长上限。
    static std::pair<BigNat,BigNat> divide(Context& c,const BigNat& a,const BigNat& b){if(zero(b))throw ExactFailure{"ZeroExactDenominator",ErrorKind::rule};if(compare(c,a,b)<0)return {{},a};BigNat quotient,remainder;c.capacity(a.limbs_.size());quotient.limbs_.resize(a.limbs_.size());for(auto i=a.limbs_.size();i>0;--i){for(int bit=31;bit>=0;--bit){c.spend();shift_bit(c,remainder,(a.limbs_[i-1]>>bit)&1U);if(compare(c,remainder,b)>=0){remainder=subtract(c,remainder,b);quotient.limbs_[i-1]|=std::uint32_t(1U)<<bit;}}}trim(quotient);return {std::move(quotient),std::move(remainder)};}
    // 非零数的尾零位只读真实limb，供二进制最大公约数消去公因子。
    static std::size_t trailing_bits(Context& c,const BigNat& value){std::size_t count=0;for(auto limb:value.limbs_){c.spend();if(limb==0){count+=32;continue;}while((limb&1U)==0){++count;limb>>=1;c.spend();}break;}return count;}
    // 精确除2的幂，不经除法大循环；仅用于已证尾零或gcd中间量。
    static void shift_right(Context& c,BigNat& value,std::size_t bits){auto words=bits/32;c.spend(value.limbs_.size());if(words)value.limbs_.erase(value.limbs_.begin(),value.limbs_.begin()+words);auto shift=unsigned(bits%32);if(shift){std::uint32_t carry=0;for(auto i=value.limbs_.size();i>0;--i){auto limb=value.limbs_[i-1];value.limbs_[i-1]=(limb>>shift)|carry;carry=limb<<(32-shift);}}trim(value);}
    // 二进制gcd避免每次余数都扫描完整大分子；所有步骤仍消费同一受信预算。
    static BigNat gcd(Context& c,BigNat a,BigNat b){if(zero(a))return b;if(zero(b))return a;auto left=trailing_bits(c,a),right=trailing_bits(c,b);auto common=std::min(left,right);shift_right(c,a,left);shift_right(c,b,right);for(;;){auto order=compare(c,a,b);if(order==0)break;if(order>0){a=subtract(c,a,b);shift_right(c,a,trailing_bits(c,a));}else{b=subtract(c,b,a);shift_right(c,b,trailing_bits(c,b));}}for(std::size_t i=0;i<common;++i){c.spend();shift_bit(c,a,0);}return a;}
    // 十进制词法严格规范，逐字实际计费，长输入失败是技术预算而非精度规则。
    static BigNat parse(Context& c,std::string_view value){if(value.empty()||(value.size()>1&&value.front()=='0'))throw ExactFailure{"InvalidExactNatural",ErrorKind::rule};BigNat out;auto ten=natural(c,10);for(char ch:value){c.spend();if(ch<'0'||ch>'9')throw ExactFailure{"InvalidExactNatural",ErrorKind::rule};out=add(c,multiply(c,out,ten),natural(c,std::uint64_t(ch-'0')));}return out;}
    // 按十进制单字除法输出规范字符串，零只有一种拼写。
    static std::string decimal(Context& c,const BigNat& source){c.capacity(source.limbs_.size());BigNat value=source;if(zero(value))return "0";std::string out;out.reserve(source.limbs_.size()*10);while(!zero(value)){std::uint64_t remainder=0;for(auto i=value.limbs_.size();i>0;--i){c.spend();auto current=(remainder<<32)|value.limbs_[i-1];value.limbs_[i-1]=std::uint32_t(current/10);remainder=current%10;}out.push_back(char('0'+remainder));trim(value);}std::reverse(out.begin(),out.end());return out;}
    // 由非负分子及正分母构造唯一最简形式。
    static CanonicalFraction fraction(Context& c,BigNat n,BigNat d){if(zero(d))throw ExactFailure{"ZeroExactDenominator",ErrorKind::rule};CanonicalFraction out;if(zero(n))return out;auto divisor=gcd(c,n,d);out.numerator_=divide(c,n,divisor).first;out.denominator_=divide(c,d,divisor).first;return out;}
    // 冷读先检查原数值是否已最简，不把不合规编码修成成功。
    static CanonicalFraction decode(Context& c,const EncodedFraction& wire){auto n=parse(c,wire.numerator),d=parse(c,wire.denominator);auto result=fraction(c,n,d);if(compare(c,n,result.numerator_)||compare(c,d,result.denominator_))throw ExactFailure{"NonCanonicalFraction",ErrorKind::rule};return result;}
    // 冷写只取正规内部字段，禁止JSON数值或浮点中转。
    static EncodedFraction encode(Context& c,const CanonicalFraction& f){EncodedFraction out;out.memory=CheckpointMemoryCharge(0);out.numerator=decimal(c,f.numerator_);out.denominator=decimal(c,f.denominator_);return out;}
    // 分数精确比较，交叉积使用可增长自然数。
    static int compare_fraction(Context& c,const CanonicalFraction& a,const CanonicalFraction& b){return compare(c,multiply(c,a.numerator_,b.denominator_),multiply(c,b.numerator_,a.denominator_));}
    // 分数加法后统一约分；技术切片不在中途floor。
    static CanonicalFraction add_fraction(Context& c,const CanonicalFraction& a,const CanonicalFraction& b){return fraction(c,add(c,multiply(c,a.numerator_,b.denominator_),multiply(c,b.numerator_,a.denominator_)),multiply(c,a.denominator_,b.denominator_));}
    // 非负量差先核顺序，再以同一精确分母求差；不在单位换算前舍入。
    static CanonicalFraction subtract_fraction(Context& c,const CanonicalFraction& a,const CanonicalFraction& b){if(compare_fraction(c,a,b)<0)throw ExactFailure{"NegativeWorkUnits",ErrorKind::rule};return fraction(c,subtract(c,multiply(c,a.numerator_,b.denominator_),multiply(c,b.numerator_,a.denominator_)),multiply(c,a.denominator_,b.denominator_));}
    // 原Integer投影的ceil明确检查范围，不把过大整数截断成负数。
    static Integer ceil_integer(Context& c,const CanonicalFraction& value){auto qr=divide(c,value.numerator_,value.denominator_);if(!zero(qr.second))qr.first=add(c,qr.first,natural(c,1));if(compare(c,qr.first,natural(c,std::uint64_t(tick_max)))>0)throw ExactFailure{"ExactIntegerProjectionOverflow",ErrorKind::rule};std::uint64_t result=0;for(auto i=qr.first.limbs_.size();i>0;--i)result=(result<<32)|qr.first.limbs_[i-1];return Integer(result);}
    // 乘除只走当前BigNat实现，除零与负量均保持显式失败。
    static CanonicalFraction product(Context& c,const CanonicalFraction& a,const CanonicalFraction& b,bool inverse){return inverse?fraction(c,multiply(c,a.numerator_,b.denominator_),multiply(c,a.denominator_,b.numerator_)):fraction(c,multiply(c,a.numerator_,b.numerator_),multiply(c,a.denominator_,b.denominator_));}
    // 投影范围在真实整商计算后核，不截断高位。
    static Integer project_integer(Context& c,const CanonicalFraction& f,bool rounded){auto qr=divide(c,f.numerator_,f.denominator_);if(rounded&&compare(c,add(c,qr.second,qr.second),f.denominator_)>=0)qr.first=add(c,qr.first,natural(c,1));if(compare(c,qr.first,natural(c,std::uint64_t(tick_max)))>0)throw ExactFailure{"ExactIntegerProjectionOverflow",ErrorKind::rule};std::uint64_t value=0;for(auto i=qr.first.limbs_.size();i>0;--i)value=(value<<32)|qr.first.limbs_[i-1];return Integer(value);}
    // 精确2幂缩放沿原limb位操作，有限double最大只需1074位。
    static BigNat shift_left(Context& c,BigNat value,std::size_t n){for(std::size_t i=0;i<n;++i){c.spend();shift_bit(c,value,0);}return value;}
    // binary64拆位读取normal/subnormal，零统一正零且不接受负数或非有限值。
    static CanonicalFraction from_binary64(Context& c,double value){if(!std::isfinite(value)||value<0)throw ExactFailure{"InvalidExactBinary64",ErrorKind::rule};std::uint64_t bits=0;std::memcpy(&bits,&value,8);auto exponent=int((bits>>52)&2047);auto significand=bits&((std::uint64_t(1)<<52)-1);if(exponent)significand|=std::uint64_t(1)<<52;if(!significand)return {};int power=exponent?exponent-1023-52:-1074;auto n=natural(c,significand),d=natural(c,1);if(power>=0)n=shift_left(c,std::move(n),std::size_t(power));else d=shift_left(c,std::move(d),std::size_t(-power));return fraction(c,std::move(n),std::move(d));}
    // 同分母整除后按remainder比较中点，奇偶仅取最低位；无双重舍入。
    static BigNat quotient_even(Context& c,const BigNat& n,const BigNat& d){auto qr=divide(c,n,d);auto order=compare(c,add(c,qr.second,qr.second),d);if(order>0||(order==0&&!zero(qr.first)&&(qr.first.limbs_.front()&1U)))qr.first=add(c,qr.first,natural(c,1));return qr.first;}
    // 只在已证<=53位后取uint64，拒绝隐藏窄化。
    static std::uint64_t bounded_u64(Context& c,const BigNat& value){if(value.limbs_.size()>2)throw ExactFailure{"ExactBinary64ConversionOverflow",ErrorKind::internal};std::uint64_t out=0;for(auto i=value.limbs_.size();i>0;--i){c.spend();out=(out<<32)|value.limbs_[i-1];}return out;}
    // 有理数先找真实二进指数，再一次舍入53位或subnormal的2^-1074格。
    static double to_binary64(Context& c,const CanonicalFraction& f){if(zero(f.numerator_))return 0.0;const auto& n=f.numerator_;const auto& d=f.denominator_;int exponent=0;if(compare(c,n,d)>=0){auto threshold=d;while(compare(c,n,shift_left(c,threshold,1))>=0){threshold=shift_left(c,std::move(threshold),1);if(++exponent>1023)throw ExactFailure{"ExactBinary64ConversionOverflow",ErrorKind::rule};}}else{auto scaled=n;while(compare(c,scaled,d)<0){scaled=shift_left(c,std::move(scaled),1);if(--exponent < -1075)return 0.0;}}
        std::uint64_t bits=0;if(exponent < -1022){auto mantissa=quotient_even(c,shift_left(c,n,1074),d);bits=bounded_u64(c,mantissa);}
        else{auto numerator=n,denominator=d;int shift=52-exponent;if(shift>=0)numerator=shift_left(c,std::move(numerator),std::size_t(shift));else denominator=shift_left(c,std::move(denominator),std::size_t(-shift));auto mantissa=bounded_u64(c,quotient_even(c,numerator,denominator));if(mantissa==(std::uint64_t(1)<<53)){mantissa>>=1;if(++exponent>1023)throw ExactFailure{"ExactBinary64ConversionOverflow",ErrorKind::rule};}bits=(std::uint64_t(exponent+1023)<<52)|(mantissa- (std::uint64_t(1)<<52));}double out=0;std::memcpy(&out,&bits,8);return out;}
    // 合法Moment余量必须小于一；无效输入不自动进位。
    static void valid(Context& c,const WorldMoment& moment){if(compare(c,moment.sub_half_day.numerator_,moment.sub_half_day.denominator_)>=0)throw ExactFailure{"NonCanonicalWorldMoment",ErrorKind::rule};}
    // 所有规范化用无符号偏置算tick进位，覆盖MIN到MAX完整区间且无有符号溢出。
    static WorldMoment normalize(Context& c,Integer tick,const CanonicalFraction& offset){auto qr=divide(c,offset.numerator_,offset.denominator_);const auto room=std::uint64_t(tick_max)-std::uint64_t(tick);if(compare(c,qr.first,natural(c,room))>0)throw ExactFailure{"WorldMomentTickOverflow",ErrorKind::rule};std::uint64_t carry=0;for(auto i=qr.first.limbs_.size();i>0;--i)carry=(carry<<32)|qr.first.limbs_[i-1];const auto sign=std::uint64_t(1)<<63;const auto biased=(std::uint64_t(tick)-std::uint64_t(tick_min))+carry;const auto result=biased<sign?tick_min+Integer(biased):Integer(biased-sign);return {result,fraction(c,std::move(qr.second),offset.denominator_)};}
    // 同解释域的Moment先比tick，再精确比较余量。
    static int compare_moment(Context& c,const WorldMoment& a,const WorldMoment& b){valid(c,a);valid(c,b);if(a.half_day_tick!=b.half_day_tick)return a.half_day_tick<b.half_day_tick?-1:1;return compare_fraction(c,a.sub_half_day,b.sub_half_day);}
    // 差在完整非负整差上结余量；MIN/MAX相减不经signed Integer。
    static HalfDayDuration elapsed(Context& c,const WorldMoment& from,const WorldMoment& to){if(compare_moment(c,from,to)>0)throw ExactFailure{"NegativeWorldDuration",ErrorKind::rule};auto ticks=natural(c,std::uint64_t(to.half_day_tick)-std::uint64_t(from.half_day_tick));const auto& a=from.sub_half_day;const auto& b=to.sub_half_day;auto denominator=multiply(c,a.denominator_,b.denominator_);auto numerator=add(c,multiply(c,ticks,denominator),multiply(c,b.numerator_,a.denominator_));return {fraction(c,subtract(c,numerator,multiply(c,a.numerator_,b.denominator_)),std::move(denominator))};}
    // 直接缩放原精确值，避免先编码再解析的冷边界往返及重复消耗预算。
    static CanonicalFraction scale_fraction(Context& c,const CanonicalFraction& value,Integer n,bool to_units){scale(n);c.capacity(value.numerator_.limbs_.size());c.capacity(value.denominator_.limbs_.size());auto factor=natural(c,std::uint64_t(n));if(to_units)return fraction(c,multiply(c,value.numerator_,factor),value.denominator_);return fraction(c,value.numerator_,multiply(c,value.denominator_,factor));}
    // N只用于单位换算，不能限制事件分母或代替原TimeScale来源校验。
    static void scale(Integer n){if(n<=0)throw ExactFailure{"InvalidTimeScale",ErrorKind::rule};}
};
// 外部构造允许输入未约分数值，出口只给规范拥有值。
Outcome<CanonicalFraction> normalize_fraction(std::string_view n,std::string_view d,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){auto numerator=ExactArithmetic::parse(c,n);auto denominator=ExactArithmetic::parse(c,d);return ExactArithmetic::fraction(c,std::move(numerator),std::move(denominator));});}
// 持久层入口严格保原字节的唯一性。
Outcome<CanonicalFraction> decode_canonical_fraction(const EncodedFraction& wire,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){return ExactArithmetic::decode(c,wire);});}
Outcome<EncodedFraction> encode_canonical_fraction(const CanonicalFraction& value,ExactArithmeticBudget budget){return guarded<EncodedFraction>(budget,[&](Context& c){return ExactArithmetic::encode(c,value);});}
Outcome<int> compare_fractions(const CanonicalFraction& a,const CanonicalFraction& b,ExactArithmeticBudget budget){return guarded<int>(budget,[&](Context& c){return ExactArithmetic::compare_fraction(c,a,b);});}
// 公共数值叶仍只返回拥有值，所有调用共用受信算术预算上下文。
Outcome<CanonicalFraction> add_fractions(const CanonicalFraction& a,const CanonicalFraction& b,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){return ExactArithmetic::add_fraction(c,a,b);});}
Outcome<CanonicalFraction> subtract_fractions(const CanonicalFraction& a,const CanonicalFraction& b,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){return ExactArithmetic::subtract_fraction(c,a,b);});}
Outcome<CanonicalFraction> multiply_fractions(const CanonicalFraction& a,const CanonicalFraction& b,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){return ExactArithmetic::product(c,a,b,false);});}
Outcome<CanonicalFraction> divide_fractions(const CanonicalFraction& a,const CanonicalFraction& b,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){return ExactArithmetic::product(c,a,b,true);});}
Outcome<CanonicalFraction> fraction_from_binary64(double value,ExactArithmeticBudget budget){return guarded<CanonicalFraction>(budget,[&](Context& c){return ExactArithmetic::from_binary64(c,value);});}
Outcome<double> fraction_to_binary64(const CanonicalFraction& value,ExactArithmeticBudget budget){return guarded<double>(budget,[&](Context& c){return ExactArithmetic::to_binary64(c,value);});}
Outcome<Integer> floor_fraction(const CanonicalFraction& value,ExactArithmeticBudget budget){return guarded<Integer>(budget,[&](Context& c){return ExactArithmetic::project_integer(c,value,false);});}
Outcome<Integer> round_fraction_half_away(const CanonicalFraction& value,ExactArithmeticBudget budget){return guarded<Integer>(budget,[&](Context& c){return ExactArithmetic::project_integer(c,value,true);});}
// 纯规范化不读Common，也不修改调用者的tick。
Outcome<WorldMoment> normalize_world_moment(Integer tick,const CanonicalFraction& offset,ExactArithmeticBudget budget){return guarded<WorldMoment>(budget,[&](Context& c){return ExactArithmetic::normalize(c,tick,offset);});}
Outcome<int> compare_world_moments(const WorldMoment& a,const WorldMoment& b,ExactArithmeticBudget budget){return guarded<int>(budget,[&](Context& c){return ExactArithmetic::compare_moment(c,a,b);});}
Outcome<WorldMoment> add_world_duration(const WorldMoment& at,const HalfDayDuration& elapsed,ExactArithmeticBudget budget){return guarded<WorldMoment>(budget,[&](Context& c){ExactArithmetic::valid(c,at);return ExactArithmetic::normalize(c,at.half_day_tick,ExactArithmetic::add_fraction(c,at.sub_half_day,elapsed.value));});}
Outcome<HalfDayDuration> elapsed_world_duration(const WorldMoment& from,const WorldMoment& to,ExactArithmeticBudget budget){return guarded<HalfDayDuration>(budget,[&](Context& c){return ExactArithmetic::elapsed(c,from,to);});}
// 两个显式换算出口各返回不同强量纲类型。
Outcome<HalfDayDuration> scale_units_to_duration(const WorkUnits& units,Integer n,ExactArithmeticBudget budget){return guarded<HalfDayDuration>(budget,[&](Context& c){return HalfDayDuration{ExactArithmetic::scale_fraction(c,units.value,n,false)};});}
Outcome<WorkUnits> duration_to_scale_units(const HalfDayDuration& duration,Integer n,ExactArithmeticBudget budget){return guarded<WorkUnits>(budget,[&](Context& c){return WorkUnits{ExactArithmetic::scale_fraction(c,duration.value,n,true)};});}
// 量纲内算术保持一份credited，不能由两条writer分别累计工量和半日。
Outcome<WorkUnits> add_work_units(const WorkUnits& a,const WorkUnits& b,ExactArithmeticBudget budget){return guarded<WorkUnits>(budget,[&](Context& c){return WorkUnits{ExactArithmetic::add_fraction(c,a.value,b.value)};});}
Outcome<WorkUnits> subtract_work_units(const WorkUnits& a,const WorkUnits& b,ExactArithmeticBudget budget){return guarded<WorkUnits>(budget,[&](Context& c){return WorkUnits{ExactArithmetic::subtract_fraction(c,a.value,b.value)};});}
Outcome<WorkUnits> fixed_duration_work_units(Integer half_days,Integer n,ExactArithmeticBudget budget){return guarded<WorkUnits>(budget,[&](Context& c){ExactArithmetic::scale(n);if(half_days<=0)throw ExactFailure{"InvalidFixedDuration",ErrorKind::rule};if(half_days>tick_max/n)throw ExactFailure{"FixedDurationWorkUnitsOverflow",ErrorKind::rule};return WorkUnits{ExactArithmetic::fraction(c,ExactArithmetic::natural(c,std::uint64_t(half_days*n)),ExactArithmetic::natural(c,1))};});}
Outcome<Integer> ceil_half_day_duration(const HalfDayDuration& duration,ExactArithmeticBudget budget){return guarded<Integer>(budget,[&](Context& c){return ExactArithmetic::ceil_integer(c,duration.value);});}
// 正时间片另核严格大于，不把同刻零时事件伪装成正Advance。
Outcome<MomentSpan> make_moment_span(const WorldMoment& from,const WorldMoment& to,ExactArithmeticBudget budget){return guarded<MomentSpan>(budget,[&](Context& c){if(ExactArithmetic::compare_moment(c,from,to)>=0)throw ExactFailure{"NonPositiveMomentSpan",ErrorKind::rule};return MomentSpan{from,to};});}
// 旧offset必须处于原[0,N)，嵌入时精确除N，不猜失落来源或强制迁移。
Outcome<WorldMoment> embed_legacy_world_moment(Integer tick,Integer offset,Integer n,ExactArithmeticBudget budget){return guarded<WorldMoment>(budget,[&](Context& c){ExactArithmetic::scale(n);if(offset<0||offset>=n)throw ExactFailure{"InvalidLegacyWorldOffset",ErrorKind::rule};return WorldMoment{tick,ExactArithmetic::fraction(c,ExactArithmetic::natural(c,std::uint64_t(offset)),ExactArithmetic::natural(c,std::uint64_t(n)))};});}
// 原子解码到拥有值，全部校验成功才返回，不接触原世界或输出部分Moment。
Outcome<WorldMoment> decode_world_moment(const EncodedWorldMoment& wire,ExactArithmeticBudget budget){return guarded<WorldMoment>(budget,[&](Context& c){WorldMoment out{wire.half_day_tick,ExactArithmetic::decode(c,wire.sub_half_day)};ExactArithmetic::valid(c,out);return out;});}
Outcome<EncodedWorldMoment> encode_world_moment(const WorldMoment& value,ExactArithmeticBudget budget){return guarded<EncodedWorldMoment>(budget,[&](Context& c){ExactArithmetic::valid(c,value);return EncodedWorldMoment{value.half_day_tick,ExactArithmetic::encode(c,value.sub_half_day)};});}
void retain_canonical_fraction(CheckpointRetentionCensus& census,const CanonicalFraction& value){ExactArithmetic::retain(census,value);}

} // 运行时接线、权限和真实期间来源继续由原phase/run/owner负责。
