vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/cpp-mcp
    REF 96561a3037586e6b3e5bbdd71e151ccb8c67363b
    SHA512 20b917246ed7de650f8c1ae50f46c08c8fa5ae5d1a2c87a0cbd2a226b10543d1a06031275e22c9c62489eaed4a0f3fd1f0b16809d063393065de35f76febc4d9
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME mcp CONFIG_PATH lib/cmake/mcp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
