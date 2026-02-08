vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/cpp-mcp
    REF 8e6f3a7f157ed7402544d95ef8acae906cb4ec53
    SHA512 d397dd4e8bfb5f3ef083dd49afe741cf177ec5cb50f464cf61febaa7f41a9c0ed0302f4ea0ddadd48732175cb01a53ac7edcf71e6a89b257a4bd90a96008ac2f
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME mcp CONFIG_PATH lib/cmake/mcp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
