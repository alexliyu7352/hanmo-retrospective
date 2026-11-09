// 投影前沿真正同 World owner 全读，来源为空也不能跳过选型核。
auto custody = validate_native_private_shop_household_custody_world(w);
if (!custody.ok)
  return custody;
if (w.private_shop_household_custody)
  WORLD_FIELDS(P::section, "private_shop_household_custody_sources",
               emit_private_shop_household_custody_values_fields(
                   out, *w.private_shop_household_custody));
