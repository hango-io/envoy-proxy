#include "source/common/buffer/buffer_impl.h"
#include "source/filters/http/ip_restriction/ip_restriction.h"

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
namespace IpRestriction {

const static std::vector<std::string> IPV4_LIST{
    "10.199.198.33", "10.199.198.34", "10.199.198.35", "10.199.198.36", "10.199.198.37",
};

const static std::vector<std::string> IPV4_CIDR_LIST{
    "10.199.198.33/12",
    "10.77.88.99/8",
};

const static std::vector<std::string> IPV6_LIST{
    "A157:CD01:3579:1526:DBAC:EF21:4356:7879",
    "2132:0568:0123:1223:0DA8:0D45:0000:52D3",
};

const static std::vector<std::string> IPV6_CIDR_LIST{
    "A157:CD01:3579:1526:DBAC:EF21:4356:7879/32",
    "2132:0568:0123:1223:0DA8:0D45:0000:52D3/32",
};

TEST(BlackOrWhiteListConfig, IPV4BlackList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV4_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  for (auto ip : IPV4_LIST) {
    address = std::make_shared<Network::Address::Ipv4Instance>(ip);
    EXPECT_EQ(false, config.isAllowed(address.get()));
  }

  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.39");

  EXPECT_EQ(true, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV4WhiteList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV4_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }
  list.set_type(proxy::filters::http::iprestriction::v2::ListType::WHITE);

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  for (auto ip : IPV4_LIST) {
    address = std::make_shared<Network::Address::Ipv4Instance>(ip);
    EXPECT_EQ(true, config.isAllowed(address.get()));
  }

  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.39");

  EXPECT_EQ(false, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV4CIDRBlackList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV4_CIDR_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.44");
  EXPECT_EQ(false, config.isAllowed(address.get()));
  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.55");
  EXPECT_EQ(false, config.isAllowed(address.get()));
  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.66");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("10.77.88.102");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("10.77.88.202");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("10.77.88.155");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("127.0.0.1");
  EXPECT_EQ(true, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV4CIDRWhiteList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV4_CIDR_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }
  list.set_type(proxy::filters::http::iprestriction::v2::ListType::WHITE);

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.44");
  EXPECT_EQ(true, config.isAllowed(address.get()));
  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.55");
  EXPECT_EQ(true, config.isAllowed(address.get()));
  address = std::make_shared<Network::Address::Ipv4Instance>("10.199.198.66");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("10.77.88.102");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("10.77.88.202");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("10.77.88.155");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address = std::make_shared<Network::Address::Ipv4Instance>("127.0.0.1");
  EXPECT_EQ(false, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV6BlackList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV6_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  for (auto ip : IPV6_LIST) {
    address = std::make_shared<Network::Address::Ipv6Instance>(ip);
    EXPECT_EQ(false, config.isAllowed(address.get()));
  }

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A157:CD01:3579:1526:DBAC:EF21:4356:7880");

  EXPECT_EQ(true, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV6WhiteList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV6_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }
  list.set_type(proxy::filters::http::iprestriction::v2::ListType::WHITE);

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  for (auto ip : IPV6_LIST) {
    address = std::make_shared<Network::Address::Ipv6Instance>(ip);
    EXPECT_EQ(true, config.isAllowed(address.get()));
  }

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A157:CD01:3579:1526:DBAC:EF21:4356:7880");

  EXPECT_EQ(false, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV6CIDRBlackList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV6_CIDR_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A157:CD01:3579:1526:DBAC:EF21:4356:7879");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A157:CD01:3579:1526:DBAC:EF21:4356:7880");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A257:CD01:3579:1526:DBAC:EF21:4356:7880");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A357:CD01:3579:1526:DBAC:EF21:4356:7880");
  EXPECT_EQ(true, config.isAllowed(address.get()));
}

TEST(BlackOrWhiteListConfig, IPV6CIDRWhiteList) {
  proxy::filters::http::iprestriction::v2::BlackOrWhiteList list;
  for (auto ip : IPV6_CIDR_LIST) {
    list.mutable_list()->Add(std::move(ip));
  }
  list.set_type(proxy::filters::http::iprestriction::v2::ListType::WHITE);

  auto config = BlackOrWhiteListConfig(list);

  Network::Address::InstanceConstSharedPtr address;

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A157:CD01:3579:1526:DBAC:EF21:4356:7879");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A157:CD01:3579:1526:DBAC:EF21:4356:7880");
  EXPECT_EQ(true, config.isAllowed(address.get()));

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A257:CD01:3579:1526:DBAC:EF21:4356:7880");
  EXPECT_EQ(false, config.isAllowed(address.get()));

  address =
      std::make_shared<Network::Address::Ipv6Instance>("A357:CD01:3579:1526:DBAC:EF21:4356:7880");
  EXPECT_EQ(false, config.isAllowed(address.get()));
}

class HttpIpRestrictionFilterTest : public testing::Test {
public:
  void initIpRestrictionFilter(std::string ip_source, bool white, std::vector<std::string> list) {

    proxy::filters::http::iprestriction::v2::ListGlobalConfig filter_config;
    filter_config.set_ip_source_header(ip_source);
    filter_config_ = std::make_shared<ListGlobalConfig>(filter_config, context_, fake_prefix_);

    proxy::filters::http::iprestriction::v2::BlackOrWhiteList route_config;
    if (white) {
      route_config.set_type(proxy::filters::http::iprestriction::v2::ListType::WHITE);
    }

    for (auto ip_string : list) {
      route_config.mutable_list()->Add(std::move(ip_string));
    }

    route_config_ = std::make_shared<BlackOrWhiteListConfig>(route_config);

    filter_ = std::make_shared<HttpIpRestrictionFilter>(filter_config_.get());

    filter_->setDecoderFilterCallbacks(decode_filter_callbacks_);
  }

