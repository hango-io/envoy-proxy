load("//:distdir.bzl", "distdir_tar")

def additional_distfiles_imports(): 
    distdir_tar(
        name = "additional_distfiles",
        # Keep in sync with the archives fetched as part of building bazel.
        archives = [
            "0.2.2.tar.gz",
            "0.3.9.4.tar.gz",
            "0.4.1.tar.gz",
            "015fc86d90f4045a56f831bcdfa560bc455450e2.tar.gz",
            "0d5f3f2768c6ca2faca0079a997a97ce22997a0c.zip",
            "0d7d85c2083f7a4c9efe01c061486f332b576d28.tar.gz",
            "1.1.1.tar.gz",
            "14550beb3b7b97195e483fb74b5efb906395c31e.tar.gz",
            "2.10.3.tar.gz",
            "2.2.0-rc2.zip",
            "2019-09-01.tar.gz",
            "2feabd5d64436e670084091a937855972ee35161.tar.gz",
            "4abb566fbbc63df8fe7c1ac30b21632b9eb18d0c.tar.gz",
            "5565939d4203234ddc742c02241ce4523e7b3beb.tar.gz",
            "5cec5ea58c3efa81fa808f2bd38ce182da9ee731.tar.gz",
            "5f50c68bdf5f107692bb027d1c568f67597f4d7f.tar.gz",
            "63a16dd6f2fc7bc841bb17ff92be8318df60e2e1.tar.gz",
            "74ad4281edd4ceca658888602af74bf2050107f0.tar.gz",
            "79baebe50e4d6b73ae1f8b603f0ef41300110aa3.tar.gz",
            "7bc4be735b0560289f6b86ab6136ee25d20b65b7.tar.gz",
            "7cf3cefd652008d0a64a419c34c13bdca6c8f178.zip",
            "97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
            "98970f78091ae65b4a029bcf512696ba6d665cf4.tar.gz",
            "99fa1f4be8e564e8a6b613da7fa6f46c9edafc6c.tar.gz",
            "9f10e2d60d42edeb6662e185707a7d6a4ebc5604.tar.gz",
            "a6b28555badcb18d6be924c8fc1bea49971656b8.tar.gz",
            "b4d7f8e1aa1ebbe75aaf959d93caf5826599c9ab.tar.gz",
            "bazel-gazelle-v0.19.0.tar.gz",
            "bazel-skylib.0.8.0.tar.gz",
            "bazel-toolchains-1.1.0.tar.gz",
            "be480e391cc88a75cf2a81960ef79c80d5012068.tar.gz",
            "boringssl-66005f41fbc3529ffe8d007708756720529da20d.tar.xz",
            "compiler-rt-9.0.0.src.tar.xz",
            "curl-7.66.0.tar.gz",
            "d1fe8a7d8ae18f3d454f055eba5213c291986f21.tar.gz",
            "d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe.tar.gz",
            "d35267580568517f09bdf70cb582e5284c25401a.tar.gz",
            "d5b4c69b7113213c1da3a0ccbfd1ee1b40443c7a.tar.gz",
            "d7003576dd133856432e2e07340f45926242cc3a.tar.gz",
            "d7e070e7283f822b1d2787903cce3615536c5610.tar.gz",
            "d85f82972c2def6db9c90f3d9a23f56a0ac3caff.tar.gz",
            "fc00474ddc21fff618fc3f009b46590e241e425e.tar.gz",
            "fd7de029969b7c0ef8b754660b997399b6fd812a.tar.gz",
            "fmt-5.3.0.zip",
            "msgpack-3.2.0.tar.gz",
            "nghttp2-1.39.1.tar.gz",
            "protobuf-all-3.9.2.tar.gz",
            "rules_go-v0.20.1.tar.gz",
            "six-1.10.0.tar.gz",
            "tclap-1-2-1-release-final.tar.gz",
            "thrift-0.11.0.tar.gz",
            "twitter.common.finagle-thrift-0.3.9.tar.gz",
            "twitter.common.lang-0.3.9.tar.gz",
            "twitter.common.rpc-0.3.9.tar.gz",
            "v0.7.2.tar.gz",
            "v0.8.0.tar.gz",
            "v1.1.0.tar.gz",
            "v1.1.1.tar.gz",
            "v1.22.1.tar.gz",
            "v1.3.1.tar.gz",
            "v1.5.0.tar.gz",
            "v1.5.1.tar.gz",
            "v2.1.0-beta3.tar.gz",
            "v2.9.0.tar.gz",
            "v8.1.0.tar.gz",
            "yaml-cpp-0.6.3.tar.gz",
            ],
        dirname = "derived/distdir",
        sha256 = {
            "0.2.2.tar.gz": "688c4fe170821dd589f36ec45aaadc03a618a40283bc1f97da8fa11686fc816b",
            "0.3.9.4.tar.gz": "cbc8fba028635d959033c9ba8d8186a713165e94a9de02a030a20b3e64866a04",
            "0.4.1.tar.gz": "801b35d996a097d223e028815fdba8667bf62bc5efb353486603d31fc2ba6ff9",
            "015fc86d90f4045a56f831bcdfa560bc455450e2.tar.gz": "2f2b4bdb718250531f3ed9c2010272f04bbca92af70348714fd3687e86acc1f7",
            "0d5f3f2768c6ca2faca0079a997a97ce22997a0c.zip": "36fa66d4d49debd71d05fba55c1353b522e8caef4a20f8080a3d17cdda001d89",
            "0d7d85c2083f7a4c9efe01c061486f332b576d28.tar.gz": "549d34065eb2485dfad6c8de638caaa6616ed130eec36dd978f73b6bdd5af113",
            "1.1.1.tar.gz": "222a10e3237d92a9cd45ed5ea882626bc72bc5e0264d3ed0f2c9129fa69fc167",
            "14550beb3b7b97195e483fb74b5efb906395c31e.tar.gz": "3df5970908ed9a09ba51388d04661803a6af18c373866f442cede7f381e0b94a",
            "2.10.3.tar.gz": "db49236731373e4f3118af880eb91bb0aa6978bc0cf8b35760f6a026f1a9ffc4",
            "2.2.0-rc2.zip": "ae7a1696c0a0302b43c5b21e515c37e6ecd365941f68a510a7e442eebddf39a1",
            "2019-09-01.tar.gz": "b0382aa7369f373a0148218f2df5a6afd6bfa884ce4da2dfb576b979989e615e",
            "2feabd5d64436e670084091a937855972ee35161.tar.gz": "a447458b47ea4dc1d31499f555769af437c5d129d988ec1e13d5fdd0a6a36b4e",
            "4abb566fbbc63df8fe7c1ac30b21632b9eb18d0c.tar.gz": "c60bca3cf7f58b91394a89da96080657ff0fbe4d5675be9b21e90da8f68bc06f",
            "5565939d4203234ddc742c02241ce4523e7b3beb.tar.gz": "891352824e0f7977bc0c291b8c65076e3ed23630334841b93f346f12d4484b06",
            "5cec5ea58c3efa81fa808f2bd38ce182da9ee731.tar.gz": "faeb93f293ff715b0cb530d273901c0e2e99277b9ed1c0a0326bca9ec5774ad2",
            "5f50c68bdf5f107692bb027d1c568f67597f4d7f.tar.gz": "425dfee0c4fe9aff8acf2365cde3dd2ba7fb878d2ba37562d33920e34c40c05e",
            "63a16dd6f2fc7bc841bb17ff92be8318df60e2e1.tar.gz": "8165aa25e529d7d4b9ae849d3bf30371255a99d6db0421516abcff23214cdc2c",
            "74ad4281edd4ceca658888602af74bf2050107f0.tar.gz": "881e12d01c1703060343cd761164c98c731ef8dd52be4ee71e5501a87a2c0c1f",
            "79baebe50e4d6b73ae1f8b603f0ef41300110aa3.tar.gz": "155a8f8c1a753fb05b16a1b0cc0a0a9f61a78e245f9e0da483d13043b3bcbf2e",
            "7bc4be735b0560289f6b86ab6136ee25d20b65b7.tar.gz": "3184c244b32e65637a74213fc448964b687390eeeca42a36286f874c046bba15",
            "7cf3cefd652008d0a64a419c34c13bdca6c8f178.zip": "bc81f1ba47ef5cc68ad32225c3d0e70b8c6f6077663835438da8d5733f917598",
            "97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz": "602e7161d9195e50246177e7c55b2f39950a9cf7366f74ed5f22fd45750cd208",
            "98970f78091ae65b4a029bcf512696ba6d665cf4.tar.gz": "c95ab57835182b8b4b17cf5bbfc2406805bc78c5022c17399f3e5c643f22826a",
            "99fa1f4be8e564e8a6b613da7fa6f46c9edafc6c.tar.gz": "783bdaf8ee0464b35ec0c8704871e1e72afa0005c3f3587f65d9d6694bf3911b",
            "9f10e2d60d42edeb6662e185707a7d6a4ebc5604.tar.gz": "480ff74bcdea9e177a803feb8f797c76abd38a80ec27d93b64f4d56e7cfd28a1",
            "a6b28555badcb18d6be924c8fc1bea49971656b8.tar.gz": "d0f88bef8bd7f76c3684407977f5673f3d06a6c50d4ddaffb8f0e7df6b0ef69e",
            "b4d7f8e1aa1ebbe75aaf959d93caf5826599c9ab.tar.gz": "9aab6a1f907e05b18c4a4578f79fd93b028a9c944555db9e7d732222a46dafa8",
            "bazel-gazelle-v0.19.0.tar.gz": "41bff2a0b32b02f20c227d234aa25ef3783998e5453f7eade929704dcff7cd4b",
            "bazel-skylib.0.8.0.tar.gz": "2ef429f5d7ce7111263289644d233707dba35e39696377ebab8b0bc701f7818e",
            "bazel-toolchains-1.1.0.tar.gz": "1e16833a9f0e32b292568c0dfee7bd48133c2038605757d3a430551394310006",
            "be480e391cc88a75cf2a81960ef79c80d5012068.tar.gz": "c1969e5b72eab6d9b6cfcff748e45ba57294aeea1d96fd04cd081995de0605c2",
            "boringssl-66005f41fbc3529ffe8d007708756720529da20d.tar.xz": "b12ad676ee533824f698741bd127f6fbc82c46344398a6d78d25e62c6c418c73",
            "compiler-rt-9.0.0.src.tar.xz": "56e4cd96dd1d8c346b07b4d6b255f976570c6f2389697347a6c3dcb9e820d10e",
            "curl-7.66.0.tar.gz": "d0393da38ac74ffac67313072d7fe75b1fa1010eb5987f63f349b024a36b7ffb",
            "d1fe8a7d8ae18f3d454f055eba5213c291986f21.tar.gz": "f45c3ad82376d891cd0bcaa7165e83efd90e0014b00aebf0cbaf07eb05a1d3f9",
            "d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe.tar.gz": "ca81b8e467543383b04a33d125425f2a584a3df978761329eaa3a9aef947e594",
            "d35267580568517f09bdf70cb582e5284c25401a.tar.gz": "0644a5744765eb2fcb23b730f5643dd4c7391c9a093b731c3002fb5a6617ef40",
            "d5b4c69b7113213c1da3a0ccbfd1ee1b40443c7a.tar.gz": "335a761fac2dbae219c68c23e674bf5ddff43f99521b35bb63a1ab342e50931e",
            "d7003576dd133856432e2e07340f45926242cc3a.tar.gz": "cbd251a40485fddd44cdf641af6df2953d45695853af6d68aeb11c7efcde6771",
            "d7e070e7283f822b1d2787903cce3615536c5610.tar.gz": "bbaab13d6ad399a278d476f533e4d88a7ec7d729507348bb9c2e3b207ba4c606",
            "d85f82972c2def6db9c90f3d9a23f56a0ac3caff.tar.gz": "e21d11be5eca677fe79839d310ceffb2f950d9d03f7682af8c0d311e573a1302",
            "fc00474ddc21fff618fc3f009b46590e241e425e.tar.gz": "18574813a062eee487bc1b761e8024a346075a7cb93da19607af362dc09565ef",
            "fd7de029969b7c0ef8b754660b997399b6fd812a.tar.gz": "55c6ad4a1b405938524ab55b18349c824d3fc6eaef580e1ef2a9dfe39f737b9e",
            "fmt-5.3.0.zip": "4c0741e10183f75d7d6f730b8708a99b329b2f942dad5a9da3385ab92bb4a15c",
            "msgpack-3.2.0.tar.gz": "fbaa28c363a316fd7523f31d1745cf03eab0d1e1ea5a1c60aa0dffd4ce551afe",
            "nghttp2-1.39.1.tar.gz": "25b623cd04dc6a863ca3b34ed6247844effe1aa5458229590b3f56a6d53cd692",
            "protobuf-all-3.9.2.tar.gz": "7c99ddfe0227cbf6a75d1e75b194e0db2f672d2d2ea88fb06bdc83fe0af4c06d",
            "rules_go-v0.20.1.tar.gz": "842ec0e6b4fbfdd3de6150b61af92901eeb73681fd4d185746644c338f51d4c0",
            "six-1.10.0.tar.gz": "105f8d68616f8248e24bf0e9372ef04d3cc10104f1980f54d57b2ce73a5ad56a",
            "tclap-1-2-1-release-final.tar.gz": "f0ede0721dddbb5eba3a47385a6e8681b14f155e1129dd39d1a959411935098f",
            "thrift-0.11.0.tar.gz": "7d59ac4fdcb2c58037ebd4a9da5f9a49e3e034bf75b3f26d9fe48ba3d8806e6b",
            "twitter.common.finagle-thrift-0.3.9.tar.gz": "1e3a57d11f94f58745e6b83348ecd4fa74194618704f45444a15bc391fde497a",
            "twitter.common.lang-0.3.9.tar.gz": "56d1d266fd4767941d11c27061a57bc1266a3342e551bde3780f9e9eb5ad0ed1",
            "twitter.common.rpc-0.3.9.tar.gz": "0792b63fb2fb32d970c2e9a409d3d00633190a22eb185145fe3d9067fdaa4514",
            "v0.7.2.tar.gz": "7e93d28e81c3e95ff07674a400001d0cdf23b7842d49b211e5582d00d8e3ac3e",
            "v0.8.0.tar.gz": "defbf471facfebde6523ca1177529b63784893662d4ef2c60db074be8aef0634",
            "v1.1.0.tar.gz": "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
            "v1.1.1.tar.gz": "052fd37cd698e24ab73ee18fc3fa55acd1d43153c12a0e65b0fba0447de1117e",
            "v1.22.1.tar.gz": "cce1d4585dd017980d4a407d8c5e9f8fc8c1dbb03f249b99e88a387ebb45a035",
            "v1.3.1.tar.gz": "160845266e94db1d4922ef755637f6901266731c4cb3b30b45bf41efa0e6ab70",
            "v1.5.0.tar.gz": "3c6a165b6ecc948967a1ead710d4a181d7b0fbcaa183ef7ea84604994966221a",
            "v1.5.1.tar.gz": "015c4187f7a6426a2b5196f0ccd982aa87f010cf61f507ae3ce5c90523f92301",
            "v2.1.0-beta3.tar.gz": "409f7fe570d3c16558e594421c47bdd130238323c9d6fd6c83dedd2aaeb082a8",
            "v2.9.0.tar.gz": "ef26268c54c8084d17654ba2ed5140bffeffd2a040a895ffb22a6cca3f6c613f",
            "v8.1.0.tar.gz": "ebea8a6968722524d1bcc4426fb6a29907ddc2902aac7de1559012d3eee90cf9",
            "yaml-cpp-0.6.3.tar.gz": "77ea1b90b3718aa0c324207cb29418f5bced2354c2e483a9523d98c3460af1ed",
            },
        urls = {
            "0.2.2.tar.gz": [
            "https://github.com/openzipkin/zipkin-api/archive/0.2.2.tar.gz",
            ], 
            "0.3.9.4.tar.gz": [
            "https://github.com/nanopb/nanopb/archive/0.3.9.4.tar.gz",
            ], 
            "0.4.1.tar.gz": [
            "https://github.com/grailbio/bazel-compilation-database/archive/0.4.1.tar.gz",
            ], 
            "015fc86d90f4045a56f831bcdfa560bc455450e2.tar.gz": [
            "https://github.com/cncf/udpa/archive/015fc86d90f4045a56f831bcdfa560bc455450e2.tar.gz",
            ], 
            "0d5f3f2768c6ca2faca0079a997a97ce22997a0c.zip": [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_cc/archive/0d5f3f2768c6ca2faca0079a997a97ce22997a0c.zip",
            ], 
            "0d7d85c2083f7a4c9efe01c061486f332b576d28.tar.gz": [
            "https://github.com/libevent/libevent/archive/0d7d85c2083f7a4c9efe01c061486f332b576d28.tar.gz",
            ], 
            "1.1.1.tar.gz": [
            "https://github.com/pallets/markupsafe/archive/1.1.1.tar.gz",
            ], 
            "14550beb3b7b97195e483fb74b5efb906395c31e.tar.gz": [
            "https://github.com/abseil/abseil-cpp/archive/14550beb3b7b97195e483fb74b5efb906395c31e.tar.gz",
            ], 
            "2.10.3.tar.gz": [
            "https://github.com/pallets/jinja/archive/2.10.3.tar.gz",
            ], 
            "2.2.0-rc2.zip": [
            "https://github.com/apache/kafka/archive/2.2.0-rc2.zip",
            ], 
            "2019-09-01.tar.gz": [
            "https://github.com/google/re2/archive/2019-09-01.tar.gz",
            ], 
            "2feabd5d64436e670084091a937855972ee35161.tar.gz": [
            "https://github.com/grpc-ecosystem/grpc-httpjson-transcoding/archive/2feabd5d64436e670084091a937855972ee35161.tar.gz",
            ], 
            "4abb566fbbc63df8fe7c1ac30b21632b9eb18d0c.tar.gz": [
            "https://storage.googleapis.com/quiche-envoy-integration/4abb566fbbc63df8fe7c1ac30b21632b9eb18d0c.tar.gz",
            ], 
            "5565939d4203234ddc742c02241ce4523e7b3beb.tar.gz": [
            "https://github.com/google/boringssl/archive/5565939d4203234ddc742c02241ce4523e7b3beb.tar.gz",
            ], 
            "5cec5ea58c3efa81fa808f2bd38ce182da9ee731.tar.gz": [
            "https://github.com/census-instrumentation/opencensus-proto/archive/5cec5ea58c3efa81fa808f2bd38ce182da9ee731.tar.gz",
            ], 
            "5f50c68bdf5f107692bb027d1c568f67597f4d7f.tar.gz": [
            "https://github.com/envoyproxy/sql-parser/archive/5f50c68bdf5f107692bb027d1c568f67597f4d7f.tar.gz",
            ], 
            "63a16dd6f2fc7bc841bb17ff92be8318df60e2e1.tar.gz": [
            "https://github.com/circonus-labs/libcircllhist/archive/63a16dd6f2fc7bc841bb17ff92be8318df60e2e1.tar.gz",
            ], 
            "74ad4281edd4ceca658888602af74bf2050107f0.tar.gz": [
            "https://github.com/pantor/inja/archive/74ad4281edd4ceca658888602af74bf2050107f0.tar.gz",
            ], 
            "79baebe50e4d6b73ae1f8b603f0ef41300110aa3.tar.gz": [
            "https://github.com/madler/zlib/archive/79baebe50e4d6b73ae1f8b603f0ef41300110aa3.tar.gz",
            ], 
            "7bc4be735b0560289f6b86ab6136ee25d20b65b7.tar.gz": [
            "https://github.com/bazelbuild/rules_foreign_cc/archive/7bc4be735b0560289f6b86ab6136ee25d20b65b7.tar.gz",
            ], 
            "7cf3cefd652008d0a64a419c34c13bdca6c8f178.zip": [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_java/archive/7cf3cefd652008d0a64a419c34c13bdca6c8f178.zip",
            ], 
            "97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz": [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
            "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
            ], 
            "98970f78091ae65b4a029bcf512696ba6d665cf4.tar.gz": [
            "https://github.com/census-instrumentation/opencensus-cpp/archive/98970f78091ae65b4a029bcf512696ba6d665cf4.tar.gz",
            ], 
            "99fa1f4be8e564e8a6b613da7fa6f46c9edafc6c.tar.gz": [
            "https://github.com/prometheus/client_model/archive/99fa1f4be8e564e8a6b613da7fa6f46c9edafc6c.tar.gz",
            ], 
            "9f10e2d60d42edeb6662e185707a7d6a4ebc5604.tar.gz": [
            "https://github.com/google/jwt_verify_lib/archive/9f10e2d60d42edeb6662e185707a7d6a4ebc5604.tar.gz",
            ], 
            "a6b28555badcb18d6be924c8fc1bea49971656b8.tar.gz": [
            "https://github.com/envoyproxy/envoy-build-tools/archive/a6b28555badcb18d6be924c8fc1bea49971656b8.tar.gz",
            ], 
            "b4d7f8e1aa1ebbe75aaf959d93caf5826599c9ab.tar.gz": [
            "https://github.com/edenhill/librdkafka/archive/b4d7f8e1aa1ebbe75aaf959d93caf5826599c9ab.tar.gz",
            ], 
            "bazel-gazelle-v0.19.0.tar.gz": [
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.19.0/bazel-gazelle-v0.19.0.tar.gz",
            ], 
            "bazel-skylib.0.8.0.tar.gz": [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/0.8.0/bazel-skylib.0.8.0.tar.gz",
            ], 
            "bazel-toolchains-1.1.0.tar.gz": [
            "https://github.com/bazelbuild/bazel-toolchains/releases/download/1.1.0/bazel-toolchains-1.1.0.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-toolchains/archive/1.1.0.tar.gz",
            ], 
            "be480e391cc88a75cf2a81960ef79c80d5012068.tar.gz": [
            "https://github.com/googleapis/googleapis/archive/be480e391cc88a75cf2a81960ef79c80d5012068.tar.gz",
            ], 
            "boringssl-66005f41fbc3529ffe8d007708756720529da20d.tar.xz": [
            "https://commondatastorage.googleapis.com/chromium-boringssl-docs/fips/boringssl-66005f41fbc3529ffe8d007708756720529da20d.tar.xz",
            ], 
            "compiler-rt-9.0.0.src.tar.xz": [
            "http://releases.llvm.org/9.0.0/compiler-rt-9.0.0.src.tar.xz",
            ], 
            "curl-7.66.0.tar.gz": [
            "https://github.com/curl/curl/releases/download/curl-7_66_0/curl-7.66.0.tar.gz",
            ], 
            "d1fe8a7d8ae18f3d454f055eba5213c291986f21.tar.gz": [
            "https://github.com/google/libprotobuf-mutator/archive/d1fe8a7d8ae18f3d454f055eba5213c291986f21.tar.gz",
            ], 
            "d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe.tar.gz": [
            "https://envoy.nos-eastchina1.126.net/build/external_deps/d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe.tar.gz",
            "https://github.com/nlohmann/json/archive/d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe.tar.gz",
            ], 
            "d35267580568517f09bdf70cb582e5284c25401a.tar.gz": [
            "https://github.com/sewenew/redis-plus-plus/archive/d35267580568517f09bdf70cb582e5284c25401a.tar.gz",
            ], 
            "d5b4c69b7113213c1da3a0ccbfd1ee1b40443c7a.tar.gz": [
            "https://github.com/redis/hiredis/archive/d5b4c69b7113213c1da3a0ccbfd1ee1b40443c7a.tar.gz",
            ], 
            "d7003576dd133856432e2e07340f45926242cc3a.tar.gz": [
            "https://github.com/google/googletest/archive/d7003576dd133856432e2e07340f45926242cc3a.tar.gz",
            ], 
            "d7e070e7283f822b1d2787903cce3615536c5610.tar.gz": [
            "https://github.com/c-ares/c-ares/archive/d7e070e7283f822b1d2787903cce3615536c5610.tar.gz",
            ], 
            "d85f82972c2def6db9c90f3d9a23f56a0ac3caff.tar.gz": [
            "https://github.com/google/cel-cpp/archive/d85f82972c2def6db9c90f3d9a23f56a0ac3caff.tar.gz",
            ], 
            "fc00474ddc21fff618fc3f009b46590e241e425e.tar.gz": [
            "https://github.com/gperftools/gperftools/archive/fc00474ddc21fff618fc3f009b46590e241e425e.tar.gz",
            ], 
            "fd7de029969b7c0ef8b754660b997399b6fd812a.tar.gz": [
            "https://github.com/envoyproxy/protoc-gen-validate/archive/fd7de029969b7c0ef8b754660b997399b6fd812a.tar.gz",
            ], 
            "fmt-5.3.0.zip": [
            "https://github.com/fmtlib/fmt/releases/download/5.3.0/fmt-5.3.0.zip",
            ], 
            "msgpack-3.2.0.tar.gz": [
            "https://github.com/msgpack/msgpack-c/releases/download/cpp-3.2.0/msgpack-3.2.0.tar.gz",
            ], 
            "nghttp2-1.39.1.tar.gz": [
            "https://github.com/nghttp2/nghttp2/releases/download/v1.39.1/nghttp2-1.39.1.tar.gz",
            ], 
            "protobuf-all-3.9.2.tar.gz": [
            "https://github.com/protocolbuffers/protobuf/releases/download/v3.9.2/protobuf-all-3.9.2.tar.gz",
            ], 
            "rules_go-v0.20.1.tar.gz": [
            "https://github.com/bazelbuild/rules_go/releases/download/v0.20.1/rules_go-v0.20.1.tar.gz",
            ], 
            "six-1.10.0.tar.gz": [
            "https://files.pythonhosted.org/packages/b3/b2/238e2590826bfdd113244a40d9d3eb26918bd798fc187e2360a8367068db/six-1.10.0.tar.gz",
            ], 
            "tclap-1-2-1-release-final.tar.gz": [
            "https://github.com/mirror/tclap/archive/tclap-1-2-1-release-final.tar.gz",
            ], 
            "thrift-0.11.0.tar.gz": [
            "https://files.pythonhosted.org/packages/c6/b4/510617906f8e0c5660e7d96fbc5585113f83ad547a3989b80297ac72a74c/thrift-0.11.0.tar.gz",
            ], 
            "twitter.common.finagle-thrift-0.3.9.tar.gz": [
            "https://files.pythonhosted.org/packages/f9/e7/4f80d582578f8489226370762d2cf6bc9381175d1929eba1754e03f70708/twitter.common.finagle-thrift-0.3.9.tar.gz",
            ], 
            "twitter.common.lang-0.3.9.tar.gz": [
            "https://files.pythonhosted.org/packages/08/bc/d6409a813a9dccd4920a6262eb6e5889e90381453a5f58938ba4cf1d9420/twitter.common.lang-0.3.9.tar.gz",
            ], 
            "twitter.common.rpc-0.3.9.tar.gz": [
            "https://files.pythonhosted.org/packages/be/97/f5f701b703d0f25fbf148992cd58d55b4d08d3db785aad209255ee67e2d0/twitter.common.rpc-0.3.9.tar.gz",
            ], 
            "v0.7.2.tar.gz": [
            "https://github.com/Cyan4973/xxHash/archive/v0.7.2.tar.gz",
            ], 
            "v0.8.0.tar.gz": [
            "https://github.com/lightstep/lightstep-tracer-cpp/archive/v0.8.0.tar.gz",
            ], 
            "v1.1.0.tar.gz": [
            "https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz",
            ], 
            "v1.1.1.tar.gz": [
            "https://github.com/DataDog/dd-opentracing-cpp/archive/v1.1.1.tar.gz",
            ], 
            "v1.22.1.tar.gz": [
            "https://github.com/grpc/grpc/archive/v1.22.1.tar.gz",
            ], 
            "v1.3.1.tar.gz": [
            "https://github.com/gabime/spdlog/archive/v1.3.1.tar.gz",
            ], 
            "v1.5.0.tar.gz": [
            "https://github.com/google/benchmark/archive/v1.5.0.tar.gz",
            ], 
            "v1.5.1.tar.gz": [
            "https://github.com/opentracing/opentracing-cpp/archive/v1.5.1.tar.gz",
            ], 
            "v2.1.0-beta3.tar.gz": [
            "https://github.com/LuaJIT/LuaJIT/archive/v2.1.0-beta3.tar.gz",
            ], 
            "v2.9.0.tar.gz": [
            "https://github.com/nodejs/http-parser/archive/v2.9.0.tar.gz",
            ], 
            "v8.1.0.tar.gz": [
            "https://github.com/apache/skywalking-data-collect-protocol/archive/v8.1.0.tar.gz",
            ], 
            "yaml-cpp-0.6.3.tar.gz": [
            "https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.tar.gz",
            ], 
            
        },
    )
