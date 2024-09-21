load(
	"macos_application",
)
load(
    "MACOS_BUNDLE_ID_PREFIX",
)

mozc_macos_application(
    name = "mozc_server_macos",
    app_icons = ["//data/images/mac:product_icon.icns"],
    bundle_id = MACOS_BUNDLE_ID_PREFIX + ".Converter",
    bundle_name = BRANDING + "Converter",
    infoplists = ["Info.plist"],
    deps = [":mozc_server_lib"],
    # When we support Breakpad, uncomment the following block.
    # additional_contents = {
    #     "[Breakpad]" : "Frameworks",
    # },
)
