#pragma once

#include "envoy/http/async_client.h"
#include "envoy/http/filter.h"

#include "common/buffer/buffer_impl.h"
#include "common/config/datasource.h"
#include "common/http/utility.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/common/lua/wrappers.h"

#include "filters/http/rider/context.h"
#include "filters/http/rider/vm.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

using FilterConfigProto = proxy::filters::http::rider::v3alpha1::FilterConfig;
using RouteConfigProto =
    proxy::filters::http::rider::v3alpha1::RouteFilterConfig;

class Config {
public:
  virtual ~Config() = default;

  virtual Upstream::ClusterManager &clusterManager() PURE;

  virtual TimeSource &timeSource() PURE;

  virtual AccessLog::AccessLogFileSharedPtr logFile() PURE;

  virtual PluginSharedPtr plugin() PURE;

  virtual PluginHandle &pluginHandle() PURE;

  virtual absl::string_view pluginName() const PURE;

  virtual bool allowNoRoute() const PURE;
};

class ConfigImpl : public Config {
public:
  ConfigImpl(const FilterConfigProto &proto_config,
             Upstream::ClusterManager &clusterManager,
             AccessLog::AccessLogManager &log_manager, TimeSource &time_source,
             ThreadLocal::SlotAllocator &tls, Api::Api &api);

  virtual ~ConfigImpl(){};

public:
  Upstream::ClusterManager &clusterManager() override {
    return cluster_manager_;
  }

  TimeSource &timeSource() override { return time_source_; }

  AccessLog::AccessLogFileSharedPtr logFile() override { return log_file_; }

  PluginSharedPtr plugin() override { return plugin_; }

  PluginHandle &pluginHandle() override {
    return plugin_handle_->getTyped<PluginHandle>();
  }

  absl::string_view pluginName() const override { return plugin_->name(); }

  // TODO(Tong Cai): configurable.
  bool allowNoRoute() const override { return allow_no_route_; }

private:
  Upstream::ClusterManager &cluster_manager_;
  TimeSource &time_source_;
  AccessLog::AccessLogFileSharedPtr log_file_;

  std::shared_ptr<Plugin> plugin_;
  ThreadLocal::SlotPtr plugin_handle_;
  bool allow_no_route_;
};

class RoutePluginConfig {
public:
  RoutePluginConfig(
      const proxy::filters::http::rider::v3alpha1::RoutePluginConfig
          &proto_config);
  RoutePluginConfig(absl::string_view name, absl::string_view configuration,
                    size_t hash)
      : name_(name.data()),
        configuration_(configuration.data(), configuration.length()),
        hash_(hash) {}

  absl::string_view name() const { return name_; }
  absl::string_view configuration() const { return configuration_; }
  size_t hash() const { return hash_; }

private:
  const std::string name_;
  const std::string configuration_;
  size_t hash_;
};

class RouteConfig : public Router::RouteSpecificFilterConfig {
public:
  RouteConfig(const proxy::filters::http::rider::v3alpha1::RouteFilterConfig
                  &proto_config);

  const std::list<RoutePluginConfig> &plugins() const { return plugins_; }

private:
  std::list<RoutePluginConfig> plugins_;
};

class FilterCallbacks {
public:
  virtual ~FilterCallbacks() = default;

  virtual const Buffer::Instance *bufferedBody() PURE;

  virtual void addData(Buffer::Instance &data) PURE;

  virtual void onHeadersModified() PURE;

  virtual void continueIteration() PURE;

  /**
   * Perform an immediate response.
   * @param headers supplies the response headers.
   * @param body supplies the optional response body.
   * @param state supplies the active Lua state.
   */
  virtual void respond(Http::ResponseHeaderMapPtr &&headers,
                       Buffer::Instance *body, lua_State *state) PURE;
};

class Filter;

