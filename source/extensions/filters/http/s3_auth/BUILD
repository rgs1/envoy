licenses(["notice"])  # Apache 2

# L7 HTTP S3 auth filter
# Public docs: docs/root/configuration/http_filters/s3_auth_filter.rst

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "s3_auth_filter_lib",
    srcs = ["s3_auth_filter.cc"],
    hdrs = ["s3_auth_filter.h"],
    deps = [
        "//include/envoy/http:filter_interface",
        "@envoy_api//envoy/extensions/filters/http/s3_auth/v3:pkg_cc_proto",
    ],
)

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    security_posture = "requires_trusted_downstream_and_upstream",
    status = "alpha",
    deps = [
        ":s3_auth_filter_lib",
        "//include/envoy/registry",
        "//source/extensions/filters/http:well_known_names",
        "//source/extensions/filters/http/common:factory_base_lib",
        "@envoy_api//envoy/extensions/filters/http/s3_auth/v3:pkg_cc_proto",
    ],
)
