#include <regex>

#include "source/filters/http/header_rewrite/header_rewrite.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRewrite {

TEST(ExtractorRewriterConfigExtractorTest, ExtractWithRegex) {
  ProtoExtractor proto_config;
  *proto_config.mutable_header_name() = "test";
  auto fake = ExtractorRewriterConfig::Extractor(proto_config);

  // Some regex error: error regex pattern or mark count is bigger than group number.
  *proto_config.mutable_regex() = "***";
  proto_config.set_group(1);

  EXPECT_THROW(fake = ExtractorRewriterConfig::Extractor(proto_config), EnvoyException);

  *proto_config.mutable_regex() = "(.{3})(.*)";

  proto_config.set_group(3);

  EXPECT_THROW_WITH_MESSAGE(fake = ExtractorRewriterConfig::Extractor(proto_config), EnvoyException,
                            "Group is 3 but pattern only has 2");

  Http::TestRequestHeaderMapImpl headers = {
      {"test", "aaabbbcccdddeeefffggg1234567890///\\\\\\???|||@@@###$$$%%%^^^&&&"}};
  std::string path;
  Http::Utility::QueryParams parameters;

  // '0' group get whole string.
  proto_config.set_group(0);
  fake = ExtractorRewriterConfig::Extractor(proto_config);
  EXPECT_EQ(fake.extract(headers, path, parameters),
            "aaabbbcccdddeeefffggg1234567890///\\\\\\???|||@@@###$$$%%%^^^&&&");

  // Get group '1'.
  *proto_config.mutable_regex() = "aaabbb(.*)";
  proto_config.set_group(1);
  fake = ExtractorRewriterConfig::Extractor(proto_config);
  EXPECT_EQ(fake.extract(headers, path, parameters),
            "cccdddeeefffggg1234567890///\\\\\\???|||@@@###$$$%%%^^^&&&");
  // Get group '2'
  *proto_config.mutable_regex() = "aaabbb([a-zA-Z]*)(\\d+)(.*)";
  proto_config.set_group(2);
  fake = ExtractorRewriterConfig::Extractor(proto_config);
  EXPECT_EQ(fake.extract(headers, path, parameters), "1234567890");

  // Get group '4' which is not exist.
  *proto_config.mutable_regex() = "aaabbb(.*)(\\d+)(.*)(99999999999)";
  proto_config.set_group(4);
  fake = ExtractorRewriterConfig::Extractor(proto_config);
  EXPECT_EQ(fake.extract(headers, path, parameters), "");
}

TEST(ExtractorRewriterConfigExtractorTest, ExtractHeader) {
  ProtoExtractor proto_config;
  *proto_config.mutable_header_name() = "test";
  auto fake = ExtractorRewriterConfig::Extractor(proto_config);

  std::string path;
  Http::Utility::QueryParams parameters;
  Http::TestRequestHeaderMapImpl headers;

  // Header not exist.
  EXPECT_EQ(fake.extract(headers, path, parameters), "");

  headers = {{"test", "1234567890abcdefg"}};

  EXPECT_EQ(fake.extract(headers, path, parameters), "1234567890abcdefg");
}

TEST(ExtractorRewriterConfigExtractorTest, ExtractPath) {
  ProtoExtractor proto_config;
  proto_config.mutable_path();
  auto fake = ExtractorRewriterConfig::Extractor(proto_config);

  std::string path;
  Http::Utility::QueryParams parameters;
  Http::TestRequestHeaderMapImpl headers;

  // Empty path.
  EXPECT_EQ(fake.extract(headers, path, parameters), "");

  path = "xxxxxxxxxxxx";
  EXPECT_EQ(fake.extract(headers, path, parameters), "xxxxxxxxxxxx");
}

