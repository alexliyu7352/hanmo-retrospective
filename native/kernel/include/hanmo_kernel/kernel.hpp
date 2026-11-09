// 内核本体（设计第 0、4、14 节）：能不接游戏本体独立运行和测试。包、心智、反射、记录在开局前装上。
// 半日推进的顺序：过程到期 → 拿主意（并行）→ 按固定顺序执行、感知分发 → 闲谈 → 身体 → 遗忘 → 检查 → 存档。
#pragma once

#include <memory>

#include "mind.hpp"
#include "package.hpp"

namespace hanmo::kernel {

struct Config {
  const char *content_dir = nullptr;  // 开局内容：地方、道路、人、物品种类、招、开局教养
  uint64_t seed = 0;                  // 世界种子：同一种子、同一输入，结果相同
};

// 内置宏观统计：判断世界宏观上像不像，沙盒的里程碑也用它。包的统计走包的公开查询。
struct MacroStats {
  HalfDay now = 0;
  uint64_t people = 0;    // 人数合计（含群体）
  uint64_t named = 0;     // 具名人物
  uint64_t deaths = 0;    // 本半日死亡
  uint64_t hungry = 0;    // 饥饿的人数
  uint64_t facts = 0;     // 本半日产生的事实
  uint64_t memories = 0;  // 全部记忆条数
};

class Kernel {
 public:
  static std::unique_ptr<Kernel> create(const Config &);
  virtual ~Kernel() = default;

  // 开局前装上。清单的接口版本不符的包不装，返回假。
  virtual bool install(std::unique_ptr<Package>) = 0;
  // 指定某人用哪个心智；who 为 0 表示默认。
  virtual void set_mind(ActorId who, std::shared_ptr<const Mind>) = 0;
  virtual void add_reflex(std::shared_ptr<const Reflex>) = 0;
  // 决策记录；为空时不记录。
  virtual void set_recorder(DecisionRecorder *) = 0;

  // 读内容、开局、调各包的 start。失败返回假。
  virtual bool start() = 0;
  // 玩家的命令，与 NPC、包走同一个口。
  virtual Outcome submit(const Action &) = 0;
  virtual PreviewResult preview(const Action &) const = 0;
  // 推进若干个半日。半日检查发现问题时停下，返回已推进的半日数。
  virtual uint32_t advance(uint32_t half_days) = 0;

  // 在半日边界存档：内核的段加各包的段。
  virtual bool save(const char *path) const = 0;
  // 坏档或版本不符就拒绝，旧世界不动。
  virtual bool load(const char *path) = 0;

  // 真值（包、调试用）。
  virtual const WorldView &world() const = 0;
  virtual const FactView &facts() const = 0;
  // 某人所知（玩家界面看主角，心智看自己）。
  virtual std::unique_ptr<KnownView> known(ActorId) const = 0;

  virtual MacroStats stats() const = 0;
  // 全世界状态的摘要：同一种子、同一输入必须相同。
  virtual uint64_t digest() const = 0;
};

}  // namespace hanmo::kernel