class StreamWrapper : public Logger::Loggable<Logger::Id::filter>,
                      public Http::AsyncClient::Callbacks {
public:
  enum class State {
    // Lua code is currently running or the script has finished.
    Running,
    // Lua script is blocked waiting for the full body.
    WaitForBody,
    // Lua script is blocked waiting for the next body chunk.
    WaitForBodyChunk,
    // Lua script is blocked waiting for trailers.
    WaitForTrailers,
    // Lua script is blocked waiting for trailers.
    HttpCall,
    // Lua script has done a direct response.
    Responded
  };

  StreamWrapper(Coroutine &coroutine, Filter &filter,
                FilterCallbacks &callbacks, bool end_stream);

  Http::FilterHeadersStatus start(int function_ref);

  Http::FilterDataStatus onData(Buffer::Instance &data, bool end_stream);

  // Http::AsyncClient::Callbacks
  void onSuccess(const Http::AsyncClient::Request &,
                 Http::ResponseMessagePtr &&) override;
  void onFailure(const Http::AsyncClient::Request &,
                 Http::AsyncClient::FailureReason) override;
  void onBeforeFinalizeUpstreamSpan(Tracing::Span &,
                                    const Http::ResponseHeaderMap *) override {}

  void onReset() {
    if (http_request_) {
      http_request_->cancel();
      http_request_ = nullptr;
    }
  }

  int luaBody(lua_State *state);
  int luaHttpCall(lua_State *state);
  int luaRespond(lua_State *state);
  void buildHeadersFromTable(Http::HeaderMap &headers,
                                           lua_State *state, int table_index);

private:
  Filter &filter_;
  FilterCallbacks &filter_callbacks_;
  State state_{State::Running};
  Coroutine &coroutine_;
  std::function<void()> yield_callback_;
  Http::AsyncClient::Request *http_request_{};

  bool saw_body_{};
  bool buffered_body_{};
  bool end_stream_{};
  bool headers_continued_{};
};

using StreamWrapperPtr = std::unique_ptr<StreamWrapper>;

class Filter : public ContextBase, public Http::StreamFilter {
public:
  Filter(Config &config)
      : ContextBase(this, config.pluginHandle().vm(), config.plugin()),
        config_(config) {}

  virtual ~Filter() = default;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.rider");
  }

  Upstream::ClusterManager &clusterManager() {
    return config_.clusterManager();
  }

  void scriptError(const LuaException &e);

  // Http::StreamFilterBase
  void onDestroy() override;

  void resetInternalState();

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap &headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance &data,
                                    bool end_stream) override;
  Http::FilterTrailersStatus
  decodeTrailers(Http::RequestTrailerMap &) override {
    return Http::FilterTrailersStatus::Continue;
  }
  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks &callbacks) override {
    decoder_callbacks_.callbacks_ = &callbacks;
  }

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus
  encode100ContinueHeaders(Http::ResponseHeaderMap &) override {
    return Http::FilterHeadersStatus::Continue;
  }
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap &headers,
                                          bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance &data,
                                    bool end_stream) override;
  Http::FilterTrailersStatus
  encodeTrailers(Http::ResponseTrailerMap &) override {
    return Http::FilterTrailersStatus::Continue;
  };
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap &) override {
    return Http::FilterMetadataStatus::Continue;
  }
  void setEncoderFilterCallbacks(
      Http::StreamEncoderFilterCallbacks &callbacks) override {
    encoder_callbacks_.callbacks_ = &callbacks;
  };

