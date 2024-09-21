load(
    "mozc_macos_application",
)
load(
    "MACOS_BUNDLE_ID_PREFIX",
)

# macOS
mozc_macos_application(
    name = "mozc_renderer_macos",
    app_icons = ["//data/images/mac:product_icon.icns"],
    bundle_id = MACOS_BUNDLE_ID_PREFIX + ".Renderer",
    bundle_name = BRANDING + "Renderer",
    infoplists = ["mac/Info.plist"],
    resources = ["//data/images/mac:candidate_window_logo.tiff"],
    deps = mozc_select(macos = [":mozc_renderer_main_macos"]),
    # When we support Breakpad, uncomment the following block.
    # additional_contents = {
    #     "[Breakpad]" : "Frameworks",
    # },
)

mozc_cc_library(
    name = "mozc_renderer_main_macos",
    srcs = mozc_select(macos = ["mac/mac_renderer_main.cc"]),
    deps = [
        "//renderer:init_mozc_renderer",
    ] + mozc_select(
        macos = [":mozc_renderer_lib_macos"],
    ),
)

mozc_objc_library(
    name = "mozc_renderer_lib_macos",
    srcs = [
        "mac/CandidateController.mm",
        "mac/CandidateView.mm",
        "mac/CandidateWindow.mm",
        "mac/InfolistView.mm",
        "mac/InfolistWindow.mm",
        "mac/RendererBaseWindow.mm",
        "mac/mac_server.mm",
        "mac/mac_server_send_command.mm",
        "mac/mac_view_util.mm",
    ],
    hdrs = [
        "mac/CandidateController.h",
        "mac/CandidateView.h",
        "mac/CandidateWindow.h",
        "mac/InfolistView.h",
        "mac/InfolistWindow.h",
        "mac/RendererBaseWindow.h",
        "mac/mac_server.h",
        "mac/mac_server_send_command.h",
        "mac/mac_view_util.h",
    ],
    sdk_frameworks = [
        "Carbon",
        "Cocoa",
    ],
    deps = [
        ":renderer_interface",
        ":renderer_server",
        ":renderer_style_handler",
        ":table_layout",
        ":window_util",
        "//base:const",
        "//base:coordinates",
        "//base:util",
        "//base/mac:mac_util",
        "//client:client_interface",
        "//mac:common",
        "//protocol:candidates_cc_proto",
        "//protocol:commands_cc_proto",
        "//protocol:renderer_cc_proto",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

