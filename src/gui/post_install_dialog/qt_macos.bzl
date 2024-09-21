load(
    "mozc_macos_qt_application",
)

mozc_macos_qt_application(
    name = "post_install_dialog_macos",
    bundle_name = "PostInstallDialog",
    deps = [":post_install_dialog_main_lib"],
)
