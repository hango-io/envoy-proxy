workspace(name = "envoy_netease")

#######################################################################################
# 不要轻易修改 WORKSPACE 文件中命令顺序以及内容，以免影响到最终工程构建                        #
# Do not easily modify the order and content of commands in the WORKSPACE file,       #
# so as not to affect the final project construction                                  #
#######################################################################################

######################################## Envoy ########################################
local_repository(
    name = "envoy",
    path = "envoy",
)

##################################### Deps of Envoy ###################################
load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:repositories_extra.bzl", "envoy_dependencies_extra")

envoy_dependencies_extra()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()

##################################### Deps archives ###################################

load("//:distfiles.bzl", "additional_distfiles_imports")

additional_distfiles_imports()

##################################### Envoy Proxy ###################################
load("//bazel:repositories.bzl", "envoy_netease_dependencies")

envoy_netease_dependencies()
