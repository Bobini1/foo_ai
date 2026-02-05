vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/cpp-mcp
    REF "8d065148d89dea815acbde23498bb624c635e351"
    SHA512 4290c35b730be5a5fa72999eada931a66ac4bd326d4640d05f98ef9f0edc88e35b3d97ad4bee5cfd83c5ea6aff7708e2781768224e4d0c41e3325c844a530f21
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME mcp CONFIG_PATH lib/cmake/mcp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
