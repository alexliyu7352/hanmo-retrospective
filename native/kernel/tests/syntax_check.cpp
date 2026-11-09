// 语法检查：把全部接口头和 C 接口一起包含一次，只验证能编，不链接。
#include "hanmo_kernel/c_api.h"
#include "hanmo_kernel/kernel.hpp"

namespace {

// 用一个最小的包确认接口能被实现（拿掉它内核也照样能编，这里只为检查）。
class EmptyPackage final : public hanmo::kernel::Package {
 public:
  hanmo::kernel::Manifest manifest() const override { return {"empty", 1, hanmo::kernel::kAbiVersion}; }
  void declare(hanmo::kernel::Declarations &) override {}
};

}  // namespace

// 只为让编译器看到用法。
int syntax_check_entry() {
  EmptyPackage package;
  return package.manifest().kernel_abi == hanmo::kernel::kAbiVersion ? 0 : 1;
}
