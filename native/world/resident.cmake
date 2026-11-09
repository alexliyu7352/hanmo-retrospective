# 常驻原生构建中央登记入口：领域清单、目标规则与消费者分别由具名片段拥有。
# include 保持原目录作用域；生产源、身份摘要与测试登记仍按原顺序执行。
include("${CMAKE_CURRENT_LIST_DIR}/cmake/resident/source_registration.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/resident/target_settings.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/resident/identity_registration.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/resident/consumer_registration.cmake")
