vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/cpp-mcp
    REF 82311673e33bb3c55374fb73029d0ec8167a7ff1
    SHA512 5df330fa35c79db91d271f8fa5be384103f1183d94aac7c8d0a7c75071d5469ad31d5ad051a206ebf3d1288d0c24ade697c8d7a953c9cd269d8a355f1e17d520
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME mcp CONFIG_PATH lib/cmake/mcp)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
