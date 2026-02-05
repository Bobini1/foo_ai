vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/base64
    REF "d7e78d1440d5dc1d87c50404469b822f960f170c"
    SHA512 fcbc6cabb60755ffa146ec32fa4fbfed30b992b31653d0b35714543b9fcfea926580c83850143359485cf5a8f4bf1d1938e8ab39f61857bce3850950ffada068
    HEAD_REF master
)

set(VCPKG_BUILD_TYPE release) # header-only port

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME base64 CONFIG_PATH lib/cmake/base64)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