TEST(ExtractorRewriterConfigExtractorTest, ExtractParameter) {
  ProtoExtractor proto_config;
  *proto_config.mutable_parameter() = "fake_query";
  auto fake = ExtractorRewriterConfig::Extractor(proto_config);

  std::string path;
  Http::Utility::QueryParams parameters;
  Http::TestRequestHeaderMapImpl headers;

  // Query not exist.
  EXPECT_EQ(fake.extract(headers, path, parameters), "");

  parameters["fake_query"] = "xxxxxxxxxxxx";

  EXPECT_EQ(fake.extract(headers, path, parameters), "xxxxxxxxxxxx");
}

TEST(ExtractorRewriterConfigExtractorTest, ExtractEnvironment) {
  ProtoExtractor proto_config;
  *proto_config.mutable_environment() = "PATH";
  auto fake = ExtractorRewriterConfig::Extractor(proto_config);

  std::string path;
  Http::Utility::QueryParams parameters;
  Http::TestRequestHeaderMapImpl headers;

  std::regex test_regex("(.*)(/usr/local/bin)(.*)");

  EXPECT_TRUE(std::regex_match(fake.extract(headers, path, parameters), test_regex));
}

TEST(ExtractorRewriterConfigRewriterTest, RewriterSimpleJinjaTemplate) {
  ProtoRewriter proto_config;
  *proto_config.mutable_header_name() = "fake_header";

  ContextDict context = {{"aaa", "aaa"}, {"bbb", "bbb"}, {"ccc", "ccc"}};

  std::string path;
  Http::Utility::QueryParams parameters;
  Http::TestRequestHeaderMapImpl headers;

  auto config = ExtractorRewriterConfig();

  *proto_config.mutable_append() = "{{ aaa }}-{{ bbb }}-{{ ccc }}";

  auto rewriter = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter.rewrite(headers, path, parameters, context);

  EXPECT_EQ("aaa-bbb-ccc", headers.get_("fake_header"));
  headers.remove("fake_header");

  // Template with error.
  *proto_config.mutable_append() = "{{ aaa }}-{{ ddd }}";
  auto rewriter_with_error = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter_with_error.rewrite(headers, path, parameters, context);

  EXPECT_EQ("", headers.get_("fake_header"));
}

TEST(ExtractorRewriterConfigRewriterTest, RewriterMatcherTest) {
  ProtoRewriter proto_config;
  *proto_config.mutable_header_name() = "fake_header";

  ContextDict context = {{"aaa", "aaa"}, {"bbb", "bbb"}, {"ccc", "ccc"}};

  std::string path;
  Http::Utility::QueryParams parameters;
  Http::TestRequestHeaderMapImpl headers;

  auto config = ExtractorRewriterConfig();

  *proto_config.mutable_append() = "{{ aaa }}xxx{{ ccc }}";

  // Append new header with no matcher.
  auto rewriter1 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter1.rewrite(headers, path, parameters, context);

  EXPECT_EQ("aaaxxxccc", headers.get_("fake_header"));

  // Append new header with matcher.
  auto* header_matchers = proto_config.mutable_matcher()->mutable_headers()->Add();
  *header_matchers->mutable_name() = "test_match";
  *header_matchers->mutable_exact_match() = "test_match_value";

  headers.remove("fake_header");
  EXPECT_EQ("", headers.get_("fake_header"));

  auto rewriter2 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter2.rewrite(headers, path, parameters, context);

  // No new header added.
  EXPECT_EQ("", headers.get_("fake_header"));

  headers.addCopy("test_match", "test_match_value");
  rewriter2.rewrite(headers, path, parameters, context);

  EXPECT_EQ("aaaxxxccc", headers.get_("fake_header"));
}

