// 共同typed检查点：类型标签、字段、ID与所有列表顺序逐项保存，读取不安装世界。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "common_checkpoint.hpp"
namespace hanmo::resident {
namespace common_checkpoint {
Status reservation(CodecVisitor& c,ReservationRef& ref,const StableId& legacy_name){
 if(c.schema_features&typed_resource_references){Integer kind=static_cast<Integer>(ref.kind);c.field("kind",kind);c.field("id",ref.id);if(kind<0||kind>3||(kind==3&&!c.domain_activity_reservation_refs_enabled)||(ref.id.empty()&&kind!=0))return c.reject("InvalidReservationKind");if(c.reading())ref.kind=static_cast<ReservationRef::Kind>(kind);if(kind==3){c.field("domain_id",ref.domain_id);c.field("activity_kind",ref.activity_kind);if(ref.domain_id.empty()||ref.activity_kind.empty())return c.reject("InvalidReservationOwner");}else{if(c.reading()){ref.domain_id.clear();ref.activity_kind.clear();}else if(!ref.domain_id.empty()||!ref.activity_kind.empty())return c.reject("InvalidReservationOwner");}return c.status();}
 if(!c.reading()&&(ref.kind!=ReservationRef::Kind::action||!ref.domain_id.empty()||!ref.activity_kind.empty()))return c.reject("UnsupportedReservationKind");if(c.reading()){ref.kind=ReservationRef::Kind::action;ref.domain_id.clear();ref.activity_kind.clear();}return c.field(legacy_name,ref.id);
}
Status owner(CodecVisitor& c,Owner& o){Integer kind=static_cast<Integer>(o.kind);c.field("kind",kind);c.field("id",o.id);if(kind<0||kind>((c.schema_features&typed_resource_references)?3:2))return c.reject("InvalidOwner");if(o.extra)return c.reject("UnsupportedOwnerCompatibility");strings(c,"field_order",o.field_order);if(c.reading())o.kind=static_cast<Owner::Kind>(kind);return c.status();}
Status location(CodecVisitor& c,Location& location){Integer tag=static_cast<Integer>(location.index());c.field("kind",tag);if(tag<0||tag>5)return c.reject("InvalidLocationKind");if(tag==5&&!c.carrier_location_enabled)return c.reject("CarrierLocationProfileRequired");if(c.reading()){switch(tag){case 0:location=SettlementLocation{};break;case 1:location=WildernessLocation{};break;case 2:location=FollowingLocation{};break;case 3:location=InArmyLocation{};break;case 4:location=OnRouteLocation{};break;case 5:location=CarriedLocation{};break;}}return std::visit([&](auto& value)->Status{using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,SettlementLocation>){c.field("settlement_id",value.settlement_id);c.field("place_id",value.place_id);}else if constexpr(std::is_same_v<T,WildernessLocation>){c.field("map_id",value.map_id);c.field("origin",value.origin);c.field("x",value.point.x);c.field("y",value.point.y);}else if constexpr(std::is_same_v<T,FollowingLocation>)c.field("party_id",value.party_id);else if constexpr(std::is_same_v<T,InArmyLocation>)c.field("army_id",value.army_id);else if constexpr(std::is_same_v<T,OnRouteLocation>){c.field("edge_id",value.edge_id);c.field("origin",value.origin);c.field("progress",value.progress);}else if constexpr(std::is_same_v<T,CarriedLocation>){Integer kind=static_cast<Integer>(value.carrier_kind);c.field("carrier_kind",kind);c.field("carrier_id",value.carrier_id);if(kind<0||kind>2||value.carrier_id.empty())return c.reject("InvalidCarriedLocation");if(c.reading())value.carrier_kind=static_cast<CarriedLocation::CarrierKind>(kind);}return c.status();},location);}
Status lot(CodecVisitor& v,LotView& l){
 if(l.extra)return v.reject("UnsupportedLotCompatibility");strings(v,"field_order",l.field_order);v.field("id",l.id);v.field("item_id",l.item_id);v.field("quantity",l.quantity);reservation(v,l.reserved_by,"reserved_by");v.object("owner",[&](CodecVisitor& e){return owner(e,l.owner);});
 if(v.item_instance_refs_enabled)v.optional("item_instance_id",l.item_instance_id,[](CodecVisitor& e,StableId& id){e.field("value",id);return id.empty()?e.reject("InvalidItemInstanceReference"):e.status();});else if(l.item_instance_id)return v.reject("CommonExtensionCodecMismatch");
 if(v.lot_property_refs_enabled)v.optional("lot_properties_ref",l.lot_properties_ref,[](CodecVisitor& e,LotPropertiesRef& ref){e.field("properties_id",ref.properties_id);return ref.properties_id.empty()?e.reject("InvalidLotPropertiesReference"):e.status();});else if(l.lot_properties_ref)return v.reject("CommonExtensionCodecMismatch");
 if(l.item_instance_id&&l.lot_properties_ref)return v.reject("LotReferenceConflict");
 for(const auto& key:l.field_order)if(key!="id"&&key!="item_id"&&key!="quantity"&&key!="owner"&&key!="reserved_by"&&!(key=="item_instance_id"&&v.item_instance_refs_enabled)&&!(key=="lot_properties_ref"&&v.lot_property_refs_enabled))return v.reject("CommonExtensionCodecMismatch",key);
 return v.status();
}
// 钱包复用共同检查点的原字段顺序；所有权或预留读写失败沿用访问器错误。
Status wallet(CodecVisitor &c, WalletView &w) {
  c.field("id", w.id);
  c.object("owner", [&](CodecVisitor &v) { return owner(v, w.owner); });
  c.field("balance", w.balance);
  strings(c, "field_order", w.field_order);
  return c.sequence("reserved", w.reserved,
                    [](CodecVisitor &v, Reservation &r) {
                      reservation(v, r.action_id, "action_id");
                      return v.field("amount", r.amount);
                    });
}
// 容器复用共同检查点的原字段和批次顺序；批次校验失败不被成功状态覆盖。
Status inventory(CodecVisitor &c, InventoryView &i) {
  c.field("id", i.id);
  c.object("owner", [&](CodecVisitor &v) { return owner(v, i.owner); });
  c.field("capacity", i.capacity);
  c.field("revision", i.revision);
  strings(c, "field_order", i.field_order);
  return c.sequence("lots", i.lots, [](CodecVisitor &v, LotView &l) {
    return common_checkpoint::lot(v, l);
  });
}
} // 各域复用唯一owner/location schema，避免自造近似所有权。
namespace {
using namespace common_checkpoint;
Status appearance(CodecVisitor& c,AppearanceProfile& p){
c.field("format_version",p.format_version);
c.field("generator_version",p.generator_version);
c.field("generation_seed",p.generation_seed);
c.field("attire_style",p.attire_style);
c.field("source",p.source);
c.field("features_present",p.base_features.present);
c.field("face_shape",p.base_features.face_shape);
c.field("eyes",p.base_features.eyes);
c.field("brows",p.base_features.brows);
c.field("nose",p.base_features.nose);
c.field("mouth",p.base_features.mouth);
c.field("hair",p.base_features.hair);
c.field("beard",p.base_features.beard);
c.field("skin_palette",p.base_features.skin_palette);
c.field("build",p.base_features.build);
strings(c,"parent_sources",p.parent_sources);strings(c,"field_order",p.field_order);return strings(c,"features_field_order",p.base_features.field_order);}
Status actor(CodecVisitor& c,ActorRecord& a){
c.field("id",a.id);
c.field("action_id",a.action_id);
c.field("wallet_id",a.wallet_id);
c.field("inventory_id",a.inventory_id);
c.field("alive",a.alive);
c.field("available",a.available);
c.field("hp",a.hp);
c.field("fatigue",a.fatigue);
c.field("merit",a.merit);
c.field("injury",a.injury);
c.object("location",[&](CodecVisitor& v){return location(v,a.location);});
c.optional("birth_tick",a.birth_tick,[](CodecVisitor& v,Integer& x){return v.field("value",x);});
c.optional("created_tick",a.created_tick,[](CodecVisitor& v,Integer& x){return v.field("value",x);});
c.optional("prototype_id",a.prototype_id,[](CodecVisitor& v,StableId& x){return v.field("value",x);});
c.optional("display_name",a.display_name,[](CodecVisitor& v,StableId& x){return v.field("value",x);});
c.object("attributes",[&](CodecVisitor& v){v.field("administration",a.attributes.administration);v.field("charisma",a.attributes.charisma);v.field("intellect",a.attributes.intellect);v.field("leadership",a.attributes.leadership);v.field("martial",a.attributes.martial);return v.status();});
c.object("skills",[&](CodecVisitor& v){v.field("archery",a.skills.archery);v.field("cavalry",a.skills.cavalry);v.field("commerce",a.skills.commerce);v.field("debate",a.skills.debate);v.field("infantry",a.skills.infantry);v.field("martial",a.skills.martial);v.field("negotiation",a.skills.negotiation);v.field("rites",a.skills.rites);v.field("tactics",a.skills.tactics);v.field("transport",a.skills.transport);
if(v.schema_features&extended_actor_skills){if(v.reading())a.skills.extended.emplace();if(!a.skills.extended)return v.reject("MissingExtendedSkills");auto& x=*a.skills.extended;v.object("extended",[&](CodecVisitor& e){e.field("naval",x.naval);e.field("espionage",x.espionage);e.field("fortification",x.fortification);e.field("agriculture",x.agriculture);e.field("medicine",x.medicine);e.field("forging",x.forging);return strings(e,"field_order",x.field_order);});}
else if(a.skills.extended)return v.reject("ExtendedSkillsCapabilityRequired");
return v.status();});
c.object("combat",[&](CodecVisitor& v){v.field("weapon_id",a.combat.weapon_id);v.field("heart_id",a.combat.heart_id);v.optional("armor_id",a.combat.armor_id,[](CodecVisitor& e,StableId& id){return e.field("value",id);});return table(v,"learned",a.combat.learned,[](CodecVisitor& e,Integer& value){return e.field("mastery",value);});});
if (c.reading()) {
  std::optional<AppearanceProfile> profile;
  c.optional("appearance", profile, appearance);
  // 沿原检查点共享工厂保留真实凭据，读取字段与空值语义不变。
  if (c.status().ok)
    a.appearance = profile ? checkpoint_make_shared<const AppearanceProfile>(
                                 std::move(*profile))
                           : nullptr;
} else
  c.object("appearance", [&](CodecVisitor &writer) {
    bool present = bool(a.appearance);
    writer.field("present", present);
    if (present && writer.status().ok)
      return appearance(writer, const_cast<AppearanceProfile &>(*a.appearance));
    return writer.status();
  });
strings(c, "field_order", a.field_order);
strings(c, "location_order", a.location_order);
strings(c, "attributes_order", a.attributes_order);
strings(c, "skills_order", a.skills_order);
strings(c, "combat_order", a.combat_order);
return c.status();
}
Status action(CodecVisitor& c,ActionCoreView& a){c.field("id",a.id);c.field("kind",a.kind);c.field("actor_id",a.actor_id);strings(c,"participants",a.participants);c.field("start_tick",a.start_tick);return c.field("remaining_ticks",a.remaining_ticks);}
Status party(CodecVisitor& c,PartyRecord& p){c.field("id",p.id);c.field("leader_id",p.leader_id);strings(c,"members",p.members);table(c,"terms",p.terms,[](CodecVisitor& v,CompanionTerms& t){v.field("role_id",t.role_id);v.field("cost_per_tick",t.cost_per_tick);return strings(v,"field_order",t.field_order);});c.field("created_tick",p.created_tick);return strings(c,"field_order",p.field_order);}
} // 只读兼容外观进入明确结构，未声明字段不会藏在checkpoint载荷里。
Status visit_common_checkpoint(CodecVisitor &c, CommonState &s,
                               const CommonResultCodec *results) {
  using namespace common_checkpoint;
  if (c.reading()) {
    std::vector<Integer> world_id;
    c.sequence("world_id", world_id,
               [](CodecVisitor &v, Integer &b) { return v.field("byte", b); });
    if (c.status().ok) {
      if (world_id.size() != 16)
        return c.reject("InvalidWorldId");
      for (std::size_t i = 0; i < 16; ++i) {
        if (world_id[i] < 0 || world_id[i] > 255)
          return c.reject("InvalidWorldId");
        s.identity.world_id[i] = static_cast<std::uint8_t>(world_id[i]);
      }
    }
  } else
    c.sequence_view("world_id", s.identity.world_id.size(),
                    [&](CodecVisitor &v, std::uint64_t index) {
                      Integer byte = s.identity.world_id[std::size_t(index)];
                      return v.field("byte", byte);
                    });
  strings(c, "root_field_order", s.root_field_order);
  strings(c, "battle_order", s.battle_order);
  auto orders = [](CodecVisitor &v, std::vector<StableId> &keys) {
    return strings(v, "keys", keys);
  };
  table(c, "action_field_order", s.action_field_order, orders);
  c.field("revision", s.identity.revision);
  c.field("tick", s.identity.tick);
  c.field("next_instance_id", s.next_instance_id);
  c.object("metadata", [&](CodecVisitor &v) {
    v.field("ruleset_id", s.metadata.ruleset_id);
    v.field("content_id", s.metadata.content_id);
    v.field("content_version", s.metadata.content_version);
    v.field("content_digest", s.metadata.content_digest);
    v.field("engine_build", s.metadata.engine_build);
    v.field("rng_backend", s.metadata.rng_backend);
    v.field("scenario_id", s.metadata.scenario_id);
    v.field("player_actor_id", s.metadata.player_actor_id);
    return v.status();
  });
  table(c, "actors", s.actors, actor);
  table(c, "actions", s.actions, action);
  table(c, "wallets", s.wallets, [](CodecVisitor &v, WalletView &w) {
    return common_checkpoint::wallet(v, w);
  });
  table(c, "inventories", s.inventories, [](CodecVisitor &v, InventoryView &i) {
    return common_checkpoint::inventory(v, i);
  });
  table(c, "parties", s.parties, party);
  table(c, "roles", s.roles, [](CodecVisitor &v, RoleRecord &r) {
    v.field("actor_id", r.actor_id);
    v.field("enabled", r.enabled);
    return strings(v, "field_order", r.field_order);
  });
  table(c, "facts", s.facts, [](CodecVisitor &v, FactRecord &f) {
    v.field("id", f.id);
    v.field("kind", f.kind);
    v.field("domain", f.domain);
    return strings(v, "field_order", f.field_order);
  });
  table(c, "rng", s.rng, [](CodecVisitor &v, RngStream &r) {
    v.field("seed", r.seed);
    v.field("state", r.state);
    return strings(v, "field_order", r.field_order);
  });
  table(c, "request_sequences", s.request_sequences,
        [](CodecVisitor &v, Integer &n) { return v.field("value", n); });
  table(c, "results", s.results,
        [&](CodecVisitor &v, std::shared_ptr<const CommitResult> &result) {
          if (!results)
            return v.reject("ResultCodecRequired");
          if (!v.reading() && !result)
            return v.reject("InvalidCachedResult");
          auto status = results->visit(v, result);
          if (!status.ok)
            return status;
          if (!result)
            return v.reject("InvalidCachedResult");
          return status;
        });
  c.sequence("history", s.history, [](CodecVisitor &v, HistoryRecord &h) {
    v.field("tick", h.tick);
    v.field("kind", h.kind);
    if (h.kind == "configure_household_provision" &&
        !(v.schema_features & household_provision_configuration_receipts))
      return v.reject("UnexpectedHouseholdProvisionHistory");
    v.field("actor_id", h.actor_id);
    v.field("result_id", h.result_id);
    return strings(v, "field_order", h.field_order);
  });
  return c.status();
}
} // 共享状态结果不擅自补填，恢复后的外层必须重绑领域借用并执行完整profile。
