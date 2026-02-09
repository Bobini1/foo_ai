vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/cpp-mcp
    REF 23be45cd1e00bd8df09d0004cc8d2cc5b79fb398
    SHA512 7b496badcf3b0f4e121e6c8dda24ffd318e1665a3cfb4a4c5bbd864f4368d84b5f7d05c2d82b1645a100ec00d7299d4445fdae13fb6c61b1fe1dfa24c9422fb6
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME mcp CONFIG_PATH lib/cmake/mcp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