TEST(ExtractorRewriterConfigRewriterTest, TargetHeaderTest) {
  ProtoRewriter proto_config;
  *proto_config.mutable_header_name() = "fake_header";

  ContextDict context = {{"aaa", "aaa"}, {"bbb", "bbb"}, {"ccc", "ccc"}};

  Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header"}};
  std::string path;
  Http::Utility::QueryParams parameters;

  EXPECT_EQ("fake_header", headers.get_("fake_header"));

  // Remove header.
  proto_config.mutable_remove();

  auto config = ExtractorRewriterConfig();

  auto rewriter1 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter1.rewrite(headers, path, parameters, context);

  EXPECT_EQ("", headers.get_("fake_header"));

  // Append new header.
  *proto_config.mutable_append() = "{{ aaa }}xxx{{ ccc }}";
  auto rewriter2 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter2.rewrite(headers, path, parameters, context);

  EXPECT_EQ("aaaxxxccc", headers.get_("fake_header"));

  // Append existed header.
  rewriter2.rewrite(headers, path, parameters, context);
  size_t number = 0;
  headers.iterate([&number](const Http::HeaderEntry& entry) {
    if (entry.key().getStringView() == "fake_header") {
      number += 1;
    }
    return Http::HeaderMap::Iterate::Continue;
  });
  EXPECT_EQ(2, number);

  // Update existed header.
  *proto_config.mutable_update() = "xxx{{ bbb }}xxx";
  auto rewriter3 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter3.rewrite(headers, path, parameters, context);
  EXPECT_EQ("xxxbbbxxx", headers.get_("fake_header"));

  headers.remove("fake_header");
  EXPECT_EQ("", headers.get_("fake_header"));

  rewriter3.rewrite(headers, path, parameters, context);
  EXPECT_EQ("xxxbbbxxx", headers.get_("fake_header"));
}

TEST(ExtractorRewriterConfigRewriterTest, TargetPathTest) {
  ProtoRewriter proto_config;
  proto_config.mutable_path();

  ContextDict context = {{"aaa", "aaa"}, {"bbb", "bbb"}, {"ccc", "ccc"}};

  Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header"}};
  std::string path = "/testpath";
  Http::Utility::QueryParams parameters;

  auto config = ExtractorRewriterConfig();

  // Cannot remove path.
  proto_config.mutable_remove();
  auto rewriter1 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter1.rewrite(headers, path, parameters, context);

  EXPECT_EQ("/testpath", "/testpath");

  // Update path with empty string.
  *proto_config.mutable_update() = "{{ ddd }}";
  auto rewriter2 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter2.rewrite(headers, path, parameters, context);
  EXPECT_EQ("/testpath", path);

  // Update path.
  *proto_config.mutable_update() = "/{{ aaa }}/{{ bbb }}";
  auto rewriter3 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter3.rewrite(headers, path, parameters, context);
  EXPECT_EQ("/aaa/bbb", path);

  // Append path.
  *proto_config.mutable_append() = "/{{ aaa }}/{{ bbb }}";
  auto rewriter4 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter4.rewrite(headers, path, parameters, context);
  EXPECT_EQ("/aaa/bbb/aaa/bbb", path);

  EXPECT_TRUE(config.wouldUpdatePath());
}

TEST(ExtractorRewriterConfigRewriterTest, TargetParameterTest) {
  ProtoRewriter proto_config;
  *proto_config.mutable_parameter() = "fake_query1";

  ContextDict context = {{"aaa", "aaa"}, {"bbb", "bbb"}, {"ccc", "ccc"}};

  Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header"}};
  std::string path;
  Http::Utility::QueryParams parameters = {{"fake_query1", "fake_query1"},
                                           {"fake_query2", "fake_query2"},
                                           {"fake_query3", "fake_query3"}};

  auto config = ExtractorRewriterConfig();

  EXPECT_EQ(parameters["fake_query1"], "fake_query1");

  // Remove query.
  proto_config.mutable_remove();
  auto rewriter1 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter1.rewrite(headers, path, parameters, context);

  EXPECT_EQ(parameters["fake_query1"], "");

  // Try remove a query not exist.
  rewriter1.rewrite(headers, path, parameters, context);

  *proto_config.mutable_append() = "{{ aaa }}{{ bbb }}";
  auto rewriter2 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter2.rewrite(headers, path, parameters, context);

  EXPECT_EQ(parameters["fake_query1"], "aaabbb");

  rewriter2.rewrite(headers, path, parameters, context);

  EXPECT_EQ(parameters["fake_query1"], "aaabbbaaabbb");

  *proto_config.mutable_update() = "{{ aaa }}{{ ccc }}";
  auto rewriter3 = ExtractorRewriterConfig::Rewriter(proto_config, config);
  rewriter3.rewrite(headers, path, parameters, context);

  EXPECT_EQ(parameters["fake_query1"], "aaaccc");
}

