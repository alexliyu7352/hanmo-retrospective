// 只投影调用者已选中的一个人物；公开值深拥有，不带世界、seed、亲代或可写资源引用。
#pragma once
#include "../model/common_model.hpp"
#include "../checkpoint/ordered_value.hpp"

namespace hanmo::resident {
// 六个原character_input人物字段；空作者名仍是有效名字，未知年龄明确为-1。
struct CharacterActorView {
    StableId actor_id;
    std::string display_name;
    Integer age_years=-1;
    StableId presentation_profile_id,portrait_kind;
    Integer visibility_version=0;
};
// 五个公开外貌字段；base_features仅复用九槽值与边界序，绝不带档案的私密来源。
struct CharacterAppearanceView {
    Integer format_version=1;
    StableId generator_version;
    AppearanceFeatures base_features;
    StableId attire_style;
    bool legacy=false;
};
struct CharacterPresentationView { CharacterActorView actor_view; CharacterAppearanceView appearance_view; };

// 观察者许可由C先判断；此端口不枚举人物，不把人物存在当作可见性授权。
class CharacterPresentationRead {
public:
    virtual ~CharacterPresentationRead()=default;
    virtual Outcome<CharacterPresentationView> character(const StableId& actor_id) const=0;
};
// 绑定同一候选和不可变作者目录，clone/restore后构造新的只读投影器。
class CommonCharacterPresentation final : public CharacterPresentationRead {
public:
    CommonCharacterPresentation(const CommonState& state,const CommonCatalog& catalog):state_(state),catalog_(catalog){}
    Outcome<CharacterPresentationView> character(const StableId& actor_id) const override;
private:
    const CommonState& state_;
    const CommonCatalog& catalog_;
};
// 唯一有界DTO编码沿原字典键序，只输出六+五字段；非法字段序元数据明确失败。
Outcome<BoundaryValue> encode_character_presentation(const CharacterPresentationView&);
} // 公开外貌没有来源类别、生成种子、亲代列表、资产、角色私账或整人物副本。
