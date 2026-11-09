// 有限表现来源只借当前候选与只读目录；不枚举人物，不授予可见性，不泄漏内部引用。
#pragma once
#include "hanmo/resident/presentation_contract.hpp"
#include "../model/common_model.hpp"

namespace hanmo::resident {
// 每次查询绑定同一候选；clone/restore必须构造新读取器，返回字符串独立拥有。
class CommonPresentation final : public PresentationRead {
public:
    CommonPresentation(const CommonState& state,const CommonCatalog& catalog):state_(state),catalog_(catalog){}
    // 按Identity.definition和sidebar._name解析调用方已经筛选的一个人物。
    Outcome<std::string> display_name(const StableId& actor_id) const override;
    // 已选对白/选项的作者文本缺失视为明确内部读错误；合法空文本成功返回。
    Outcome<std::string> text(const StableId& text_key) const override;
private:
    const CommonState& state_;
    const CommonCatalog& catalog_;
};
} // 实例不可越过所属候选生命周期；DTO只复制有限的最终展示结果。