  Http::TestRequestHeaderMapImpl request_headers_{
      {"content-type", "test"}, {":method", "GET"},       {":authority", "www.proxy.com"},
      {":path", "/path"},       {"comefrom", "unittest"}, {"x-real-ip", "8.8.8.8"}};

  Http::TestResponseHeaderMapImpl response_headers_{{"content-type", "test"},
                                                    {"respfrom", "unittest"},
                                                    {":status", "429"},
                                                    {"replacekey", "old"}};

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decode_filter_callbacks_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;

  IpRestrictionFilterSharedPtr filter_{nullptr};
  ListGlobalConfigSharedPtr filter_config_{nullptr};
  BlackOrWhiteListConfigSharedPtr route_config_{nullptr};

  NiceMock<Server::Configuration::MockFactoryContext> context_;

  std::string fake_prefix_ = "fake_ip_restriction";
};

TEST_F(HttpIpRestrictionFilterTest, NoHttpRoute) {
  initIpRestrictionFilter("", false, {});

  ON_CALL(decode_filter_callbacks_, route()).WillByDefault(Return(nullptr));

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
}

TEST_F(HttpIpRestrictionFilterTest, NoFilterConfig) {
  initIpRestrictionFilter("", false, {});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(nullptr));

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
}

TEST_F(HttpIpRestrictionFilterTest, ForbiddenByBlackList) {

  initIpRestrictionFilter("", false, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  EXPECT_CALL(decode_filter_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info_));

  Network::Address::InstanceConstSharedPtr address =
      std::make_shared<Network::Address::Ipv4Instance>("8.8.8.8");

  stream_info_.downstream_connection_info_provider_->setRemoteAddress(address);

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status);
}

TEST_F(HttpIpRestrictionFilterTest, NotForbiddenByBlackList) {

  initIpRestrictionFilter("", false, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  EXPECT_CALL(decode_filter_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info_));

  Network::Address::InstanceConstSharedPtr address =
      std::make_shared<Network::Address::Ipv4Instance>("8.8.8.9");
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(address);

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
}

TEST_F(HttpIpRestrictionFilterTest, ForbiddenByWhiteList) {

  initIpRestrictionFilter("", true, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  EXPECT_CALL(decode_filter_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info_));

  Network::Address::InstanceConstSharedPtr address =
      std::make_shared<Network::Address::Ipv4Instance>("8.8.8.9");
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(address);

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status);
}

TEST_F(HttpIpRestrictionFilterTest, NotForbiddenByWhiteList) {

  initIpRestrictionFilter("", true, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  EXPECT_CALL(decode_filter_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info_));

  Network::Address::InstanceConstSharedPtr address =
      std::make_shared<Network::Address::Ipv4Instance>("8.8.8.8");
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(address);

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
}

TEST_F(HttpIpRestrictionFilterTest, IpSourceHeader) {

  initIpRestrictionFilter("x-real-ip", false, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status);
}

TEST_F(HttpIpRestrictionFilterTest, ForbiddenByIpTypeError) {

  initIpRestrictionFilter("", true, {"8.8.8.256"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  EXPECT_CALL(decode_filter_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info_));

  Network::Address::InstanceConstSharedPtr address =
      std::make_shared<Network::Address::Ipv4Instance>("8.8.8.8");
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(address);

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status);
}

TEST_F(HttpIpRestrictionFilterTest, ForbiddenByIpEntryEmpty) {

  initIpRestrictionFilter("", true, {""});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  EXPECT_CALL(decode_filter_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info_));

  Network::Address::InstanceConstSharedPtr address =
      std::make_shared<Network::Address::Ipv4Instance>("8.8.8.8");
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(address);

  auto status = filter_->decodeHeaders(request_headers_, true);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status);
}

TEST_F(HttpIpRestrictionFilterTest, NoIpSourceHeader) {

  initIpRestrictionFilter("x-real-ip", false, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  Http::TestRequestHeaderMapImpl request_headers{{"content-type", "test"},
                                                 {":method", "GET"},
                                                 {":authority", "www.proxy.com"},
                                                 {":path", "/path"}};
  auto status = filter_->decodeHeaders(request_headers, true);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
}

TEST_F(HttpIpRestrictionFilterTest, IpSourceHeaderFormatError) {

  initIpRestrictionFilter("x-real-ip", false, {"8.8.8.8"});

  ON_CALL(*decode_filter_callbacks_.route_,
          mostSpecificPerFilterConfig(HttpIpRestrictionFilter::name()))
      .WillByDefault(Return(route_config_.get()));

  Http::TestRequestHeaderMapImpl request_headers{{"content-type", "test"},
                                                 {":method", "GET"},
                                                 {":authority", "www.proxy.com"},
                                                 {":path", "/path"},
                                                 {"x-real-ip", "8.8.8.256"}};
  auto status = filter_->decodeHeaders(request_headers, true);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
}

} // namespace IpRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