TEST(ExtractorRewriterConfigTest, ExtractorRewriterConfigTestSimpleExtractors) {
  ProtoExtractorRewriter extractor_rewriter;
  constexpr absl::string_view config_string = R"EOF(
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
  )EOF";

  TestUtility::loadFromYaml(std::string(config_string), extractor_rewriter);
  auto config = ExtractorRewriterConfig(extractor_rewriter);

  Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header_value"}};
  std::string path = "/fake_path";
  Http::Utility::QueryParams parameters = {{"fake_parameter", "fake_parameter_value"}};

  ContextDict context;
  config.extractContext(headers, path, parameters, context);

  EXPECT_EQ(4, context.size());
  EXPECT_EQ("fake_header_value", context["aaa"]);
  EXPECT_EQ("/fake_path", context["bbb"]);
  EXPECT_EQ("fake_parameter_value", context["ccc"]);
  EXPECT_EQ("fake", context["ddd"]);
}

TEST(ExtractorRewriterConfigTest, ExtractorRewriterConfigTestSimpleRewriters) {
  ProtoExtractorRewriter extractor_rewriter;
  constexpr absl::string_view config_string = R"EOF(
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
  )EOF";

  TestUtility::loadFromYaml(std::string(config_string), extractor_rewriter);
  auto config = ExtractorRewriterConfig(extractor_rewriter);

  Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header_value"}};
  std::string path = "/fake_path";
  Http::Utility::QueryParams parameters = {{"fake_parameter", "fake_parameter_value"}};

  ContextDict context;
  config.extractContext(headers, path, parameters, context);
  config.rewriteHeaders(headers, path, parameters, context);

  EXPECT_EQ("new_fake_header_value", headers.get_("new_fake_header"));
  EXPECT_EQ("new_fake_parameter_value", parameters["new_fake_parameter"]);
  EXPECT_EQ("/fake/request/fake_path", path);
  // Token "bbb" can only get in encoding.
  EXPECT_EQ("", headers.get_("can_not_add_header"));
}

TEST(ExtractorRewriterConfigTest, ExtractorRewriterConfigTestPathUpdate) {
  ProtoExtractorRewriter extractor_rewriter;
  {
    constexpr absl::string_view config_string = R"EOF(
      rewriters:
      - header_name: new_fake_header
        update: "new_{{ aaa }}"
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
    )EOF";

    TestUtility::loadFromYaml(std::string(config_string), extractor_rewriter);
    auto config = ExtractorRewriterConfig(extractor_rewriter);
    EXPECT_EQ(false, config.wouldUpdatePath());
  }
  {
    constexpr absl::string_view config_string = R"EOF(
      rewriters:
      - header_name: new_fake_header
        update: "new_{{ aaa }}"
      - path: {}
        update: /{{ ddd }}/request{{ bbb }}
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
    )EOF";

    TestUtility::loadFromYaml(std::string(config_string), extractor_rewriter);
    auto config = ExtractorRewriterConfig(extractor_rewriter);
    EXPECT_EQ(true, config.wouldUpdatePath());
  }
  {
    constexpr absl::string_view config_string = R"EOF(
      rewriters:
      - header_name: new_fake_header
        update: "new_{{ aaa }}"
      - path: {}
        update: /{{ ddd }}/request{{ bbb }}
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
    )EOF";

    TestUtility::loadFromYaml(std::string(config_string), extractor_rewriter);
    auto config = ExtractorRewriterConfig(extractor_rewriter);
    EXPECT_EQ(true, config.wouldUpdatePath());
  }
  {
    constexpr absl::string_view config_string = R"EOF(
      rewriters:
      - header_name: new_fake_header
        update: "new_{{ aaa }}"
      - path: {}
        update: /{{ ddd }}/request{{ bbb }}
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
    )EOF";

    TestUtility::loadFromYaml(std::string(config_string), extractor_rewriter);
    auto config = ExtractorRewriterConfig(extractor_rewriter);
    EXPECT_EQ(true, config.wouldUpdatePath());
  }
}

