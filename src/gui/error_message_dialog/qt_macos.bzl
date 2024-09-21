load(
	"mozc_macos_qt_application",
)

mozc_macos_qt_application(
    name = "error_message_dialog_macos",
    bundle_name = "ErrorMessageDialog",
    deps = [":error_message_dialog_main_lib"],
)
