// 所选人物只读投影的有限门；可导出同一输入交真实GD原projector作独立精确对照。
#include "resident/views/character_presentation.hpp"
#include "resident/storage/common_codec.hpp"
#include <fstream>
#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace hanmo::resident;
using namespace hanmo::resident::boundary;

// 冷目录只提供名字及两列原型表现信息；无资产或整人物定义进入公开DTO。
BoundaryValue data(){
    auto parsed=parse_boundary_json(R"({"items":{},"places":{},"roles":{},"organizations":{},"wallets":{},"inventories":{},"actors":{"actor.a":{"name_key":"name.a","presentation_profile_id":"profile.author","portrait_kind":"clerk"},"actor.b":{"name_key":"name.b","presentation_profile_id":"profile.other","portrait_kind":"traveler"}},"texts":{"name.a":"","name.b":"乙","":"sidebar fallback must not mask invalid identity"}})");
    assert(parsed.ok);return std::move(parsed.value);
}
// 真正存储九槽值，故意非规范字段序以检测输出是否偷偷排序。
AppearanceProfile fixed(){
    AppearanceProfile p;p.generator_version="appearance-v1";p.generation_seed="not-sampled";p.attire_style="robe";p.source="fixed";
    auto& f=p.base_features;f.present=true;f.face_shape="round";f.eyes="narrow";f.brows="straight";f.nose="medium";f.mouth="small";f.hair="bound";f.beard="none";f.skin_palette="warm";f.build="lean";
    f.field_order={"build","skin_palette","beard","hair","mouth","nose","brows","eyes","face_shape"};return p;
}
// 仅构造原query实际读取的typed人物，其余共同字段保持可无损导出的默认值。
CommonState initial(){CommonState s;s.identity.tick=1439;s.identity.revision=7;ActorRecord a;a.id="actor.a";a.birth_tick=-1;s.actors.insert("actor.a",std::move(a));return s;}

struct Cases {
    BoundaryValue::Array rows;std::size_t checks=0;
    // 同一候选调用前后完整共同导出必须一致；成功输出只有原六+五字段。
    CharacterPresentationView run(const StableId& label,CommonState s,BoundaryValue source,bool ok=true,const StableId& code={},const StableId& id="actor.a",bool gd=true){
        auto catalog=decode_common_catalog(source);assert(catalog.ok);const auto before=encode_common_state(s);
        CommonCharacterPresentation read(s,catalog.value);auto projected=read.character(id);assert(projected.ok==ok);
        BoundaryValue expected;
        if(ok){auto encoded=encode_character_presentation(projected.value);assert(encoded.ok);expected=std::move(encoded.value);
            assert(order(expected)==std::vector<StableId>({"ok","actor_view","appearance_view"}));
            assert(order(*expected.find("actor_view"))==std::vector<StableId>({"actor_id","display_name","age_years","presentation_profile_id","portrait_kind","visibility_version"}));
            assert(order(*expected.find("appearance_view"))==std::vector<StableId>({"format_version","generator_version","base_features","attire_style","legacy"}));
            assert(!expected.find("appearance_view")->find("generation_seed")&&!expected.find("appearance_view")->find("parent_sources")&&!expected.find("appearance_view")->find("source"));
        }else {assert(projected.error.original.code==code);expected=obj({{"ok",BoundaryValue{false}},{"code",BoundaryValue{code}}});}
        assert(compare_boundary(before,encode_common_state(s)).exact());
        if(gd)rows.push_back(obj({{"case",BoundaryValue{label}},{"state",before},{"data",std::move(source)},{"actor_id",BoundaryValue{id}},{"expected",std::move(expected)}}));
        ++checks;return projected.value;
    }
};

