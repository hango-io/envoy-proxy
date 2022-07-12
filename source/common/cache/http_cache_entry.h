#pragma once

#include "source/common/cache/cache_base.h"
#include "source/common/http/message_impl.h"
#include "source/common/http/proxy_base.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

static inline Envoy::Http::HeaderMap::Iterate
headersToJson(const Envoy::Http::HeaderEntry& e, rapidjson::Writer<rapidjson::StringBuffer>& w) {
  const auto& key = e.key().getStringView();
  const auto& value = e.value().getStringView();
  w.Key(key.data(), key.size());
  w.String(value.data(), value.size());
  return Envoy::Http::HeaderMap::Iterate::Continue;
}

template <class M, class M_IMPL, class H_IMPL> class HttpCacheEntryBase : public CacheEntry {
public:
  HttpCacheEntryBase(std::unique_ptr<M>&& message, uint64_t cache_expire)
      : cache_message_(std::move(message)), cache_expire_(cache_expire) {

    static_assert(std::is_same_v<M_IMPL, Envoy::Http::RequestMessageImpl> ||
                  std::is_same_v<M_IMPL, Envoy::Http::ResponseMessageImpl>);
    static_assert(std::is_same_v<H_IMPL, Envoy::Http::RequestHeaderMapImpl> ||
                  std::is_same_v<H_IMPL, Envoy::Http::ResponseHeaderMapImpl>);

    if (cache_message_) {
      cache_length_ += cache_message_->headers().byteSize();
      cache_length_ += cache_message_->body().length();
      // TODO(wbpcode): support trailter.
    }
  }

  // Default contructor. We can create empty cache entry and fill it by loadFromString.
  HttpCacheEntryBase() = default;

  // CacheEntry
  void loadFromString(absl::string_view raw_string) override {
    if (raw_string.empty()) {
      cache_message_ = nullptr;
      return;
    }
    rapidjson::Document doc;
    doc.Parse(raw_string.data(), raw_string.size());
    if (doc.HasParseError()) {
      // Parse error.
      cache_message_ = nullptr;
      return;
    }
    static const std::string headers = "headers", rawbody = "rawbody";

    auto header_ptr = H_IMPL::create();
    if (doc.HasMember(headers.c_str())) {
      const rapidjson::Value& value = doc[headers.c_str()];
      for (auto it = value.MemberBegin(); it != value.MemberEnd(); it++) {
        Envoy::Http::LowerCaseString header_name(it->name.GetString());
        std::string header_value(it->value.GetString());
        header_ptr->addCopy(header_name, header_value);
      }
    }

    if constexpr (std::is_same_v<H_IMPL, Envoy::Http::ResponseHeaderMapImpl>) {
      if (header_ptr->empty() || !header_ptr->Status()) {
        cache_message_ = nullptr;
        return;
      }
    } else if (std::is_same_v<H_IMPL, Envoy::Http::RequestHeaderMapImpl>) {
      if (header_ptr->empty() || !header_ptr->Host() || !header_ptr->Path() ||
          !header_ptr->Method()) {
        cache_message_ = nullptr;
        return;
      }
    }

    cache_message_.reset(new M_IMPL(std::move(header_ptr)));

    if (doc.HasMember(rawbody.c_str())) {
      const rapidjson::Value& value = doc[rawbody.c_str()];
      cache_message_->body().add(value.GetString(), value.GetStringLength());
    }

    cache_message_->headers().removeTransferEncoding();

    auto content_length = cache_message_->body().length();
    cache_message_->headers().setContentLength(content_length);
  }

  absl::optional<std::string> serializeAsString() override {
    if (!cache_message_) {
      return absl::nullopt;
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> result_writer(buffer);
    result_writer.StartObject();
    result_writer.Key("headers");
    result_writer.StartObject();
    cache_message_->headers().iterate([&result_writer](const Envoy::Http::HeaderEntry& e) {
      return headersToJson(e, result_writer);
    });

    result_writer.EndObject();
    result_writer.Key("rawbody");
    auto body_string = cache_message_->bodyAsString();
    result_writer.String(body_string.data(), body_string.size());
    result_writer.EndObject();
    std::string result_string(buffer.GetString(), buffer.GetSize());
    return result_string;
  }

  CacheEntryPtr createCopy() override {
    return cache_message_ ? std::make_unique<HttpCacheEntryBase<M, M_IMPL, H_IMPL>>(
                                Http::makeMessageCopy(cache_message_), 0)
                          : nullptr;
  }

  uint64_t cacheExpire() override { return cache_expire_; }
  void cacheExpire(uint64_t new_expire_) override { cache_expire_ = new_expire_; };

  uint64_t cacheLength() override { return cache_length_; }

  std::unique_ptr<M>& cacheMessage() { return cache_message_; }
  void setCacheExpire(uint64_t expire) { cache_expire_ = expire; }

private:
  std::unique_ptr<M> cache_message_{};
  uint64_t cache_expire_{0};
  uint64_t cache_length_{0};
};

using HttpCacheEntry =
    HttpCacheEntryBase<Envoy::Http::ResponseMessage, Envoy::Http::ResponseMessageImpl,
                       Envoy::Http::ResponseHeaderMapImpl>;
using HttpCacheEntryPtr = std::unique_ptr<HttpCacheEntry>;

using HttpRequestCacheEntry =
    HttpCacheEntryBase<Envoy::Http::RequestMessage, Envoy::Http::RequestMessageImpl,
                       Envoy::Http::RequestHeaderMapImpl>;
using HttpRequestCacheEntryPtr = std::unique_ptr<HttpRequestCacheEntry>;

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
