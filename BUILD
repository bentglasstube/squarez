package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "squarez",
    data = ["//content"],
    linkopts = [
        "-lSDL2",
        "-static-libstdc++",
        "-static-libgcc",
    ],
    srcs = ["main.cc"],
    deps = [
        "@libgam//:game",
        ":config",
        ":game_screen",
    ],
)

cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = ["@libgam//:game"],
)

cc_library(
    name = "game_screen",
    srcs = ["game_screen.cc"],
    hdrs = ["game_screen.h"],
    deps = [
        "@libgam//:screen",
        "@libgam//:util",
        "@entt//:entt",
        ":components",
        ":config",
    ],
)

cc_library(
    name = "components",
    hdrs = ["components.h"],
    deps = [":geometry"],
)

cc_library(
    name = "geometry",
    hdrs = ["geometry.h"],
)
