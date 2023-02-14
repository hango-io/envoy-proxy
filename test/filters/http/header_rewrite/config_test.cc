#include "source/filters/http/header_rewrite/config.h"

#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRewrite {
namespace {

static constexpr char TestHeaderRewrite[] = "proxy.filters.http.test_header_rewrite";
using TestHeaderRewriteFactory = Factory<TestHeaderRewrite>;

TEST(HeaderRewriteFactoryTest, HeaderRewriteFactoryTest) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  TestHeaderRewriteFactory factory;

  // Create factory function with proto config.
  {
    ProtoCommonConfig config;
    constexpr absl::string_view config_string = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - header_name: new_fake_header
          update: "new_{{ aaa }}"
        - path: {}
          update: /{{ ddd }}/request{{ bbb }}
        - parameter: new_fake_parameter
          update: "new_{{ ccc }}"
        - header_name: can_not_add_header
          update: "with_error_{{ eee }}"
        extractors:
          aaa:
            header_name: "fake_header"
          bbb:
            path: {}
          ccc:
            parameter: "fake_parameter"
          ddd:
            header_name: "fake_header"
            regex: (.{4}).*
            group: 1
      encoder_rewriters:
        extractors:
          eee:
            header_name: "fake_header"
    )EOF";

    TestUtility::loadFromYaml(std::string(config_string), config);

    auto factoryCb = factory.createFilterFactoryFromProto(config, "", context);
    EXPECT_NE(nullptr, factoryCb);

    NiceMock<Http::MockFilterChainFactoryCallbacks> filter_chain_callback;
    EXPECT_CALL(filter_chain_callback, addStreamFilter(_));
    factoryCb(filter_chain_callback);
  }

  // Create route config with proto config.
  {
    ProtoRouteConfig config;
    constexpr absl::string_view config_string = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - header_name: new_fake_header
          update: "new_{{ aaa }}"
        - path: {}
          update: /{{ ddd }}/request{{ bbb }}
        - parameter: new_fake_parameter
          update: "new_{{ ccc }}"
        - header_name: can_not_add_header
          update: "with_error_{{ eee }}"
        extractors:
          aaa:
            header_name: "fake_header"
          bbb:
            path: {}
          ccc:
            parameter: "fake_parameter"
          ddd:
            header_name: "fake_header"
            regex: (.{4}).*
            group: 1
      encoder_rewriters:
        extractors:
          eee:
            header_name: "fake_header"
    )EOF";

    TestUtility::loadFromYaml(std::string(config_string), config);

    EXPECT_NE(nullptr,
              factory.createRouteSpecificFilterConfig(config, context.getServerFactoryContext(),
                                                      context.messageValidationVisitor()));
  }
}

} // namespace
} // namespace HeaderRewrite
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
