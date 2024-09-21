load("@build_bazel_rules_android//android:rules.bzl", "android_sdk_repository")
android_sdk_repository(
    name = "androidsdk",
)
android_ndk_repository_extension = use_extension(
    "@rules_android_ndk//:extension.bzl",
    "android_ndk_repository_extension",
)
use_repo(android_ndk_repository_extension, "androidndk")
register_toolchains("@androidndk//:all")


load("@//bazel:android_repository.bzl", "android_repository")
android_repository(name = "android_repository")
load("@android_repository//:setup.bzl", "android_ndk_setup")
android_ndk_setup()


# Placeholder of rules_android for the compatibility with Bzlmod.
new_local_repository(
    name = "rules_android",
    path = "bazel",
    build_file_content = "",
)

remote_android_extensions = use_extension(
    "@rules_android//bzlmod_extensions:android_extensions.bzl",
    "remote_android_tools_extensions",
)
use_repo(remote_android_extensions, "android_gmaven_r8", "android_tools")
register_toolchains(
"@rules_android//toolchains/android:android_default_toolchain",
"@rules_android//toolchains/android_sdk:android_sdk_tools",
)
android_sdk_repository_extension = use_extension(
    "@rules_android//rules/android_sdk_repository:rule.bzl",
    "android_sdk_repository_extension",
)
use_repo(android_sdk_repository_extension, "androidsdk")
register_toolchains("@androidsdk//:sdk-toolchain", "@androidsdk//:all")

new_local_repository = use_repo_rule(
    "@bazel_tools//tools/build_defs/repo:local.bzl",
    "new_local_repository",
)

