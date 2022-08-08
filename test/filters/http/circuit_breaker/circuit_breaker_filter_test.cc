#include <cmath>

#include "envoy/stats/scope.h"

#include "source/common/http/utility.h"
#include "source/filters/http/circuit_breaker/filter.h"

#include "test/mocks/server/mocks.h"
#include "test/test_common/registry.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {
namespace {

using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgReferee;
using testing::WithArgs;

class MockCircuitBreakerManager : public CircuitBreakerManager {
public:
  MockCircuitBreakerManager() {}

  MOCK_METHOD(bool, isOpen, (const std::string&, CircuitBreakerFactoryCb), (override));
  MOCK_METHOD(void, updateMetrics, (const std::string&, bool, std::chrono::milliseconds),
              (override));
  MOCK_METHOD(CircuitBreaker*, getCircuitBreaker, (const std::string&), (override));

  void delegateToImpl() {
    ON_CALL(*this, isOpen)
        .WillByDefault([this](const std::string& uuid, CircuitBreakerFactoryCb cb) {
          return impl_.isOpen(uuid, cb);
        });

    ON_CALL(*this, updateMetrics)
        .WillByDefault([this](const std::string& uuid, bool error, std::chrono::milliseconds rt) {
          impl_.updateMetrics(uuid, error, rt);
          return;
        });

    ON_CALL(*this, getCircuitBreaker).WillByDefault([this](const std::string& uuid) {
      return impl_.getCircuitBreaker(uuid);
    });
  }

public:
  CircuitBreakerManagerImpl impl_{};
};

class CircuitBreakerFilterTest : public testing::Test {
public:
  void setUpFilter() {
    config_ = std::make_shared<CircuitBreakerFilterConfig>(
        mock_manager_, factory_context_.timeSource(), factory_context_.runtime());
    CircuitBreakerFactoryInternalCb circuit_breaker_factory_internal_cb =
        [this](const CircuitBreakerRouteSpecificFilterConfig* route_config) -> CircuitBreakerPtr {
      return std::make_unique<CircuitBreakerImpl>(route_config->settings(), route_config->uuid(),
                                                  config_->timeSource());
    };

    filter_ = std::make_unique<CircuitBreakerFilter>(config_, circuit_breaker_factory_internal_cb);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    cb_ = circuit_breaker_factory_internal_cb;
  }

  void setFilterConfigPerRoute(const std::string& yaml) {
    CircuitBreakerRouteSpecificFilterConfigProto proto_config;
    TestUtility::loadFromYaml(yaml, proto_config);
    auto uuid = factory_context_.api().randomGenerator().uuid();
    route_config_ = std::make_shared<CircuitBreakerRouteSpecificFilterConfig>(proto_config, uuid);

    ON_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(CircuitBreakerFilter::name()))
        .WillByDefault(Return(route_config_.get()));
  }

public:
  std::unique_ptr<CircuitBreakerFilter> filter_;
  CircuitBreakerFilterConfigSharedPtr config_;
  CircuitBreakerRouteSpecificFilterConfigSharedPtr route_config_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  Event::SimulatedTimeSystem time_system_;
  NiceMock<MockCircuitBreakerManager> mock_manager_;
  CircuitBreakerFactoryInternalCb cb_;

public:
  const std::string error_50_percent_config = R"EOF(
  min_request_amount: 2
  error_percent_threshold:
    value: 50
  response:
    http_status: 419
  break_duration: 1s
  )EOF";

  const std::string buffer_data_config = R"EOF(
  min_request_amount: 2
  error_percent_threshold:
    value: 50
  response:
    http_status: 419
  break_duration: 1s
  wait_body: true
  )EOF";

  const std::string rt_threshold_config = R"EOF(
  average_response_time: 0.1s
  consecutive_slow_requests: 3

  response:
    http_status: 419
  break_duration: 1s
  )EOF";
};

