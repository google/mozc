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
    name = "mozc_client_pool",
    srcs = ["mozc_client_pool.cc"],
    hdrs = ["mozc_client_pool.h"],
    deps = [
        ":mozc_connection",
        "@fcitx5//:fcitx5",
    ]
)

mozc_cc_library(
    name = "mozc_engine",
    srcs = [
        "mozc_engine.cc",
        "mozc_response_parser.cc",
        "mozc_state.cc"
    ],
    hdrs = [
        "mozc_engine.h",
        "mozc_response_parser.h",
        "mozc_state.h"
    ],
    local_defines = [
        'FCITX_GETTEXT_DOMAIN=\\"fcitx5-mozc\\"',
    ],
    deps = [
        ":mozc_connection",
        ":mozc_client_pool",
        ":fcitx_key_util",
        ":surrounding_text_util",
        "//base:init_mozc",
        "//base:port",
        "//base:process",
        "//base:run_level",
        "//base:util",
        "//base:vlog",
        "//protocol:commands_cc_proto",
        "//client:client_interface",
        "@fcitx5//:fcitx5",
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
        "//base:singleton",
        "//protocol:config_cc_proto",
        "//protocol:commands_cc_proto",
        "@fcitx5//:fcitx5",
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
        "@fcitx5//:fcitx5",
    ],
)

mozc_cc_binary(
    name = "fcitx5-mozc.so",
    srcs = [
        "mozc_engine_factory.cc"
    ],
    deps = [
        ":mozc_engine",
    ],
    linkstatic = 1,
    linkshared = 1,
)