TEST(HeaderRewriteConfigTest, HeaderRewriteConfigCommonTest) {
  ProtoHeaderRewrite rewriters;
  {
    constexpr absl::string_view error_encoder_config_with_path = R"EOF(
      encoder_rewriters:
        rewriters:
        - path: {}
          append: "/aaa/bbb/ccc"
    )EOF";

    TestUtility::loadFromYaml(std::string(error_encoder_config_with_path), rewriters);

    EXPECT_THROW_WITH_MESSAGE(auto error_config = HeaderRewriteConfig(rewriters), EnvoyException,
                              "Illegal target: only header target can be set for encode rewrite");
  }
  {
    constexpr absl::string_view error_encoder_config_with_parameter = R"EOF(
      encoder_rewriters:
        rewriters:
        - parameter: "fake_parameter"
          append: "aaa"
    )EOF";

    TestUtility::loadFromYaml(std::string(error_encoder_config_with_parameter), rewriters);

    EXPECT_THROW_WITH_MESSAGE(auto error_config = HeaderRewriteConfig(rewriters), EnvoyException,
                              "Illegal target: only header target can be set for encode rewrite");
  }
  {
    constexpr absl::string_view config_string = R"EOF(
      decoder_rewriters:
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

    Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header_value"}};
    Http::TestResponseHeaderMapImpl response_headers = {{"fake_header", "fake_header_value"}};
    std::string path = "/fake_path";
    Http::Utility::QueryParams parameters = {{"fake_parameter", "fake_parameter_value"}};

    TestUtility::loadFromYaml(std::string(config_string), rewriters);

    auto config = HeaderRewriteConfig(rewriters);

    ContextDict context;
    config.decodeRewritersConfig().extractContext(headers, path, parameters, context);

    EXPECT_EQ(4, context.size());
    EXPECT_EQ("fake_header_value", context["aaa"]);
    EXPECT_EQ("/fake_path", context["bbb"]);
    EXPECT_EQ("fake_parameter_value", context["ccc"]);
    EXPECT_EQ("fake", context["ddd"]);

    config.encodeRewritersConfig().extractContext(response_headers, path, parameters, context);
    EXPECT_EQ("fake_header_value", context["eee"]);
  }
  {
    constexpr absl::string_view config_string = R"EOF(
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

    Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header_value"}};
    Http::TestResponseHeaderMapImpl response_headers = {{"fake_header", "fake_header_value"}};
    std::string path = "/fake_path";
    Http::Utility::QueryParams parameters = {{"fake_parameter", "fake_parameter_value"}};

    TestUtility::loadFromYaml(std::string(config_string), rewriters);
    auto config = HeaderRewriteConfig(rewriters);

    ContextDict context;
    config.decodeRewritersConfig().extractContext(headers, path, parameters, context);
    config.decodeRewritersConfig().rewriteHeaders(headers, path, parameters, context);

    EXPECT_EQ("new_fake_header_value", headers.get_("new_fake_header"));
    EXPECT_EQ("new_fake_parameter_value", parameters["new_fake_parameter"]);
    EXPECT_EQ("/fake/request/fake_path", path);
    // Token "eee" can only get in encoding.
    EXPECT_EQ("", headers.get_("can_not_add_header"));
  }
  {
    constexpr absl::string_view config_string = R"EOF(
      encoder_rewriters:
        rewriters:
        - header_name: new_fake_encoder_header
          update: "new_{{ aaa }}"
        - header_name: can_add_header
          update: "no_error_{{ eee }}"
        - header_name: new_fake_encoder_header_with_matcher
          update: "new_{{ ddd }}_encoder_header_with_matcher_value"
          matcher:
            headers:
            - name: fake_header
              exact_match: fake_header_value
        - header_name: new_fake_encoder_header_with_error_matcher
          update: "new_{{ ddd }}_encoder_header_with_matcher_value"
          matcher:
            headers:
            - name: fake_header
              exact_match: fake_header_value_with_error
        extractors:
          eee:
            header_name: "fake_header"
      decoder_rewriters:
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
    )EOF";

    Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header_value"}};
    Http::TestResponseHeaderMapImpl response_headers = {{"fake_header", "fake_header_value"}};
    std::string path = "/fake_path";
    Http::Utility::QueryParams parameters = {{"fake_parameter", "fake_parameter_value"}};

    TestUtility::loadFromYaml(std::string(config_string), rewriters);
    auto config = HeaderRewriteConfig(rewriters);

    ContextDict context;
    config.decodeRewritersConfig().extractContext(headers, path, parameters, context);
    config.encodeRewritersConfig().extractContext(response_headers, path, parameters, context);

    config.encodeRewritersConfig().rewriteHeaders(response_headers, path, parameters, context);

    EXPECT_EQ("new_fake_header_value", response_headers.get_("new_fake_encoder_header"));
    EXPECT_EQ("no_error_fake_header_value", response_headers.get_("can_add_header"));
    EXPECT_EQ("new_fake_encoder_header_with_matcher_value",
              response_headers.get_("new_fake_encoder_header_with_matcher"));
    EXPECT_EQ("", response_headers.get_("new_fake_encoder_header_with_error_matcher"));
  }
}

