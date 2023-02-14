**基于开源网络代理 [Envoy Proxy](https://github.com/envoyproxy/envoy) 构建的高性能网关数据面。**

## 构建

该工程和开源网络代理 Envoy Proxy 保持相同的编译环境要求。可以参考 [Envoy Proxy 社区](https://www.envoyproxy.io/docs/envoy/latest/start/building) 关于编译构建的基础要求。

**更加建议在构建容器之中执行构建和编译。**

在开始构建之前，需要初始化 Envoy submodule。

```shell
git submodule init
git submodule update
```

执行以下命令构建本地二进制：

```shell
./ci/do_ci.sh compile
```

执行以下命令构建容器镜像：

```shell
# Export `ENVOY_PROXY_HUB` ENV to specify image repo.
export ENVOY_PROXY_HUB="hangoio/envoy-proxy"
# Build container image.
./ci/do_ci.sh envoy-release
```

## 致谢

- [Envoy Proxy](https://github.com/envoyproxy/envoy)：整个工程都构建在 Envoy 基础之上，不单单包括 Proxy 主体，同时也包括大量的工具脚本也源于 Envoy。

- [Istio](https://github.com/istio/istio)：数据面使用 Istio 中 Pilot-agent 模块来管理容器中 Envoy 生命周期。
