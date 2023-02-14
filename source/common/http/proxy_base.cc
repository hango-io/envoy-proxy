#include "source/common/http/proxy_base.h"

#include <iostream>

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

HttpUriPtr HttpUri::makeHttpUri(const absl::string_view uri) {
  if (uri.find("://") == std::string::npos) {
    return nullptr;
  }
  absl::string_view scheme;
  absl::string_view host;
  absl::string_view path;

  scheme = uri.substr(0, uri.find("://"));

  if (scheme != "http" && scheme != "https") {
    return nullptr;
  }

  absl::string_view uri_no_scheme = uri.substr(uri.find("://") + 3);

  if (uri_no_scheme.empty()) {
    return nullptr;
  }

  size_t path_start = uri_no_scheme.find('/');

  if (path_start == std::string::npos) {
    host = uri_no_scheme;
    path = "/";
  } else {
    host = uri_no_scheme.substr(0, path_start);
    path = uri_no_scheme.substr(path_start);
  }
  if (host.empty()) {
    return nullptr;
  }

  HttpUriPtr result = std::make_unique<HttpUri>();
  result->scheme = std::string(scheme);
  result->host = std::string(host);
  result->path = std::string(path);
  return result;
}

Envoy::Http::ResponseMessagePtr makeMessageCopy(const Envoy::Http::ResponseMessagePtr& message) {
  auto header_ptr = Envoy::Http::ResponseHeaderMapImpl::create();
  Envoy::Http::HeaderMapImpl::copyFrom(*header_ptr, message->headers());

  auto result = std::make_unique<Envoy::Http::ResponseMessageImpl>(std::move(header_ptr));
  if (message->body().length() <= 0) {
    // Body 不存在或者 Body 大小为 0
    result->headers().removeTransferEncoding();
    result->headers().setContentLength(0);
  } else {
    // Body 存在且其大小不为 0
    result->body().add(message->body());
    result->headers().removeTransferEncoding();
    result->headers().setContentLength(result->body().length());
  }
  return std::move(result);
}

Envoy::Http::RequestMessagePtr makeMessageCopy(const Envoy::Http::RequestMessagePtr& message) {
  auto header_ptr = Envoy::Http::RequestHeaderMapImpl::create();
  Envoy::Http::HeaderMapImpl::copyFrom(*header_ptr, message->headers());

  auto result = std::make_unique<Envoy::Http::RequestMessageImpl>(std::move(header_ptr));
  if (message->body().length() <= 0) {
    // Body 不存在或者 Body 大小为 0
    result->headers().removeTransferEncoding();
    result->headers().setContentLength(0);
  } else {
    // Body 存在且其大小不为 0
    result->body().add(message->body());
    result->headers().removeTransferEncoding();
    result->headers().setContentLength(result->body().length());
  }
  return std::move(result);
}

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