class HeaderRewriteFilterTest : public testing::Test {
public:
  void setup(const absl::string_view common_config, const absl::string_view cluster_config,
             const absl::string_view route_config) {
    ProtoCommonConfig proto_common_config;
    if (!common_config.empty()) {
      TestUtility::loadFromYaml(std::string(common_config), proto_common_config);
    }
    common_config_ = std::make_shared<CommonConfig>(proto_common_config);

    if (!route_config.empty()) {
      ProtoRouteConfig proto_route_config;
      TestUtility::loadFromYaml(std::string(route_config), proto_route_config);
      route_config_ = std::make_shared<RouteConfig>(proto_route_config);
    }

    filter_ = std::make_unique<Filter>(common_config_.get(), "proxy.filters.http.header_rewrite");
    filter_->setDecoderFilterCallbacks(decoder_filter_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_filter_callbacks_);

    mock_cluster_info_ = std::make_shared<NiceMock<Upstream::MockClusterInfo>>();

    if (!cluster_config.empty()) {
      ProtobufWkt::Struct proto_cluster_config;
      TestUtility::loadFromYaml(std::string(cluster_config), proto_cluster_config);

      mock_cluster_info_->metadata_.mutable_filter_metadata()->insert(
          {"proxy.filters.http.header_rewrite", proto_cluster_config});
    }
  }

protected:
  CommonConfigSharedPtr common_config_;
  RouteConfigSharedPtr route_config_;

  FilterPtr filter_;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_filter_callbacks_;

  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_filter_callbacks_;

  std::shared_ptr<NiceMock<Upstream::MockClusterInfo>> mock_cluster_info_;
};

TEST_F(HeaderRewriteFilterTest, RewriteHostAndPath) {
  constexpr absl::string_view rewriter_config = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - path: {}
          update: "/{{ suffix }}"
        - header_name: host
          update: "{{ host }}"
        extractors:
          suffix:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 2
          host:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 1
  )EOF";

  Http::TestRequestHeaderMapImpl headers1 = {
      {":path", "/proxy/baidu.com/anything?aaa=aaa&bbb=bbb&ccc=ccc"}, {"host", "test.com"}};

  setup(rewriter_config, "", "");

  filter_->decodeHeaders(headers1, true);

  EXPECT_EQ("/anything?aaa=aaa&bbb=bbb&ccc=ccc", headers1.get_(":path"));
  EXPECT_EQ("baidu.com", headers1.get_(":authority"));
  EXPECT_TRUE(common_config_->config_.decodeRewritersConfig().wouldUpdatePath());
}

