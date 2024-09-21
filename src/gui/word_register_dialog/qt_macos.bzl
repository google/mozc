load(
	"mozc_macos_qt_application",
)

mozc_macos_qt_application(
    name = "word_register_dialog_macos",
    bundle_name = "WordRegisterDialog",
    deps = [":word_register_dialog_main_lib"],
)
