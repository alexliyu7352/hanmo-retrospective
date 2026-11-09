// 新 /1 段与旧 operation /2 无交叠，不在旧格式尾静默加字段。
r = validate_native_private_shop_household_custody_world(w);
if (!r.ok)
  return r;
if (w.private_shop_household_custody) {
  r = section("private_shop_household_custody_sources", [&](auto &c) {
    return visit_private_shop_household_custody_values(
        c, *w.private_shop_household_custody);
  });
  if (!r.ok)
    return r;
}
