load(
    "//:build_defs.bzl",
    "mozc_cc_binary",
    "mozc_cc_library",
)

package(default_visibility = ["//:__subpackages__"])

mozc_cc_library(
    name = "mozc_connection",
    srcs = ["mozc_connection.cc"],
    hdrs = ["mozc_connection.h"],
    deps = [
        ":fcitx_key_util",
        ":surrounding_text_util",
        "//base:port",
        "//base:util",
        "//base:vlog",
        "//client:client",
        "//protocol:commands_cc_proto",
    ]
)

mozc_cc_library(
    name = "fcitx_mozc",
    srcs = [
        "fcitx_mozc.cc",
        "mozc_response_parser.cc",
    ],
    hdrs = [
        "fcitx_mozc.h",
        "mozc_response_parser.h",
    ],
    deps = [
        ":mozc_connection",
        ":fcitx_key_util",
        ":surrounding_text_util",
        "//base:port",
        "//base:process",
        "//base:run_level",
        "//base:util",
        "//base:vlog",
        "//protocol:commands_cc_proto",
        "//client:client_interface",
    ],
)

mozc_cc_library(
    name = "fcitx_key_util",
    srcs = [
        "fcitx_key_event_handler.cc",
        "fcitx_key_translator.cc",
    ],
    hdrs = [
        "fcitx_key_event_handler.h",
        "fcitx_key_translator.h",
    ],
    deps = [
        "//base:logging",
        "//base:port",
        "//base:vlog",
        "//protocol:config_cc_proto",
        "//protocol:commands_cc_proto",
        "@fcitx//:fcitx",
    ],
)

mozc_cc_library(
    name = "surrounding_text_util",
    srcs = [
        "surrounding_text_util.cc",
    ],
    hdrs = [
        "surrounding_text_util.h",
    ],
    deps = [
        "//base:util",
        "//base:logging",
        "//base:port",
        "//base:vlog",
        "@fcitx//:fcitx",
    ],
)

mozc_cc_binary(
    name = "fcitx-mozc.so",
    srcs = ["eim.cc"],
    deps = [
        ":fcitx_mozc",
        "//base:init_mozc",
        "@fcitx//:fcitx",
    ],
    local_defines = [
        'LOCALEDIR=\\"/usr/share/locale\\"',
    ],
    linkstatic = 1,
    linkshared = 1,
)

