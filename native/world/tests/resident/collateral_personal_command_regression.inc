// 原测试片段尚未恢复；此替身只补原调用入口，不重建或放宽缺失的断言。
// A1/A2 缺源登记；原片段的具体 Family 清单覆盖未知，不能据此声明通过。
// 被调用时交由原测试的异常处理报告 NOT_IMPLEMENTED，并以失败退出。
void collateral_personal_actual_command_binding() {
  throw std::runtime_error(
      "NOT_IMPLEMENTED: collateral_personal_command_regression.inc original "
      "test fragment unavailable; original command assertions remain "
      "unimplemented");
}
