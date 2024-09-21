load(
	"mozc_macos_qt_application",
)

mozc_macos_qt_application(
    name = "dictionary_tool_macos",
    bundle_name = "DictionaryTool",
    deps = [":dictionary_tool_main_lib"],
)