TEST_F(HeaderRewriteFilterTest, RewriterWithClearRouteCache) {
  constexpr absl::string_view rewriter_config = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - path: {}
          update: "/{{ suffix }}"
        - header_name: host
          update: "{{ host }}"
        extractors:
          suffix:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 2
          host:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 1
      clear_route_cache: true
  )EOF";

  Http::TestRequestHeaderMapImpl headers1 = {
      {":path", "/proxy/baidu.com/anything?aaa=aaa&bbb=bbb&ccc=ccc"}, {"host", "test.com"}};

  setup(rewriter_config, "", "");

  EXPECT_CALL(decoder_filter_callbacks_, clearRouteCache());

  filter_->decodeHeaders(headers1, true);

  EXPECT_EQ("/anything?aaa=aaa&bbb=bbb&ccc=ccc", headers1.get_(":path"));
  EXPECT_EQ("baidu.com", headers1.get_(":authority"));
  EXPECT_TRUE(common_config_->config_.decodeRewritersConfig().wouldUpdatePath());
}

TEST_F(HeaderRewriteFilterTest, RewriterWithRouteConfig) {
  constexpr absl::string_view rewriter_config = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - path: {}
          update: "/{{ suffix }}"
        - header_name: host
          update: "{{ host }}"
        extractors:
          suffix:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 2
          host:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 1
      clear_route_cache: true
  )EOF";

  constexpr absl::string_view route_config = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - parameter: bbb
          append: "xxxxxx"
        - parameter: ccc
          update: "yyyyyy"
        - parameter: ddd
          update: "zzzzzz"
        extractors:
          new_path:
            path: {}
      encoder_rewriters:
        rewriters:
        - header_name: fake_encoder_header
          update: "{{ new_path }}"
      clear_route_cache: false
  )EOF";

  Http::TestRequestHeaderMapImpl headers1 = {
      {":path", "/proxy/baidu.com/anything?aaa=aaa&bbb=bbb&ccc=ccc"}, {"host", "test.com"}};

  Http::TestResponseHeaderMapImpl headers2;

  setup(rewriter_config, "", route_config);

  EXPECT_CALL(decoder_filter_callbacks_, clearRouteCache());
  ON_CALL(*decoder_filter_callbacks_.route_,
          mostSpecificPerFilterConfig("proxy.filters.http.header_rewrite"))
      .WillByDefault(Return(route_config_.get()));

  filter_->decodeHeaders(headers1, true);

  EXPECT_EQ("/anything?aaa=aaa&bbb=bbbxxxxxx&ccc=yyyyyy&ddd=zzzzzz", headers1.get_(":path"));
  EXPECT_EQ("baidu.com", headers1.get_(":authority"));
  EXPECT_TRUE(common_config_->config_.decodeRewritersConfig().wouldUpdatePath());

  filter_->encodeHeaders(headers2, true);

  EXPECT_EQ("/anything", headers2.get_("fake_encoder_header"));
}

TEST_F(HeaderRewriteFilterTest, RewriterWithRouteConfigDisabled) {
  constexpr absl::string_view rewriter_config = R"EOF(
    config:
      decoder_rewriters:
        rewriters:
        - path: {}
          update: "/{{ suffix }}"
        - header_name: host
          update: "{{ host }}"
        extractors:
          suffix:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 2
          host:
            path: {}
            regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
            group: 1
      clear_route_cache: true
  )EOF";

  constexpr absl::string_view route_config = R"EOF(
    disabled: true
  )EOF";

  Http::TestRequestHeaderMapImpl headers1 = {
      {":path", "/proxy/baidu.com/anything?aaa=aaa&bbb=bbb&ccc=ccc"}, {"host", "test.com"}};

  setup(rewriter_config, "", route_config);
  ON_CALL(*decoder_filter_callbacks_.route_,
          mostSpecificPerFilterConfig("proxy.filters.http.header_rewrite"))
      .WillByDefault(Return(route_config_.get()));

  filter_->decodeHeaders(headers1, true);

  EXPECT_EQ("test.com", headers1.get_(":authority"));
}

