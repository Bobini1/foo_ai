vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Bobini1/foosdk
    REF "754d4e1f2fdbcd58baf6d54d72994658b687cfec"
    SHA512 9417a18ea6c502a002b1b70b10d8a4fc8f32a4d3be2cce7cbe0a05d31ad68ce63bf8a91531a3c74796fee280e6c413f384a091c1ab6b8a7ee77b1312245b6656
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