public:
  // GeneralInterface
  uint64_t getCurrentTimeMilliseconds() override {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               config_.timeSource().systemTime().time_since_epoch())
        .count();
  }
  void log(spdlog::level::level_enum level, const char *message) override;

  // StreamInterface
  void fileLog(const char *buf, int len) override;
  int getOrCreateSharedTable() override;
  int luaBody(lua_State *state, StreamDirection direction) override;
  int luaHttpCall(lua_State *state, StreamDirection direction) override;
  int luaRespond(lua_State *state, StreamDirection direction) override;
  uint64_t getRouteConfigHash() override;
  int getRouteConfiguration(envoy_lua_ffi_str_t *buffe) override;

  // StreamInfoInterface
  const char *upstreamHost() override;
  const char *upstreamCluster() override;
  const char *downstreamLocalAddress() override;
  const char *downstreamRemoteAddress() override;
  int64_t startTime() override {
    ASSERT(decoder_callbacks_.callbacks_);

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               decoder_callbacks_.callbacks_->streamInfo()
                   .startTime()
                   .time_since_epoch())
        .count();
  }

  // HeaderInterface
  int getHeaderMapSize(LuaStreamOpSourceType type) override;
  int getHeaderMap(LuaStreamOpSourceType type,
                   envoy_lua_ffi_string_pairs *buf) override;
  int getHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                        envoy_lua_ffi_str_t *value) override;
  int setHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                        absl::string_view value) override;
  int removeHeaderMapValue(LuaStreamOpSourceType type,
                           absl::string_view key) override;

  int getQueryParameters(envoy_lua_ffi_string_pairs *buf) override;

  // FilterMetadataInterface
  int getMetadataValue(envoy_lua_ffi_str_t *filter_name,
                       envoy_lua_ffi_str_t *key,
                       envoy_lua_ffi_str_t *value) override;

private:
  Http::FilterHeadersStatus
  doHeaders(StreamWrapperPtr &stream_wrapper, StreamDirection direction,
            LuaVirtualMachine &vm, CoroutinePtr &coroutine,
            FilterCallbacks &callbacks, int function_ref, Http::HeaderMap &,
            bool end_stream);

  Http::FilterDataStatus doData(StreamWrapper &stream_wrapper,
                                Buffer::Instance &data, bool end_stream);

  struct DecoderCallbacks : public FilterCallbacks {
    DecoderCallbacks(Filter &parent) : parent_(parent) {}

    // FilterCallbacks
    void addData(Buffer::Instance &data) override {
      return callbacks_->addDecodedData(data, false);
    }
    const Buffer::Instance *bufferedBody() override {
      return callbacks_->decodingBuffer();
    }
    void continueIteration() override { return callbacks_->continueDecoding(); }
    void onHeadersModified() override { callbacks_->clearRouteCache(); }
    void respond(Http::ResponseHeaderMapPtr &&headers, Buffer::Instance *body,
                 lua_State *state) override;

    Filter &parent_;
    Http::StreamDecoderFilterCallbacks *callbacks_{};
  };

  struct EncoderCallbacks : public FilterCallbacks {
    EncoderCallbacks(Filter &parent) : parent_(parent) {}

    // FilterCallbacks
    void addData(Buffer::Instance &data) override {
      return callbacks_->addEncodedData(data, false);
    }
    const Buffer::Instance *bufferedBody() override {
      return callbacks_->encodingBuffer();
    }
    void continueIteration() override { return callbacks_->continueEncoding(); }
    void onHeadersModified() override {}
    void respond(Http::ResponseHeaderMapPtr &&headers, Buffer::Instance *body,
                 lua_State *state) override;

    Filter &parent_;
    Http::StreamEncoderFilterCallbacks *callbacks_{};
  };

  Config &config_;
  absl::optional<RoutePluginConfig> route_config_;
  Http::RequestHeaderMap *request_headers_{};
  Http::ResponseHeaderMap *response_headers_{};
  Buffer::OwnedImpl request_body_{};
  Buffer::OwnedImpl response_body_{};
  DecoderCallbacks decoder_callbacks_{*this};
  EncoderCallbacks encoder_callbacks_{*this};
  StreamWrapperPtr request_stream_wrapper_;
  StreamWrapperPtr response_stream_wrapper_;
  CoroutinePtr request_coroutine_;
  CoroutinePtr response_coroutine_;
  std::string error_message_;
  std::string temporary_string_;
  Envoy::Http::Utility::QueryParams temporary_query_params_;
  int shared_table_reference_{};
  bool destroyed_{};
  bool error_{};
};

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
