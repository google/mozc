load(
    "mozc_macos_qt_application",
)
mozc_macos_qt_application(
    name = "mozc_tool_macos",
    bundle_name = "MozcTool",
    deps = [":mozc_tool_main_lib"],
)

mozc_macos_qt_application(
    name = "mozc_prelauncher_macos",
    bundle_name = BRANDING + "Prelauncher",
    deps = [":mozc_tool_main_lib"],
)