// TEST_F(HeaderRewriteFilterTest, RewriterWithClusterConfig) {
//   constexpr absl::string_view rewriter_config = R"EOF(
//     config:
//       decoder_rewriters:
//         rewriters:
//         - path: {}
//           update: "/{{ suffix }}"
//         - header_name: host
//           update: "{{ host }}"
//         extractors:
//           suffix:
//             path: {}
//             regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
//             group: 2
//           host:
//             path: {}
//             regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
//             group: 1
//       clear_route_cache: true
//   )EOF";

//   constexpr absl::string_view cluster_config = R"EOF(
//     config:
//       decoder_rewriters:
//         rewriters:
//         - parameter: bbb
//           append: "xxxxxx"
//         - parameter: ccc
//           update: "yyyyyy"
//         - parameter: ddd
//           update: "zzzzzz"
//         extractors:
//           new_path:
//             path: {}
//       encoder_rewriters:
//         rewriters:
//         - header_name: fake_encoder_header
//           update: "{{ new_path }}"
//       clear_route_cache: false
//   )EOF";

//   Http::TestRequestHeaderMapImpl headers1 = {
//       {":path", "/proxy/baidu.com/anything?aaa=aaa&bbb=bbb&ccc=ccc"}, {"host", "test.com"}};

//   Http::TestResponseHeaderMapImpl headers2;

//   setup(rewriter_config, cluster_config, "");

//   EXPECT_CALL(decoder_filter_callbacks_, clearRouteCache());
//   ON_CALL(decoder_filter_callbacks_.stream_info_, upstreamClusterInfo())
//       .WillByDefault(
//           Return(absl::optional<Upstream::ClusterInfoConstSharedPtr>(mock_cluster_info_)));

//   filter_->decodeHeaders(headers1, true);

//   EXPECT_EQ("/anything?aaa=aaa&bbb=bbbxxxxxx&ccc=yyyyyy&ddd=zzzzzz", headers1.get_(":path"));
//   EXPECT_EQ("baidu.com", headers1.get_(":authority"));
//   EXPECT_TRUE(common_config_->config_.decodeRewritersConfig().wouldUpdatePath());

//   filter_->encodeHeaders(headers2, true);

//   EXPECT_EQ("/anything", headers2.get_("fake_encoder_header"));
// }

// TEST_F(HeaderRewriteFilterTest, RewriterWithClusterConfigDisabled) {
//   constexpr absl::string_view rewriter_config = R"EOF(
//     config:
//       decoder_rewriters:
//         rewriters:
//         - path: {}
//           update: "/{{ suffix }}"
//         - header_name: host
//           update: "{{ host }}"
//         extractors:
//           suffix:
//             path: {}
//             regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
//             group: 2
//           host:
//             path: {}
//             regex: "/proxy/([a-zA-Z\\.]+)/(.*)"
//             group: 1
//       clear_route_cache: true
//   )EOF";

//   constexpr absl::string_view cluster_config = R"EOF(
//     disabled: true
//   )EOF";

//   Http::TestRequestHeaderMapImpl headers1 = {
//       {":path", "/proxy/baidu.com/anything?aaa=aaa&bbb=bbb&ccc=ccc"}, {"host", "test.com"}};

//   setup(rewriter_config, cluster_config, "");
//   ON_CALL(decoder_filter_callbacks_.stream_info_, upstreamClusterInfo())
//       .WillByDefault(
//           Return(absl::optional<Upstream::ClusterInfoConstSharedPtr>(mock_cluster_info_)));

//   filter_->decodeHeaders(headers1, true);

//   EXPECT_EQ("test.com", headers1.get_(":authority"));
// }

} // namespace HeaderRewrite
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