TEST_F(CircuitBreakerFilterTest, ErrorPercentClose) {
  InSequence s;

  mock_manager_.delegateToImpl();

  setUpFilter();
  setFilterConfigPerRoute(error_50_percent_config);

  EXPECT_CALL(mock_manager_, isOpen(route_config_->uuid(), _));
  EXPECT_CALL(mock_manager_, updateMetrics(route_config_->uuid(), false, _));

  Http::TestRequestHeaderMapImpl request_headers{
      {":authority", "foo.com"}, {":method", "GET"}, {":path", "/goods"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
  time_system_.advanceTimeWait(std::chrono::milliseconds(5));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, true));

  CircuitBreaker* _breaker = mock_manager_.getCircuitBreaker(route_config_->uuid());
  EXPECT_TRUE(_breaker);
  auto breaker = dynamic_cast<CircuitBreakerImpl*>(_breaker);
  Common::Stats::RollingNumber& rollingErrors = breaker->rollingErrors();
  Common::Stats::RollingNumber& rollingRequests = breaker->rollingRequests();

  auto now = time_system_.systemTime();
  EXPECT_EQ(0, rollingErrors.Sum(now));
  EXPECT_EQ(1, rollingRequests.Sum(now));
}

TEST_F(CircuitBreakerFilterTest, ErrorPercentOpen) {
  InSequence s;

  mock_manager_.delegateToImpl();

  setUpFilter();
  setFilterConfigPerRoute(error_50_percent_config);

  auto cb = [this]() -> CircuitBreakerPtr { return cb_(route_config_.get()); };
  // Create the breaker
  mock_manager_.isOpen(route_config_->uuid(), cb);
  CircuitBreaker* _breaker = mock_manager_.getCircuitBreaker(route_config_->uuid());
  EXPECT_TRUE(_breaker);

  // Record one error
  mock_manager_.updateMetrics(route_config_->uuid(), true, std::chrono::milliseconds(1));
  EXPECT_EQ(false, mock_manager_.isOpen(route_config_->uuid(), cb));

  // Record one error, breaker open
  mock_manager_.updateMetrics(route_config_->uuid(), true, std::chrono::milliseconds(1));
  EXPECT_EQ(true, mock_manager_.isOpen(route_config_->uuid(), cb));

  // Request rejected
  Http::TestRequestHeaderMapImpl request_headers{
      {":authority", "foo.com"}, {":method", "GET"}, {":path", "/goods"}};

  EXPECT_CALL(mock_manager_, isOpen(route_config_->uuid(), _));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(static_cast<Http::Code>(419), _, _, _, _));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, true));
  time_system_.advanceTimeWait(std::chrono::milliseconds(1));

  auto breaker = dynamic_cast<CircuitBreakerImpl*>(_breaker);
  Common::Stats::RollingNumber& rollingErrors = breaker->rollingErrors();
  Common::Stats::RollingNumber& rollingRequests = breaker->rollingRequests();

  auto now = time_system_.systemTime();
  EXPECT_EQ(2, rollingErrors.Sum(now));
  EXPECT_EQ(2, rollingRequests.Sum(now));
}

