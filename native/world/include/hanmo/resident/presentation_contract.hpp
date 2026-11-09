// P2有限表现查询的只读来源；只解析调用方已选中的可见人物，不授予额外可见权限。
#pragma once
#include "model_contract.hpp"
namespace hanmo::resident {
class PresentationRead {
public:
    virtual ~PresentationRead()=default;
    // 复用ActorIdentity.definition与原sidebar._name顺序；合法空作者文本仍为空。
    // 无效/不存在身份沿原texts[空键]或空字符串回退，不擅自返回ID/未知标签。
    virtual Outcome<std::string> display_name(const StableId& actor_id) const=0;
    // 只解析当前对白/选项实际选中的作者文本；缺键明确失败，绝不回整份texts。
    virtual Outcome<std::string> text(const StableId& text_key) const=0;
};
} // 实例只借用同一候选CommonState和不可变catalog；clone/restore后必须重新绑定。
