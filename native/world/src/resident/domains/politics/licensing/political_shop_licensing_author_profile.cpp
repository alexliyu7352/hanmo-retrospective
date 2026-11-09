#include "political_shop_licensing_author_profile.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/storage/common_codec.hpp"
#include <new>
namespace hanmo::resident {
Outcome<PoliticalShopLicensingAuthorProfile>
decode_political_shop_licensing_author_profile(const BoundaryValue &input) {
  try {
    auto guarded = validate_political_takeover_payload(input);
    if (!guarded.ok)
      return {false, {}, guarded.error};
    constexpr auto path = "/political_shop_licensing";
    const auto version = boundary::get_int(input, "version", path);
    const BoundaryValue *catalog_input = &input;
    if (version == 2) {
      boundary::fields(input, {"version", "catalog"}, {}, path);
      catalog_input = &boundary::field(input, "catalog", path);
    } else if (version != 1) {
      return {false,
              {},
              common_failure("UnsupportedPoliticalShopLicensingAuthorProfile")
                  .error};
    }
    // 借原输入子树交原严格目录1解码器；不复制JSON、不重写version。
    auto decoded = decode_political_shop_licensing_catalog(*catalog_input);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {version, std::move(decoded.value)}, {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  } catch (const std::bad_alloc &) {
    return {
        false,
        {},
        common_failure("PoliticalShopLicensingAuthorResourceExhausted").error};
  }
}
} // namespace hanmo::resident
