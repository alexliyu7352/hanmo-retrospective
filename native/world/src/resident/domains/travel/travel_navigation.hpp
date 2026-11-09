// 场所导航只给原旅行命令的下一步；到任、接待、费用与世界提交仍由各原业务负责。
#pragma once
#include "travel.hpp"
namespace hanmo::resident {
using TravelNextCommand=std::variant<MoveLocalCommand,TravelCommand>;
// 无command表示已经在目标场所；只有跨城travel带原plan_travel报价，绝不保存第二行程。
struct TravelNextStep { StableId target_place_id;std::optional<TravelNextCommand> command;std::optional<TravelQuote> travel_quote; };
// 调用者须持有真实已知目标来源；每次仅返回当前状态下的一步，提交前仍走唯一preview/command。
Outcome<TravelNextStep> next_travel_command(const CommonRead&,const TravelState&,const TravelCatalog&,const TravelServices&,const StableId& actor_id,const StableId& target_place_id);
}
