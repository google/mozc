load(
    "//:config.bzl",
    "BAZEL_TOOLS_PREFIX",
    "BRANDING",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
)
load(
    "//:build_defs.bzl",
    "MOZC_TAGS",
    "mozc_select",
    "mozc_infoplist",
    "mozc_infoplist_strings",
)
load("//bazel:run_build_tool.bzl", "mozc_run_build_tool")
load("//bazel:stubs.bzl", "pytype_strict_binary", "pytype_strict_library", "register_extension_info")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application", "macos_bundle")

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies")
load("@rules_proto//proto:toolchains.bzl", "rules_proto_toolchains")

load(
    "@build_bazel_rules_apple//apple:repositories.bzl",
    "apple_rules_dependencies",
)

load(
    "@build_bazel_rules_swift//swift:repositories.bzl",
    "swift_rules_dependencies",
)

load(
    "@build_bazel_rules_swift//swift:extras.bzl",
    "swift_rules_extra_dependencies",
)

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)

def apple_dependencies():
    apple_rules_dependencies()
    swift_rules_dependencies()
    swift_rules_extra_dependencies()
    apple_support_dependencies()

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

def _tweak_infoplists(name, infoplists):
    tweaked_infoplists = []
    for i, plist in enumerate(infoplists):
        plist_name = "%s_plist%d" % (name, i)
        mozc_infoplist(
            name = plist_name,
            srcs = [plist],
            outs = [plist.replace(".plist", "_tweaked.plist")],
        )
        tweaked_infoplists.append(plist_name)
    return tweaked_infoplists

def _tweak_strings(name, strings):
    tweaked_strings = []
    for i, string in enumerate(strings):
        string_name = "%s_string%d" % (name, i)
        mozc_infoplist_strings(
            name = string_name,
            srcs = [string],
            outs = ["tweaked/" + string],
        )
        tweaked_strings.append(string_name)
    return tweaked_strings

def mozc_macos_application(name, bundle_name, infoplists, strings = [], bundle_id = None, tags = [], **kwargs):
    """Rule to create .app for macOS.

    Args:
      name: name for macos_application.
      bundle_name: bundle_name for macos_application.
      infoplists: infoplists are tweaked and applied to macos_application.
      strings: strings are tweaked and applied to macos_application.
      bundle_id: bundle_id for macos_application.
      tags: tags for macos_application.
      **kwargs: other arguments for macos_application.
    """
    macos_application(
        name = name,
        bundle_id = bundle_id or (MACOS_BUNDLE_ID_PREFIX + "." + bundle_name),
        bundle_name = bundle_name,
        infoplists = _tweak_infoplists(name, infoplists),
        strings = _tweak_strings(name, strings),
        minimum_os_version = MACOS_MIN_OS_VER,
        version = "//data/version:version_macos",
        target_compatible_with = ["@platforms//os:macos"],
        tags = tags + MOZC_TAGS.MAC_ONLY,
        **kwargs
    )

def mozc_macos_bundle(name, bundle_name, infoplists, strings = [], bundle_id = None, tags = [], **kwargs):
    """Rule to create .bundle for macOS.

    Args:
      name: name for macos_bundle.
      bundle_name: bundle_name for macos_bundle.
      infoplists: infoplists are tweaked and applied to macos_bundle.
      strings: strings are tweaked and applied to macos_bundle.
      bundle_id: bundle_id for macos_bundle.
      tags: tags for macos_bundle.
      **kwargs: other arguments for macos_bundle.
    """
    macos_bundle(
        name = name,
        bundle_id = bundle_id or (MACOS_BUNDLE_ID_PREFIX + "." + bundle_name),
        bundle_name = bundle_name,
        infoplists = _tweak_infoplists(name, infoplists),
        strings = _tweak_strings(name, strings),
        minimum_os_version = MACOS_MIN_OS_VER,
        version = "//data/version:version_macos",
        target_compatible_with = ["@platforms//os:macos"],
        tags = tags + MOZC_TAGS.MAC_ONLY,
        **kwargs
    )