TEST_F(CircuitBreakerFilterTest, ErrorPercentOpenThenClose) {
  InSequence s;

  mock_manager_.delegateToImpl();

  setUpFilter();
  setFilterConfigPerRoute(error_50_percent_config);

  auto cb = [this]() -> CircuitBreakerPtr { return cb_(route_config_.get()); };
  // Create the breaker
  mock_manager_.isOpen(route_config_->uuid(), cb);
  CircuitBreaker* _breaker = mock_manager_.getCircuitBreaker(route_config_->uuid());
  EXPECT_TRUE(_breaker);
  auto breaker = dynamic_cast<CircuitBreakerImpl*>(_breaker);
  Common::Stats::RollingNumber& rollingErrors = breaker->rollingErrors();
  Common::Stats::RollingNumber& rollingRequests = breaker->rollingRequests();

  // Record one error
  mock_manager_.updateMetrics(route_config_->uuid(), true, std::chrono::milliseconds(1));
  EXPECT_EQ(false, mock_manager_.isOpen(route_config_->uuid(), cb));

  // Record one error, breaker open
  mock_manager_.updateMetrics(route_config_->uuid(), true, std::chrono::milliseconds(1));
  EXPECT_EQ(true, mock_manager_.isOpen(route_config_->uuid(), cb));
  EXPECT_EQ(time_system_.systemTime() + std::chrono::milliseconds(1000), breaker->validTime());

  // Sleep break duration
  time_system_.advanceTimeWait(std::chrono::milliseconds(10000));

  // Request should pass
  Http::TestRequestHeaderMapImpl request_headers{
      {":authority", "foo.com"}, {":method", "GET"}, {":path", "/goods"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
  time_system_.advanceTimeWait(std::chrono::milliseconds(5));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, true));

  auto now = time_system_.systemTime();
  EXPECT_EQ(0, rollingErrors.Sum(now));
  EXPECT_EQ(1, rollingRequests.Sum(now));
}

TEST_F(CircuitBreakerFilterTest, RtThresholdOpen) {
  InSequence s;

  mock_manager_.delegateToImpl();

  setUpFilter();
  setFilterConfigPerRoute(rt_threshold_config);

  auto cb = [this]() -> CircuitBreakerPtr { return cb_(route_config_.get()); };

  // Create the breaker
  mock_manager_.isOpen(route_config_->uuid(), cb);
  CircuitBreaker* _breaker = mock_manager_.getCircuitBreaker(route_config_->uuid());
  EXPECT_TRUE(_breaker);
  auto breaker = dynamic_cast<CircuitBreakerImpl*>(_breaker);
  Common::Stats::RollingNumber& rollingErrors = breaker->rollingErrors();
  Common::Stats::RollingNumber& rollingRequests = breaker->rollingRequests();

  // Record 5 normal requests
  for (int i = 0; i < 5; i++) {
    mock_manager_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(1));
    EXPECT_EQ(false, mock_manager_.isOpen(route_config_->uuid(), cb));
  }
  auto now = time_system_.systemTime();
  EXPECT_EQ(0, rollingErrors.Sum(now));
  EXPECT_EQ(5, rollingRequests.Sum(now));

  // Record 2 slow requests
  for (int i = 0; i < 2; i++) {
    mock_manager_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(101));
    EXPECT_EQ(false, mock_manager_.impl_.isOpen(route_config_->uuid(), cb));
  }

  // The third slow request trigger circuit break to open
  mock_manager_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(101));
  EXPECT_EQ(true, mock_manager_.impl_.isOpen(route_config_->uuid(), cb));
  EXPECT_EQ(3, breaker->consecutiveSlowRequestCount());

  // Request rejected
  Http::TestRequestHeaderMapImpl request_headers{
      {":authority", "foo.com"}, {":method", "GET"}, {":path", "/goods"}};

  EXPECT_CALL(mock_manager_, isOpen(route_config_->uuid(), _));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(static_cast<Http::Code>(419), _, _, _, _));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, true));

  // Sleep break duration
  time_system_.advanceTimeWait(std::chrono::milliseconds(2000));

  // Breaker should close
  EXPECT_EQ(false, mock_manager_.impl_.isOpen(route_config_->uuid(), cb));
  mock_manager_.impl_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(1));
  now = time_system_.systemTime();
  EXPECT_EQ(0, dynamic_cast<CircuitBreakerImpl*>(_breaker)->consecutiveSlowRequestCount());
  EXPECT_EQ(0, rollingErrors.Sum(now));
  // Old buckets are still within lookback duration.
  EXPECT_EQ(9, rollingRequests.Sum(now));

  // Sleep for lookback duration.
  time_system_.advanceTimeWait(std::chrono::milliseconds(10000));
  // Trigger breaker to remove old buckets.
  mock_manager_.impl_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(1));
  now = time_system_.systemTime();
  EXPECT_EQ(1, rollingRequests.Sum(now));
}

