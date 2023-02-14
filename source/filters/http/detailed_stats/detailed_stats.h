
#pragma once

#include <string>

#include "envoy/http/filter.h"
#include "envoy/runtime/runtime.h"
#include "envoy/server/filter_config.h"
#include "envoy/stats/stats.h"

#include "source/common/common/enum_to_int.h"
#include "source/common/common/utility.h"
#include "source/common/config/utility.h"
#include "source/common/http/proxy_matcher.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/stream_info/utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/detailed_stats/v2/detailed_stats.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {

using ProtoCommonConfig = proxy::filters::http::detailed_stats::v2::ProtoCommonConfig;
using ProtoRouterConfig = proxy::filters::http::detailed_stats::v2::ProtoRouterConfig;

using MatchedStatTags =
    std::pair<std::unique_ptr<Proxy::Common::Http::CommonMatcher>, Stats::StatNameTagVector>;

class TagsMetadataStats : public Envoy::Config::TypedMetadata::Object {
public:
  class StatsScopes;
  using StatsScopesSharedPtr = std::shared_ptr<StatsScopes>;
  using StatsScopesWeakPtr = std::weak_ptr<StatsScopes>;

  class StatsScopes {
  private:
    StatsScopes(const proxy::filters::http::detailed_stats::v2::PrefixedStatTags& proto_config,
                const std::string& string_config)
        : string_config_(string_config),
          pool_(Runtime::LoaderSingleton::getExisting()->getRootScope().symbolTable()) {

      auto* stats = Runtime::LoaderSingleton::getExisting();
      ASSERT(stats != nullptr);

      request_total_scope_ =
          stats->getRootScope().createScope(proto_config.stat_prefix() + ".requests_total");
      request_bytes_scope_ =
          stats->getRootScope().createScope(proto_config.stat_prefix() + ".request_bytes");
      response_bytes_scope_ =
          stats->getRootScope().createScope(proto_config.stat_prefix() + ".response_bytes");
      request_time_scope_ = stats->getRootScope().createScope(proto_config.stat_prefix() +
                                                              ".request_duration_milliseconds");

      for (const auto& stat_tag : proto_config.stat_tags()) {
        scope_tags_.push_back({pool_.add(stat_tag.key()), pool_.add(stat_tag.val())});
      }
    }

  public:
    const std::string string_config_;

    Stats::StatNamePool pool_;
    Stats::StatNameTagVector scope_tags_;

    Stats::ScopePtr request_total_scope_;
    Stats::ScopePtr request_bytes_scope_;
    Stats::ScopePtr response_bytes_scope_;
    Stats::ScopePtr request_time_scope_;

    static StatsScopesSharedPtr getStatsScopesFromProto(
        const proxy::filters::http::detailed_stats::v2::PrefixedStatTags& proto_config);
    static void eraseStatsScopes(StatsScopesSharedPtr scopes);

    static absl::Mutex mutex_;
    static absl::flat_hash_map<std::string, StatsScopesWeakPtr>
        cached_stats_scopes_ ABSL_GUARDED_BY(mutex_);
  };

  TagsMetadataStats(const proxy::filters::http::detailed_stats::v2::PrefixedStatTags& proto_config)
      : scopes_(StatsScopes::getStatsScopesFromProto(proto_config)) {}

  ~TagsMetadataStats() override { StatsScopes::eraseStatsScopes(std::move(scopes_)); }

  struct StreamStatsContext {
    Stats::StatNameTagVector request_tags_;

    uint64_t request_bytes_{};
    uint64_t response_bytes_{};
    uint64_t request_time_{};
  };

  void onStreamComplete(const StreamStatsContext& context) const {
    Stats::StatNameTagVector final_tags;
    final_tags.reserve(scopes_->scope_tags_.size() + context.request_tags_.size());
    final_tags.insert(final_tags.end(), scopes_->scope_tags_.cbegin(), scopes_->scope_tags_.cend());
    final_tags.insert(final_tags.end(), context.request_tags_.cbegin(),
                      context.request_tags_.cend());
    scopes_->request_total_scope_->counterFromStatNameWithTags(Stats::StatName(), final_tags).inc();

    scopes_->request_bytes_scope_
        ->histogramFromStatNameWithTags(Stats::StatName(), final_tags,
                                        Stats::Histogram::Unit::Bytes)
        .recordValue(context.request_bytes_);
    scopes_->response_bytes_scope_
        ->histogramFromStatNameWithTags(Stats::StatName(), final_tags,
                                        Stats::Histogram::Unit::Bytes)
        .recordValue(context.response_bytes_);
    scopes_->request_time_scope_
        ->histogramFromStatNameWithTags(Stats::StatName(), final_tags,
                                        Stats::Histogram::Unit::Milliseconds)
        .recordValue(context.request_time_);
  }

  mutable StatsScopesSharedPtr scopes_;
};

class TagsRouteTypedMetadataFactory : public Envoy::Router::HttpRouteTypedMetadataFactory {
public:
  std::string name() const override { return "proxy.metadata_stats.detailed_stats"; }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override;

  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any& data) const override;
};

class TagsHostTypedMetadataFactory : public Envoy::Upstream::HostTypedMetadataFactory {
public:
  std::string name() const override { return "proxy.metadata_stats.detailed_stats"; }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override;

  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any& data) const override;
};

class CommonConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  CommonConfig(const ProtoCommonConfig& config, Server::Configuration::FactoryContext& context)
      : symbol_table_(Runtime::LoaderSingleton::getExisting()->getRootScope().symbolTable()),
        time_source_(context.timeSource()), names_pool_(symbol_table_) {

    response_code_name_ = names_pool_.add("response_code");
    response_flags_name_ = names_pool_.add("response_flags");
    none_flags_or_code_ = names_pool_.add("-");

    request_protocol_name_ = names_pool_.add("request_protocol");
    request_protocol_ = names_pool_.add("http");

    for (const auto& matched_tags : config.matched_stats()) {
      Stats::StatNameTagVector tags;

      for (const auto& tag : matched_tags.stat_tags()) {
        tags.push_back({names_pool_.add(tag.key()), names_pool_.add(tag.val())});
      }

      matched_stats_.emplace_back(
          std::make_unique<Proxy::Common::Http::CommonMatcher>(matched_tags.matcher()),
          std::move(tags));
    }
  }

  Envoy::TimeSource& timeSource() { return time_source_; }

  TagsMetadataStats::StreamStatsContext
  streamStatsContext(const Http::ResponseHeaderMap& response_headers,
                     const StreamInfo::StreamInfo& stream_info) {
    TagsMetadataStats::StreamStatsContext context;
    context.request_tags_.reserve(context.request_tags_.size() + 3);

    context.request_tags_.push_back({request_protocol_name_, request_protocol_});
    context.request_tags_.push_back(
        {response_code_name_,
         responseCodeToStatName(
             Http::Utility::getResponseStatusNoThrow(response_headers).value_or(0))});
    context.request_tags_.push_back(
        {response_flags_name_, responseFlagToStatName(stream_info.responseFlags())});

    context.request_bytes_ = stream_info.bytesReceived();
    context.response_bytes_ = stream_info.bytesSent();

    auto duration = stream_info.requestComplete();
    if (duration.has_value()) {
      context.request_time_ =
          std::chrono::duration_cast<std::chrono::milliseconds>(duration.value()).count();
    }
    return context;
  }

  void completeStreamStatsContext(const Common::Http::HttpCommonMatcherContext& match_context,
                                  TagsMetadataStats::StreamStatsContext& context) const;

private:
  static constexpr uint32_t NumHttpCodes = 500;
  static constexpr uint32_t HttpCodeOffset = 100; // code 100 is at index 0.

  Stats::StatName responseCodeToStatName(uint64_t code) const {
    // Take a lock only if we've never seen this response-code before.
    const uint32_t rc_index = static_cast<uint32_t>(code) - HttpCodeOffset;
    if (rc_index >= NumHttpCodes) {
      return none_flags_or_code_;
    }
    return Stats::StatName(code_stat_names_.get(rc_index, [this, code]() -> const uint8_t* {
      return names_pool_.addReturningStorage(std::to_string(code));
    }));
  }

  Stats::StatName responseFlagToStatName(uint64_t flags) const {
    // Fast-path for the common case.
    if (flags == 0) {
      return none_flags_or_code_;
    }

    Stats::StatNameVec flag_names;
    for (size_t i = 0; i < StreamInfo::ResponseFlagUtils::ALL_RESPONSE_STRING_FLAGS.size(); i++) {
      if (flags & StreamInfo::ResponseFlagUtils::ALL_RESPONSE_STRING_FLAGS[i].second) {
        return Stats::StatName(flag_stat_names_.get(i, [this, i]() -> const uint8_t* {
          return names_pool_.addReturningStorage(
              StreamInfo::ResponseFlagUtils::ALL_RESPONSE_STRING_FLAGS[i].first);
        }));
      }
    }
    return none_flags_or_code_;
  }

  Stats::SymbolTable& symbol_table_;
  Envoy::TimeSource& time_source_;

  mutable Stats::StatNamePool names_pool_;

  Stats::StatName response_code_name_;
  Stats::StatName response_flags_name_;
  Stats::StatName none_flags_or_code_;

  Stats::StatName request_protocol_name_;
  Stats::StatName request_protocol_;

  std::vector<MatchedStatTags> matched_stats_;

  mutable Thread::AtomicPtrArray<const uint8_t, NumHttpCodes,
                                 Thread::AtomicPtrAllocMode::DoNotDelete>
      code_stat_names_;
  mutable Thread::AtomicPtrArray<const uint8_t, 64, Thread::AtomicPtrAllocMode::DoNotDelete>
      flag_stat_names_;
};

class RouterConfig : public Router::RouteSpecificFilterConfig {
public:
  RouterConfig(const ProtoRouterConfig& config)
      : names_pool_(Runtime::LoaderSingleton::getExisting()->getRootScope().symbolTable()) {

    for (const auto& matched_tags : config.matched_stats()) {
      Stats::StatNameTagVector tags;

      for (const auto& tag : matched_tags.stat_tags()) {
        tags.push_back({names_pool_.add(tag.key()), names_pool_.add(tag.val())});
      }

      matched_stats_.emplace_back(
          std::make_unique<Proxy::Common::Http::CommonMatcher>(matched_tags.matcher()),
          std::move(tags));
    }
  }

  void completeStreamStatsContext(const Common::Http::HttpCommonMatcherContext& match_context,
                                  TagsMetadataStats::StreamStatsContext& context) const;

  mutable Stats::StatNamePool names_pool_;
  std::vector<MatchedStatTags> matched_stats_;
};

class Filter : public Envoy::Http::PassThroughEncoderFilter, Logger::Loggable<Logger::Id::filter> {
public:
  Filter(CommonConfig* config) : config_(config){};

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  void onDestroy() override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.detailed_stats");
  }

private:
  const Http::ResponseHeaderMap* response_headers_{};
  CommonConfig* config_{nullptr};
};

} // namespace DetailedStats
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
