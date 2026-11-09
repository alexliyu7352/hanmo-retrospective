// 制造世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (w.craft_jobs)
  WORLD_FIELDS(P::action_payloads, "craft_jobs",
               emit_native_item_craft_action_payloads_fields(
                   out, w.common, *w.catalog,
                   w.item_instances ? &*w.item_instances : nullptr,
                   &*w.craft_jobs, NativeItemCraftFactProjection::craft));
if (w.item_instances)
  WORLD_FIELDS(P::action_payloads, "item_instances",
               emit_native_item_craft_action_payloads_fields(
                   out, w.common, *w.catalog, &*w.item_instances,
                   w.craft_jobs ? &*w.craft_jobs : nullptr,
                   NativeItemCraftFactProjection::repair));
if (w.craft_jobs)
  WORLD_FIELDS(P::fact_payloads, "craft_jobs",
               emit_native_item_craft_fact_payloads_fields(
                   out, w.common, *w.catalog,
                   w.item_instances ? &*w.item_instances : nullptr,
                   &*w.craft_jobs, NativeItemCraftFactProjection::craft));
if (w.item_instances)
  WORLD_FIELDS(P::fact_payloads, "item_instances",
               emit_native_item_craft_fact_payloads_fields(
                   out, w.common, *w.catalog, &*w.item_instances,
                   w.craft_jobs ? &*w.craft_jobs : nullptr,
                   NativeItemCraftFactProjection::repair));
