load(
	"mozc_macos_qt_application",
)

mozc_macos_qt_application(
    name = "about_dialog_macos",
    bundle_name = "AboutDialog",
    deps = [":about_dialog_main_lib"],
)
