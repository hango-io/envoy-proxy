#include "api/proxy/filters/access_log/escape_filter/v2/escape_filter.pb.h"

#include "common/access_log/access_log_impl.h"
#include "common/http/headers.h"
#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Proxy {
namespace AccessLogFilters {
namespace EscapeFilter {

using ProtoConfig = proxy::filters::access_log::escape_filter::v2::ProtoConfig;
using ProtoEscape = proxy::filters::access_log::escape_filter::v2::EscapeConfig;

class EscapeFilterFactory : public AccessLog::ExtensionFilterFactory {
public:
  std::string name() const override { return "com.proxy.accesslog.escape"; }
  ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  virtual AccessLog::FilterPtr
  createFilter(const envoy::config::accesslog::v3::ExtensionFilter& config,
               Runtime::Loader& runtime, Random::RandomGenerator& random) override;
};

class EscapeFilter : public AccessLog::Filter {
public:
  EscapeFilter(const ProtoConfig&);
  bool evaluate(const StreamInfo::StreamInfo& info, const Http::RequestHeaderMap& request_headers,
                const Http::ResponseHeaderMap& response_headers,
                const Http::ResponseTrailerMap& response_trailers) const override;

private:
  std::vector<Http::LowerCaseString> escape_rqx_;
  std::vector<Http::LowerCaseString> escape_rpx_;
};

} // namespace EscapeFilter
} // namespace AccessLogFilters
} // namespace Proxy
} // namespace Envoy
