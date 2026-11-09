// 唯一首次容器receipt字段表；内部钱包/库存帧仍走Common原布局，S6只组合此口。
#pragma once
#include "foundation_asset_resource_fields.hpp"
#include "foundation_container_opening.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
namespace hanmo::resident::foundation_container_opening_fields {
using Layout = foundation_asset_resource_fields::Layout;
// 模板地址与真实实例分离；只接受本有限创建的两个资源种类。
template <class C> Status definition(C &c, ResourceKey &value) {
  foundation_asset_resource_fields::begin(c);
  StableId tag;
  if (!c.reading()) {
    if (value.kind == ResourceKey::Kind::wallet)
      tag = "wallet";
    else if (value.kind == ResourceKey::Kind::inventory)
      tag = "inventory";
    else
      return c.reject("FoundationContainerDefinitionKindRequired");
  }
  c.field("kind", tag);
  if (!c.status().ok)
    return c.status();
  if (tag != "wallet" && tag != "inventory")
    return c.reject("FoundationContainerDefinitionKindRequired");
  if (c.reading())
    value.kind = tag == "wallet" ? ResourceKey::Kind::wallet
                                 : ResourceKey::Kind::inventory;
  return exact_fields::required_ref(c, "id", value.id);
}
// 原创建请求只有原wallet/inventory字段，after保同一真实捕获的原Common帧。
template <class C>
Status actual(C &c, FoundationWalletCreationEvidence &value,
              const Layout &layout) {
  foundation_asset_resource_fields::begin(c);
  c.object("request", [&](auto &request) {
    foundation_asset_resource_fields::begin(request);
    return request.object("wallet", [&](auto &wallet) {
      return foundation_asset_resource_fields::wallet(
          wallet, value.request.wallet, layout);
    });
  });
  return c.object("after", [&](auto &after) {
    return foundation_asset_resource_fields::wallet(after, value.after, layout);
  });
}
template <class C>
Status actual(C &c, FoundationInventoryCreationEvidence &value,
              const Layout &layout) {
  foundation_asset_resource_fields::begin(c);
  c.object("request", [&](auto &request) {
    foundation_asset_resource_fields::begin(request);
    return request.object("inventory", [&](auto &inventory) {
      return foundation_asset_resource_fields::inventory(
          inventory, value.request.inventory, layout);
    });
  });
  return c.object("after", [&](auto &after) {
    return foundation_asset_resource_fields::inventory(after, value.after,
                                                       layout);
  });
}
// definition的已核tag是唯一判别，不新增可不一致的第二种类字段。
template <class C>
Status creation(C &c, FoundationContainerCreation &value,
                const Layout &layout) {
  foundation_asset_resource_fields::begin(c);
  c.object("definition",
           [&](auto &child) { return definition(child, value.definition); });
  if (!c.status().ok)
    return c.status();
  if (value.definition.kind == ResourceKey::Kind::wallet) {
    if (c.reading() &&
        !std::holds_alternative<FoundationWalletCreationEvidence>(
            value.actual)) {
      auto prepared =
          c.template prepare_default_values<FoundationWalletCreationEvidence>();
      if (!prepared.ok)
        return prepared;
      value.actual.template emplace<FoundationWalletCreationEvidence>();
    }
    auto *row = std::get_if<FoundationWalletCreationEvidence>(&value.actual);
    if (!row)
      return c.reject("FoundationContainerCreationVariantRequired");
    return c.object("actual",
                    [&](auto &child) { return actual(child, *row, layout); });
  }
  if (value.definition.kind != ResourceKey::Kind::inventory)
    return c.reject("FoundationContainerDefinitionKindRequired");
  if (c.reading() &&
      !std::holds_alternative<FoundationInventoryCreationEvidence>(
          value.actual)) {
    auto prepared = c.template prepare_default_values<
        FoundationInventoryCreationEvidence>();
    if (!prepared.ok)
      return prepared;
    value.actual.template emplace<FoundationInventoryCreationEvidence>();
  }
  auto *row = std::get_if<FoundationInventoryCreationEvidence>(&value.actual);
  if (!row)
    return c.reject("FoundationContainerCreationVariantRequired");
  return c.object("actual",
                  [&](auto &child) { return actual(child, *row, layout); });
}
// 同一成员顺序供JSON、binary和projection；不存在独立可发布容器根。
template <class C>
Status receipt(C &c, FoundationContainerOpeningReceipt &value,
               const Layout &layout) {
  foundation_asset_resource_fields::begin(c);
  return c.sequence("creations", value.creations, [&](auto &child, auto &row) {
    return creation(child, row, layout);
  });
}
} // namespace hanmo::resident::foundation_container_opening_fields
