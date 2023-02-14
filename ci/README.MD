## 多架构环境

首先必须确保机器中 QEMU 已经安装并启动 buildx driver：

```
$ sudo apt install -y qemu-user-static binfmt-support
$ docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
$ docker buildx create --use
```


## 构建基础镜像

执行如下命令可以获得基础镜像名称：


```
$ export ENVOY_PROXY_IMAGE_HUB=hub.cloud.com/hub
$ ./ci/base_image # hub.cloud.com/hub/envoy-base:<tag>
```

**注意，该名称不直接指向任何镜像，需要添加架构后缀 "-amd64/-arm64"。**

执行如下命令可以构建多架构基础镜像：


```
$ export ENVOY_PROXY_IMAGE_HUB=hub.cloud.com/hub
$ ./ci/base_image build_base_image
```

## 构建执行镜像

`ci/docker_ci.sh` 提供了一键命令输出 Envoy 镜像，但是需要注意，该命令将根据执行脚本所运行的架构平台输出对应镜像。
但是，在一台安装好了 QEMU 的机器上，可以通过指定 build 镜像架构，来构建多架构镜像。

具体来来说，在安装好 QEMU 的机器上，即是机器是 AMD64 架构，也同样可以利用 QEMU 模拟器直接运行 ARM64 的镜像。所以只需要指定运行 build 镜像的架构，然后在 build 镜像中执行二进制编译即可输出对应架构镜像。可以查看 `.gitlab-ci.yml` 文件中 `release` 阶段作为参考。

注意，构建 Envoy 镜像并未使用和构建基础镜像一样的 buildx 命令，因为构建 Envoy 镜像，如果使用 buildx 命令，需要采用多阶段构建，在本地开发过程当中，对本地编译缓存并不友好。


## 其他注意事项

如果出现类似 `qemu: qemu_thread_create: operation not permitted` 错误，可以通过只用如下 docker 参数规避：`--security-opt seccomp=unconfined`。
如果 docker 命令不支持对应参数，则需要通过 dockerd `--seccomp-profile` 参数指定默认的安全选项，参数值为一个指向包含空 Json `{}` 的配置文件 。不过该操作需要重启 dockerd，不到不得以不推荐。
