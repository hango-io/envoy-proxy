#include "source/filters/http/detailed_stats/config.h"

#include "test/mocks/server/factory_context.h"
#include "test/test_common/test_runtime.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {
namespace {

class MockCustomStatNamespaces : public Stats::CustomStatNamespaces {
public:
  MOCK_METHOD(bool, registered, (const absl::string_view name), (const));
  MOCK_METHOD(void, registerStatNamespace, (const absl::string_view name));
  MOCK_METHOD(absl::optional<absl::string_view>, stripRegisteredPrefix, (const absl::string_view),
              (const));
};

TEST(Factory, Test) {
  TestScopedRuntime scoped_runtime;
  MockCustomStatNamespaces mock_custom_stat_namespaces;
  NiceMock<Server::Configuration::MockFactoryContext> context;

  EXPECT_CALL(context.api_, customStatNamespaces())
      .WillOnce(testing::ReturnRef(mock_custom_stat_namespaces));
  EXPECT_CALL(mock_custom_stat_namespaces, registerStatNamespace("metadata_stats"));

  const std::string yaml_string = R"EOF(
  custom_stat_namespaces:
  - metadata_stats
  )EOF";

  proxy::filters::http::detailed_stats::v2::ProtoCommonConfig common_proto_config;
  TestUtility::loadFromYaml(yaml_string, common_proto_config);
  Factory factory;
  auto cb = factory.createFilterFactoryFromProto(common_proto_config, "stats", context);
  NiceMock<Http::MockFilterChainFactoryCallbacks> callbacks;
  EXPECT_CALL(callbacks, addStreamEncoderFilter(_));
  cb(callbacks);

  proxy::filters::http::detailed_stats::v2::ProtoRouterConfig router_proto_config;

  EXPECT_NE(nullptr, factory.createRouteSpecificFilterConfig(
                         router_proto_config, context.server_factory_context_,
                         context.getServerFactoryContext().messageValidationVisitor()));

  EXPECT_NO_THROW(factory.createEmptyConfigProto());
  EXPECT_EQ(factory.name(), "proxy.filters.http.detailed_stats");
}

} // namespace
} // namespace DetailedStats
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
