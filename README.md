**A high-performance gateway data plane based on the open source network proxy [Envoy Proxy](https://github.com/envoyproxy/envoy).**

## BUILD

This repo maintains the same compilation environment requirements as the open source network proxy Envoy Proxy. You can refer to [Envoy Proxy Community](https://www.envoyproxy.io/docs/envoy/latest/start/building) for the basic requirements of compilation and construction.

**It is more recommended to perform the build and compile in the build container.**

Before starting to build, you need to initialize the Envoy submodule.

```shell
git submodule init
git submodule update
```

Execute the following command to build the local binary:

```shell
./ci/do_ci.sh compile
```

Execute the following command to build the container image:

```shell
./ci/do_ci.sh envoy-release
```

## Thanks

- [Envoy Proxy](https://github.com/envoyproxy/envoy): The entire repo is built on the basis of Envoy, not only includes the Envoy main body, but also includes a large number of tool scripts derived from Envoy.

- [Istio](https://github.com/istio/istio): The data plane uses the Pilot-agent module in Istio to manage the Envoy life cycle in the container.
