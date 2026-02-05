vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/cpp-mcp
    REF "42eb5a5ec4839a9a4bac3496a2584177c3f9d976"
    SHA512 4b45d170b07f35d6ab854ffaa593820b1e3d748cd9ee4a7b33de26d56bdcba12a6dd851f280ff906728d6f1e64a98428ef94b5bb9989949d6dbde47d29dbef31
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME mcp CONFIG_PATH lib/cmake/mcp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