int main(int argc,char** argv){
    assert(argc==1||argc==2);Cases tests;auto s=initial();auto catalog_data=data();
    auto view=tests.run("empty-authored-name-and-missing-appearance",s,catalog_data);
    assert(view.actor_view.display_name.empty()&&view.actor_view.age_years==2&&view.actor_view.visibility_version==7&&view.actor_view.presentation_profile_id=="profile.author"&&view.actor_view.portrait_kind=="clerk"&&view.appearance_view.legacy);
    auto* actor=s.actors.find("actor.a");actor->prototype_id="actor.b";
    view=tests.run("saved-prototype",s,catalog_data);assert(view.actor_view.display_name=="乙"&&view.actor_view.portrait_kind=="traveler");
    actor->display_name="自定名";view=tests.run("saved-name",s,catalog_data);assert(view.actor_view.display_name=="自定名");
    actor->prototype_id="missing";tests.run("missing-prototype-before-valid-name",s,catalog_data,false,"InvalidPresentationActor");
    actor->prototype_id.reset();tests.run("absent-actor",s,catalog_data,false,"InvalidPresentationActor","missing");
    for(const auto& bad:std::vector<std::string>{""," 前","后 ","前\t后","前\n后",std::string("前\x7f后")}){actor->display_name=bad;tests.run("invalid-saved-name",s,catalog_data,false,"InvalidPresentationActor");}
    std::string long_name;for(int i=0;i<80;++i)long_name+="汉";actor->display_name=long_name;tests.run("eighty-codepoints",s,catalog_data);
    actor->display_name=long_name+"汉";tests.run("eighty-one-codepoints",s,catalog_data,false,"InvalidPresentationActor");
    actor->display_name=" 汉　";tests.run("original-unicode-space-rule",s,catalog_data);actor->display_name.reset();
    auto absent_text=catalog_data;std::get<BoundaryValue::Dict>(absent_text.find("texts")->data).clear();view=tests.run("absent-author-text",s,absent_text);assert(view.actor_view.display_name=="actor.a");
    for(const auto& entry:std::vector<std::pair<std::optional<Integer>,Integer>>{{std::nullopt,-1},{1440,-1},{-2147483648LL,-1},{0,1},{1439,0},{-1,2}}){actor->birth_tick=entry.first;view=tests.run("age-boundary",s,catalog_data);assert(view.actor_view.age_years==entry.second);}
    s.identity.tick=2147483647;actor->birth_tick=-2147483647;view=tests.run("oldest-valid-age",s,catalog_data);assert(view.actor_view.age_years==4294967294LL/720);
    s.identity.tick=719;actor->birth_tick=0;view=tests.run("before-birthday",s,catalog_data);assert(view.actor_view.age_years==0);
    s.identity.tick=720;view=tests.run("on-birthday",s,catalog_data);assert(view.actor_view.age_years==1);
    auto profile=fixed();actor->appearance=std::make_shared<const AppearanceProfile>(profile);
    view=tests.run("fixed-feature-order",s,catalog_data);assert(view.appearance_view.base_features.field_order==profile.base_features.field_order&&!view.appearance_view.legacy);
    for(const auto& source:std::vector<StableId>{"custom","generated"}){profile.source=source;profile.generation_seed=source=="custom"?"not-sampled":"private-generated-seed";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run(source,s,catalog_data);}
    std::string long_seed;for(int i=0;i<256;++i)long_seed+="种";profile.generation_seed=long_seed;actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("seed-codepoints-no-byte-limit",s,catalog_data);
    profile.generation_seed=long_seed+"种";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("invalid-seed-length",s,catalog_data,false,"InvalidAppearanceSeed");
    profile=fixed();profile.format_version=2;actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("invalid-format",s,catalog_data,false,"InvalidAppearanceProfile");
    profile=fixed();profile.generator_version="future";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("unknown-generator",s,catalog_data,false,"UnsupportedAppearanceGenerator");
    actor->display_name="";tests.run("identity-before-appearance",s,catalog_data,false,"InvalidPresentationActor");actor->display_name.reset();
    profile=fixed();profile.base_features.hair="res://private.png";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("invalid-feature",s,catalog_data,false,"InvalidAppearanceProfile");
    profile=fixed();profile.generation_seed="private";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("fixed-seed-rejected",s,catalog_data,false,"InvalidAppearanceSeed");
    profile=fixed();profile.parent_sources={"private-parent"};actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("non-inherited-parent-rejected",s,catalog_data,false,"InvalidAppearanceParents");
    profile=fixed();profile.source="inherited";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("inherited-capability-stays-unsupported",s,catalog_data,false,"AppearanceInheritanceCapabilityRequired","actor.a",false);
    profile=AppearanceProfile{};profile.generator_version="legacy-portrait-v1";profile.generation_seed="not-sampled";profile.attire_style="legacy";profile.source="legacy";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("saved-canonical-legacy",s,catalog_data);
    profile.generation_seed="secret";actor->appearance=std::make_shared<const AppearanceProfile>(profile);tests.run("invalid-legacy",s,catalog_data,false,"InvalidLegacyAppearance");
    auto missing_columns=catalog_data;auto& definition=std::get<BoundaryValue::Dict>(missing_columns.find("actors")->find("actor.a")->data);definition.erase(definition.begin()+1,definition.end());actor->appearance.reset();view=tests.run("missing-cold-columns",s,missing_columns);assert(view.actor_view.presentation_profile_id.empty()&&view.actor_view.portrait_kind.empty());
    for(const auto& key:{"presentation_profile_id","portrait_kind"}){auto malformed=catalog_data;malformed.find("actors")->find("actor.a")->find(key)->data=Integer{3};auto decoded=decode_common_catalog(malformed);assert(!decoded.ok&&decoded.error.original.code=="InvalidResidentString");}
    auto catalog=decode_common_catalog(catalog_data);assert(catalog.ok);actor->appearance=std::make_shared<const AppearanceProfile>(fixed());CommonCharacterPresentation read(s,catalog.value);auto owned=read.character("actor.a");assert(owned.ok);auto changed=fixed();changed.base_features.hair="different";actor->appearance=std::make_shared<const AppearanceProfile>(changed);catalog.value.texts.set("name.a","renamed");assert(owned.value.actor_view.display_name.empty()&&owned.value.appearance_view.base_features.hair=="bound");
    auto clone=s;clone.actors.find("actor.a")->display_name="clone";CommonCharacterPresentation rebound(clone,catalog.value);assert(rebound.character("actor.a").value.actor_view.display_name=="clone"&&read.character("actor.a").value.actor_view.display_name=="renamed");
    auto invalid_order=owned.value;invalid_order.appearance_view.base_features.field_order={"hair"};assert(!encode_character_presentation(invalid_order).ok);
    if(argc==2){auto encoded=render_boundary_json(obj({{"cases",BoundaryValue{tests.rows}}}));assert(encoded.ok);std::ofstream file(argv[1]);assert(file);file<<encoded.value;}
    std::cout<<"state_character_presentation: PASS cases="<<tests.checks<<" gd_vectors="<<tests.rows.size()<<" (six/five field whitelist, identity/appearance order, exact age/name/features, no mutation, clone/owned output)\n";
}
