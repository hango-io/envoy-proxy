
#pragma once

#include <cstdint>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/waf/v2/waf.pb.h"
#include "modsecurity/modsecurity.h"
#include "modsecurity/rules_set.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Waf {

using RuleSet = proxy::filters::http::waf::v2::RuleSet;

class RuleSetConfig : public Router::RouteSpecificFilterConfig,
                      Logger::Loggable<Logger::Id::filter> {
public:
  RuleSetConfig(const RuleSet& ruleset);

  std::shared_ptr<modsecurity::ModSecurity> modsec_;
  std::shared_ptr<modsecurity::RulesSet> modsec_rules_;

  std::string rule_str_;
  std::vector<std::string> rule_files_;
};

using RuleSetConfigSharedPtr = std::shared_ptr<RuleSetConfig>;

/*
 * Filter inherits from PassthroughFilter instead of StreamFilter because PassthroughFilter
 * implements all the StreamFilter's virtual mehotds by default. This reduces a lot of unnecessary
 * code.
 */
class WafFilter : public Http::PassThroughFilter, Logger::Loggable<Logger::Id::filter> {
public:
  WafFilter(const RuleSetConfig* config);

  void onDestroy() override;

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool end_stream) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool end_stream) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.waf");
  }

private:
  const RuleSetConfig* config_{nullptr};
  std::shared_ptr<modsecurity::Transaction> modsec_transaction_;
  std::shared_ptr<modsecurity::Transaction> modsec_transaction_route_;
  bool intervined_;
  bool request_processed_;
  bool response_processed_;
  bool end_stream_;
  bool is_send_local_reply_;
  bool is_request_body_above_;
  bool is_response_body_above_;
  uint64_t status_;

  Http::ResponseHeaderMap* response_headers_{};

  bool intervention();
  bool interventionRoute();

  Http::FilterHeadersStatus getRequestHeadersStatus();
  Http::FilterDataStatus getRequestStatus();

  Http::FilterHeadersStatus getResponseHeadersStatus();
  Http::FilterDataStatus getResponseStatus();

  bool enabledRuleEngine();

  void sendEncodeReply(uint64_t status);
};

using WafFilterSharedPtr = std::shared_ptr<WafFilter>;

} // namespace Waf
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
