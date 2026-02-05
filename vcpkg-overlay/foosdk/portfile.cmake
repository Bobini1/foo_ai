vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/foosdk
    REF "aa413b20236a7eed63d9465338851d77e8b46391"
    SHA512 cb8e6ebe851e51116ab7a55b940e63f3c0a91f595e3b758d02db59be582f346c237ea751966e2519f28330a058214f007fcc3187b1910a8e20c87085085e06d4
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME foosdk CONFIG_PATH lib/cmake/foosdk)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/foobar2000/SDK/foobar2000_SDK.xcodeproj" "${CURRENT_PACKAGES_DIR}/include/foobar2000/foo_sample/foo_sample.xcodeproj" "${CURRENT_PACKAGES_DIR}/include/foobar2000/foo_sample/foo_sample.xcworkspace" "${CURRENT_PACKAGES_DIR}/include/foobar2000/foobar2000_component_client/foobar2000_component_client.xcodeproj" "${CURRENT_PACKAGES_DIR}/include/foobar2000/helpers/foobar2000_SDK_helpers.xcodeproj" "${CURRENT_PACKAGES_DIR}/include/foobar2000/shared/shared.xcodeproj" "${CURRENT_PACKAGES_DIR}/include/pfc/pfc.xcodeproj")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/foobar2000/foobar2000_component_client")

file(INSTALL "${SOURCE_PATH}/sdk/sdk-license.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
