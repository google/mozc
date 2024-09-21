load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")
load(
    "//:config.bzl",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
)

def mozc_macos_qt_application(name, bundle_name, deps):
    macos_application(
        name = name,
        tags = ["manual", "notap"],
        additional_contents = mozc_select(
            default = {},
            oss = {"@qt_mac//:libqcocoa": "Resources"},
        ),
        app_icons = ["//data/images/mac:product_icon.icns"],
        bundle_id = MACOS_BUNDLE_ID_PREFIX + ".Tool." + bundle_name,
        bundle_name = bundle_name,
        infoplists = ["//gui:mozc_tool_info_plist"],
        minimum_os_version = MACOS_MIN_OS_VER,
        resources = [
            "//data/images/mac:candidate_window_logo.tiff",
            "//gui:qt_conf",
        ],
        visibility = [
            "//:__subpackages__",
            "//:__subpackages__",
        ],
        deps = deps + mozc_select(
            default = [],
            oss = [
                "@qt_mac//:QtCore_mac",
                "@qt_mac//:QtGui_mac",
                "@qt_mac//:QtPrintSupport_mac",
                "@qt_mac//:QtWidgets_mac",
            ],
        ),
    )

register_extension_info(
    extension = mozc_macos_qt_application,
    label_regex_for_dep = "{extension_name}",
)

