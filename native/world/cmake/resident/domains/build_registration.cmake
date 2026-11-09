# 构建与接口登记身份：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产身份输入连续登记段：contracts/resident_shared_execution.json。
set(HANMO_RESIDENT_IDENTITY_build_registration_resident_shared_execution_json
    contracts/resident_shared_execution.json
    cmake/resident_shared_execution.cmake
)

# 生产身份输入连续登记段：CMakeLists.txt。
set(HANMO_RESIDENT_IDENTITY_build_registration_CMakeLists_txt
    CMakeLists.txt
)

# 生产身份输入连续登记段：resident.cmake。
set(HANMO_RESIDENT_IDENTITY_build_registration_resident_cmake
    resident.cmake
)

# 生产身份输入连续登记段：tools/resident_journey_main.cpp。
set(HANMO_RESIDENT_IDENTITY_build_registration_resident_journey_main_cpp
    tools/resident_journey_main.cpp
)
