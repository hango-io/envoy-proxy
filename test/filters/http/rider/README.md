运行该测试之前需要手动修改下 `//envoy/bazel/envoy_test.bzl`(后续可以把这个修改提给上游)

```
vscode@a6ab460dc125:/workspaces/envoy-proxy/envoy$ git diff bazel/envoy_test.bzl 
diff --git a/bazel/envoy_test.bzl b/bazel/envoy_test.bzl
index ac6c3886d..b36e573c8 100644
--- a/bazel/envoy_test.bzl
+++ b/bazel/envoy_test.bzl
@@ -67,7 +67,7 @@ def _envoy_test_linkopts():
         # TODO(mattklein123): It's not great that we universally link against the following libs.
         # In particular, -latomic and -lrt are not needed on all platforms. Make this more granular.
         "//conditions:default": ["-pthread", "-lrt", "-ldl"],
-    }) + envoy_select_force_libcpp([], ["-lstdc++fs", "-latomic"])
+    }) + envoy_select_force_libcpp([], ["-lstdc++fs", "-latomic"]) + ["-Wl,-E"]
 
 # Envoy C++ fuzz test targets. These are not included in coverage runs.
 def envoy_cc_fuzz_test(
```

```
bazel test //test/filters/http/rider/... --define  --exported_symbols=enabled
```