TEST_F(CircuitBreakerFilterTest, BreakerOpenWaitBody) {
  InSequence s;

  mock_manager_.delegateToImpl();

  setUpFilter();
  setFilterConfigPerRoute(buffer_data_config);

  auto cb = [this]() -> CircuitBreakerPtr { return cb_(route_config_.get()); };
  // Create the breaker
  mock_manager_.isOpen(route_config_->uuid(), cb);
  CircuitBreaker* _breaker = mock_manager_.getCircuitBreaker(route_config_->uuid());
  EXPECT_TRUE(_breaker);
  auto breaker = dynamic_cast<CircuitBreakerImpl*>(_breaker);
  Common::Stats::RollingNumber& rollingErrors = breaker->rollingErrors();
  Common::Stats::RollingNumber& rollingRequests = breaker->rollingRequests();

  // Record one error
  mock_manager_.updateMetrics(route_config_->uuid(), true, std::chrono::milliseconds(1));
  EXPECT_EQ(false, mock_manager_.isOpen(route_config_->uuid(), cb));

  // Record one error, breaker open
  mock_manager_.updateMetrics(route_config_->uuid(), true, std::chrono::milliseconds(1));
  EXPECT_EQ(true, mock_manager_.isOpen(route_config_->uuid(), cb));
  EXPECT_EQ(time_system_.systemTime() + std::chrono::milliseconds(1000), breaker->validTime());

  Http::TestRequestHeaderMapImpl request_headers{
      {":authority", "foo.com"}, {":method", "GET"}, {":path", "/goods"}};
  Buffer::OwnedImpl slice1("aaaaaa");
  Buffer::OwnedImpl slice2("bbbbbb");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(_, _, _, _, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer, filter_->decodeData(slice1, false));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(static_cast<Http::Code>(419), _, _, _, _));
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer, filter_->decodeData(slice2, true));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "419"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, true));

  // Sleep break duration
  time_system_.advanceTimeWait(std::chrono::milliseconds(10000));

  mock_manager_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(1));
  auto now = time_system_.systemTime();
  EXPECT_EQ(false, mock_manager_.impl_.isOpen(route_config_->uuid(), cb));
  EXPECT_EQ(0, rollingErrors.Sum(now));
  EXPECT_EQ(1, rollingRequests.Sum(now));
}

TEST_F(CircuitBreakerFilterTest, BreakerCloseNotWaitBody) {
  InSequence s;

  mock_manager_.delegateToImpl();

  setUpFilter();
  setFilterConfigPerRoute(buffer_data_config);

  auto cb = [this]() -> CircuitBreakerPtr { return cb_(route_config_.get()); };
  // Create the breaker
  mock_manager_.isOpen(route_config_->uuid(), cb);
  CircuitBreaker* _breaker = mock_manager_.getCircuitBreaker(route_config_->uuid());
  EXPECT_TRUE(_breaker);
  auto breaker = dynamic_cast<CircuitBreakerImpl*>(_breaker);
  Common::Stats::RollingNumber& rollingErrors = breaker->rollingErrors();
  Common::Stats::RollingNumber& rollingRequests = breaker->rollingRequests();

  Http::TestRequestHeaderMapImpl request_headers{
      {":authority", "foo.com"}, {":method", "GET"}, {":path", "/goods"}};
  Buffer::OwnedImpl slice1("aaaaaa");
  Buffer::OwnedImpl slice2("bbbbbb");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(_, _, _, _, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(slice1, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(slice2, true));

  // Sleep break duration
  time_system_.advanceTimeWait(std::chrono::milliseconds(10000));

  mock_manager_.updateMetrics(route_config_->uuid(), false, std::chrono::milliseconds(1));
  auto now = time_system_.systemTime();
  EXPECT_EQ(false, mock_manager_.impl_.isOpen(route_config_->uuid(), cb));
  EXPECT_EQ(0, rollingErrors.Sum(now));
  EXPECT_EQ(1, rollingRequests.Sum(now));
}

} // namespace
} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
