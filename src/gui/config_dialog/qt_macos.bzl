load(
    "mozc_macos_qt_application",
)

mozc_macos_qt_application(
    name = "config_dialog_macos",
    bundle_name = "ConfigDialog",
    deps = [":config_dialog_main_lib"],
)
