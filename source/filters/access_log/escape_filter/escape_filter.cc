#include "source/filters/access_log/escape_filter/escape_filter.h"

#include "envoy/registry/registry.h"

#include "source/common/common/proxy_utility.h"
#include "source/common/config/utility.h"
#include "source/common/protobuf/message_validator_impl.h"

namespace Envoy {
namespace Proxy {
namespace AccessLogFilters {
namespace EscapeFilter {

ProtobufTypes::MessagePtr EscapeFilterFactory::createEmptyConfigProto() {
  return std::make_unique<proxy::filters::access_log::escape_filter::v2::ProtoConfig>();
}

AccessLog::FilterPtr
EscapeFilterFactory::createFilter(const envoy::config::accesslog::v3::ExtensionFilter& config,
                                  Runtime::Loader&, Random::RandomGenerator&) {

  proxy::filters::access_log::escape_filter::v2::ProtoConfig proto_config;

  auto visitor = Envoy::ProtobufMessage::StrictValidationVisitorImpl();
  Config::Utility::translateOpaqueConfig(config.typed_config(), visitor, proto_config);
  return std::make_unique<EscapeFilter>(proto_config);
}

EscapeFilter::EscapeFilter(
    const proxy::filters::access_log::escape_filter::v2::ProtoConfig& config) {
  for (auto& to_escape : config.to_escapes()) {
    switch (to_escape.to_escape_case()) {
    case ProtoEscape::ToEscapeCase::kRqxHeader:
      escape_rqx_.push_back(Http::LowerCaseString(to_escape.rqx_header()));
      break;
    case ProtoEscape::ToEscapeCase::kRpxHeader:
      escape_rpx_.push_back(Http::LowerCaseString(to_escape.rpx_header()));
      break;
    default:;
    }
  }
}

bool EscapeFilter::evaluate(const StreamInfo::StreamInfo&,
                            const Http::RequestHeaderMap& request_headers,
                            const Http::ResponseHeaderMap& response_headers,
                            const Http::ResponseTrailerMap&) const {
  for (auto& header : escape_rqx_) {
    const auto result = request_headers.get(header);
    if (result.empty()) {
      continue;
    }
    const_cast<Http::HeaderEntry*>(result[0])->value(
        Proxy::Common::Common::StringUtil::escapeForJson(result[0]->value().getStringView()));
  }

  for (auto& header : escape_rpx_) {
    const auto result = response_headers.get(header);
    if (result.empty()) {
      continue;
    }
    const_cast<Http::HeaderEntry*>(result[0])->value(
        Proxy::Common::Common::StringUtil::escapeForJson(result[0]->value().getStringView()));
  }

  return true;
}

REGISTER_FACTORY(EscapeFilterFactory, AccessLog::ExtensionFilterFactory);

} // namespace EscapeFilter
} // namespace AccessLogFilters
} // namespace Proxy
} // namespace Envoy
