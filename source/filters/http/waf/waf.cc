#include "source/filters/http/waf/waf.h"

#include "source/common/common/logger.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Waf {

const std::string waf_dir = "/etc/envoy/waf/common/";
const std::string modsecurity_conf_path = waf_dir + "modsecurity.conf";
const std::string crs_conf_path = waf_dir + "crs-setup.conf";
const std::string before_conf_path = waf_dir + "before.conf";
const std::string evaluation_conf_path = waf_dir + "evaluation.conf";
const std::string after_conf_path = waf_dir + "after.conf";

const std::string rule_str_prefix = "SecAction \"id:900700,phase:1,nolog,pass,t:none";
const std::string set_var = "setvar:";

RuleSetConfig::RuleSetConfig(const RuleSet& ruleset) {
  int size = ruleset.waf_rule_size();
  if (!size) {
    return;
  }
  rule_files_.push_back(modsecurity_conf_path);
  rule_files_.push_back(crs_conf_path);
  rule_files_.push_back(before_conf_path);
  for (int i = 0; i < size; i++) {
    auto& waf_rule = ruleset.waf_rule(i);
    rule_files_.push_back(waf_rule.waf_rule_path());
    for (auto& it : waf_rule.config().fields()) {
      std::string var_value;
      switch (it.second.kind_case()) {
      case ProtobufWkt::Value::kNumberValue:
        var_value = std::to_string(it.second.number_value());
        break;
      case ProtobufWkt::Value::kStringValue:
        var_value = it.second.string_value();
        break;
      case ProtobufWkt::Value::kBoolValue:
        var_value = it.second.bool_value() ? "true" : "false";
        break;
      default:
        throw EnvoyException(fmt::format("Unsupport waf var type"));
      }
      std::string var = "," + set_var + "'" + it.first + "=" + var_value + "'";
      rule_str_ += var;
    }
  }
  if (!rule_str_.empty()) {
    rule_str_ = absl::StrCat(rule_str_prefix, rule_str_, "\"");
  }

  rule_files_.push_back(evaluation_conf_path);
  rule_files_.push_back(after_conf_path);

  modsec_.reset(new modsecurity::ModSecurity());
  modsec_->setConnectorInformation("ModSecurity-envoy");

  modsec_rules_.reset(new modsecurity::RulesSet());

  if (!rule_str_.empty()) {
    ENVOY_LOG(debug, "rule_str_: {}", rule_str_);
    int rulesLoaded = modsec_rules_->load(rule_str_.c_str());
    ENVOY_LOG(debug, "Loading ModSecurity string rules");
    if (rulesLoaded == -1) {
      ENVOY_LOG(error, "Failed to load rules: {}", modsec_rules_->getParserError());
    } else {
      ENVOY_LOG(debug, "Loaded {} string rules", rulesLoaded);
    }
  }

  for (int i = 0; i < rule_files_.size(); i++) {
    if (!rule_files_[i].empty()) {
      int rulesLoaded = modsec_rules_->loadFromUri(rule_files_[i].c_str());
      ENVOY_LOG(debug, "Loading ModSecurity config from {}", rule_files_[i]);
      if (rulesLoaded == -1) {
        ENVOY_LOG(error, "Failed to load rules: {}", modsec_rules_->getParserError());
      } else {
        ENVOY_LOG(debug, "Loaded {} rules", rulesLoaded);
      }
    }
  }
}

WafFilter::WafFilter(const RuleSetConfig* config)
    : config_(config), intervined_(false), request_processed_(false), response_processed_(false),
      end_stream_(false), is_send_local_reply_(false) {
  if (config_->modsec_rules_) {
    modsec_transaction_.reset(
        new modsecurity::Transaction(config_->modsec_.get(), config_->modsec_rules_.get(), this));
  }
}

const char* getProtocolString(const Http::Protocol protocol) {
  switch (protocol) {
  case Http::Protocol::Http10:
    return "1.0";
  case Http::Protocol::Http11:
    return "1.1";
  case Http::Protocol::Http2:
    return "2.0";
  case Http::Protocol::Http3:
    return "3.0";
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

Http::FilterHeadersStatus WafFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                   bool end_stream) {
  if (intervined_ || request_processed_) {
    ENVOY_LOG(debug, "Processed");
    return getRequestHeadersStatus();
  }

  auto route_config = Http::Utility::resolveMostSpecificPerFilterConfig<RuleSetConfig>(
      name(), decoder_callbacks_->route());

  if (route_config && route_config->modsec_rules_) {
    modsec_transaction_route_.reset(new modsecurity::Transaction(
        route_config->modsec_.get(), route_config->modsec_rules_.get(), this));
  }

  if (!modsec_transaction_ && !modsec_transaction_route_) {
    return Http::FilterHeadersStatus::Continue;
  }

  auto downstreamAddress =
      decoder_callbacks_->streamInfo().downstreamAddressProvider().localAddress();
  // TODO - Upstream is (always?) still not resolved in this stage. Use our local proxy's ip. Is
  // this what we want?
  ASSERT(decoder_callbacks_->connection() != nullptr);
  auto localAddress = decoder_callbacks_->connection()->connectionInfoProvider().localAddress();
  // According to documentation, downstreamAddress should never be nullptr
  ASSERT(downstreamAddress != nullptr);
  ASSERT(downstreamAddress->type() == Network::Address::Type::Ip);
  ASSERT(localAddress != nullptr);
  ASSERT(localAddress->type() == Network::Address::Type::Ip);

  if (modsec_transaction_) {
    modsec_transaction_->processConnection(
        downstreamAddress->ip()->addressAsString().c_str(), downstreamAddress->ip()->port(),
        localAddress->ip()->addressAsString().c_str(), localAddress->ip()->port());
    if (intervention()) {
      is_send_local_reply_ = true;
      decoder_callbacks_->sendLocalReply(
          static_cast<Http::Code>(status_), "ModSecurity Action.", [](Http::HeaderMap& headers) {},
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  if (modsec_transaction_route_) {
    modsec_transaction_route_->processConnection(
        downstreamAddress->ip()->addressAsString().c_str(), downstreamAddress->ip()->port(),
        localAddress->ip()->addressAsString().c_str(), localAddress->ip()->port());
    if (interventionRoute()) {
      is_send_local_reply_ = true;
      decoder_callbacks_->sendLocalReply(
          static_cast<Http::Code>(status_), "ModSecurity Action.", [](Http::HeaderMap& headers) {},
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  auto uri = headers.Path();
  auto method = headers.Method();

  if (modsec_transaction_) {
    modsec_transaction_->processURI(
        std::string(uri->value().getStringView()).c_str(),
        std::string(method->value().getStringView()).c_str(),
        getProtocolString(
            decoder_callbacks_->streamInfo().protocol().value_or(Http::Protocol::Http11)));
    if (intervention()) {
      is_send_local_reply_ = true;
      decoder_callbacks_->sendLocalReply(
          static_cast<Http::Code>(status_), "ModSecurity Action.", [](Http::HeaderMap& headers) {},
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  if (modsec_transaction_route_) {
    modsec_transaction_route_->processURI(
        std::string(uri->value().getStringView()).c_str(),
        std::string(method->value().getStringView()).c_str(),
        getProtocolString(
            decoder_callbacks_->streamInfo().protocol().value_or(Http::Protocol::Http11)));
    if (interventionRoute()) {
      is_send_local_reply_ = true;
      decoder_callbacks_->sendLocalReply(
          static_cast<Http::Code>(status_), "ModSecurity Action.", [](Http::HeaderMap& headers) {},
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  headers.iterate([&modsec_transaction_ = modsec_transaction_,
                   &modsec_transaction_route_ = modsec_transaction_route_](
                      const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    std::string k = std::string(header.key().getStringView());
    std::string v = std::string(header.value().getStringView());
    if (modsec_transaction_) {
      modsec_transaction_->addRequestHeader(k.c_str(), v.c_str());
    }
    if (modsec_transaction_route_) {
      modsec_transaction_route_->addRequestHeader(k.c_str(), v.c_str());
    }
    // TODO - does this special case makes sense? it doesn't exist on apache/nginx modsecurity
    // bridges. host header is cannonized to :authority even on http older than 2 see
    // https://github.com/envoyproxy/envoy/issues/2209
    if (k == Http::Headers::get().Host.get()) {
      if (modsec_transaction_) {
        modsec_transaction_->addRequestHeader(Http::Headers::get().HostLegacy.get().c_str(),
                                              v.c_str());
      }
      if (modsec_transaction_route_) {
        modsec_transaction_route_->addRequestHeader(Http::Headers::get().HostLegacy.get().c_str(),
                                                    v.c_str());
      }
    }
    return Http::HeaderMap::Iterate::Continue;
  });

  if (end_stream) {
    request_processed_ = true;
  }

  if (modsec_transaction_) {
    modsec_transaction_->processRequestHeaders();
    if (intervention()) {
      is_send_local_reply_ = true;
      decoder_callbacks_->sendLocalReply(
          static_cast<Http::Code>(status_), "ModSecurity Action.", [](Http::HeaderMap& headers) {},
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  if (modsec_transaction_route_) {

    modsec_transaction_route_->processRequestHeaders();
    if (interventionRoute()) {
      is_send_local_reply_ = true;
      decoder_callbacks_->sendLocalReply(
          static_cast<Http::Code>(status_), "ModSecurity Action.", [](Http::HeaderMap& headers) {},
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }
  return getRequestHeadersStatus();
}

Http::FilterDataStatus WafFilter::decodeData(Buffer::Instance& data, bool end_stream) {
  if (!modsec_transaction_ && !modsec_transaction_route_) {
    return Http::FilterDataStatus::Continue;
  }
  if (intervined_ || request_processed_) {
    ENVOY_LOG(debug, "Processed");
    return getRequestStatus();
  }

  Buffer::RawSliceVector slices = data.getRawSlices();
  for (const Buffer::RawSlice& slice : slices) {
    if (modsec_transaction_) {
      size_t requestLen = modsec_transaction_->getRequestBodyLength();
      // If append fails or append reached the limit, test for intervention (in case
      // SecRequestBodyLimitAction is set to Reject) Note, we can't rely solely on the return value
      // of append, when SecRequestBodyLimitAction is set to Reject it returns true and sets the
      // intervention
      if (modsec_transaction_->appendRequestBody(static_cast<unsigned char*>(slice.mem_),
                                                 slice.len_) == false ||
          (slice.len_ > 0 && requestLen == modsec_transaction_->getRequestBodyLength())) {
        ENVOY_LOG(debug, "WafFilter::decodeData appendRequestBody reached limit");
        end_stream = true;
        break;
      }
    }
    if (modsec_transaction_route_) {
      size_t requestLen_route = modsec_transaction_route_->getRequestBodyLength();
      if (modsec_transaction_route_->appendRequestBody(static_cast<unsigned char*>(slice.mem_),
                                                       slice.len_) == false ||
          (slice.len_ > 0 &&
           requestLen_route == modsec_transaction_route_->getRequestBodyLength())) {
        ENVOY_LOG(debug, "WafFilter::decodeData appendRequestBody reached limit");
        end_stream = true;
        break;
      }
    }
  }

  if (end_stream) {
    request_processed_ = true;
    if (modsec_transaction_) {
      modsec_transaction_->processRequestBody();
      if (intervention()) {
        is_send_local_reply_ = true;
        decoder_callbacks_->sendLocalReply(
            static_cast<Http::Code>(status_), "ModSecurity Action.",
            [](Http::HeaderMap& headers) {}, absl::nullopt, "");
        return Http::FilterDataStatus::StopIterationNoBuffer;
      }
    }
    if (modsec_transaction_route_) {
      modsec_transaction_route_->processRequestBody();
      if (interventionRoute()) {
        is_send_local_reply_ = true;
        decoder_callbacks_->sendLocalReply(
            static_cast<Http::Code>(status_), "ModSecurity Action.",
            [](Http::HeaderMap& headers) {}, absl::nullopt, "");
        return Http::FilterDataStatus::StopIterationNoBuffer;
      }
    }
  }

  return getRequestStatus();
}

Http::FilterHeadersStatus WafFilter::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                   bool end_stream) {
  if (!modsec_transaction_ && !modsec_transaction_route_) {
    return Http::FilterHeadersStatus::Continue;
  }
  if (intervined_ || response_processed_) {
    ENVOY_LOG(debug, "Processed");
    return getResponseHeadersStatus();
  }
  end_stream_ = end_stream;
  response_headers_ = &headers;

  uint64_t code = Http::Utility::getResponseStatus(headers);
  headers.iterate([&modsec_transaction_ = modsec_transaction_,
                   &modsec_transaction_route_ = modsec_transaction_route_](
                      const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    if (modsec_transaction_) {
      modsec_transaction_->addResponseHeader(std::string(header.key().getStringView()).c_str(),
                                             std::string(header.value().getStringView()).c_str());
    }
    if (modsec_transaction_route_) {
      modsec_transaction_route_->addResponseHeader(
          std::string(header.key().getStringView()).c_str(),
          std::string(header.value().getStringView()).c_str());
    }
    return Http::HeaderMap::Iterate::Continue;
  });

  if (modsec_transaction_) {
    modsec_transaction_->processResponseHeaders(
        code, getProtocolString(
                  encoder_callbacks_->streamInfo().protocol().value_or(Http::Protocol::Http11)));
    if (intervention()) {
      sendEncodeReply(status_);
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  if (modsec_transaction_route_) {
    modsec_transaction_route_->processResponseHeaders(
        code, getProtocolString(
                  encoder_callbacks_->streamInfo().protocol().value_or(Http::Protocol::Http11)));
    if (interventionRoute()) {
      sendEncodeReply(status_);
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  return getResponseHeadersStatus();
}

Http::FilterDataStatus WafFilter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!modsec_transaction_ && !modsec_transaction_route_) {
    return Http::FilterDataStatus::Continue;
  }
  if (response_processed_ || is_send_local_reply_) {
    ENVOY_LOG(debug, "Processed");
    return getResponseStatus();
  }
  end_stream_ = end_stream;

  if (intervined_) {
    data.drain(data.length());
    if (end_stream_) {
      sendEncodeReply(status_);
      return getResponseStatus();
    }
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }

  Buffer::RawSliceVector slices = data.getRawSlices();
  for (const Buffer::RawSlice& slice : slices) {
    if (modsec_transaction_) {
      size_t responseLen = modsec_transaction_->getResponseBodyLength();
      // If append fails or append reached the limit, test for intervention (in case
      // SecResponseBodyLimitAction is set to Reject) Note, we can't rely solely on the return value
      // of append, when SecResponseBodyLimitAction is set to Reject it returns true and sets the
      // intervention
      if (modsec_transaction_->appendResponseBody(static_cast<unsigned char*>(slice.mem_),
                                                  slice.len_) == false ||
          (slice.len_ > 0 && responseLen == modsec_transaction_->getResponseBodyLength())) {
        ENVOY_LOG(debug, "WafFilter::encodeData appendResponseBody reached limit");
        end_stream = true;
        break;
      }
    }
    if (modsec_transaction_route_) {
      size_t responseLen_route = modsec_transaction_route_->getResponseBodyLength();
      if (modsec_transaction_route_->appendResponseBody(static_cast<unsigned char*>(slice.mem_),
                                                        slice.len_) == false ||
          (slice.len_ > 0 &&
           responseLen_route == modsec_transaction_route_->getResponseBodyLength())) {
        ENVOY_LOG(debug, "WafFilter::encodeData appendResponseBody reached limit");
        end_stream = true;
        break;
      }
    }
  }

  if (end_stream) {
    response_processed_ = true;
    if (modsec_transaction_) {
      modsec_transaction_->processResponseBody();
      if (intervention()) {
        sendEncodeReply(status_);
        return Http::FilterDataStatus::Continue;
      }
    }
    if (modsec_transaction_route_) {
      modsec_transaction_route_->processResponseBody();
      if (interventionRoute()) {
        sendEncodeReply(status_);
        return Http::FilterDataStatus::Continue;
      }
    }
  }

  return getResponseStatus();
}

bool WafFilter::intervention() {
  modsecurity::ModSecurityIntervention it;
  modsecurity::intervention::reset(&it);

  if (!intervined_ && modsec_transaction_->intervention(&it)) {
    // intervined_ must be set to true before sendLocalReply to avoid reentrancy when encoding the
    // reply
    intervined_ = true;
    status_ = it.status;
    ENVOY_LOG(debug, "intervention");
  }
  return intervined_;
}

bool WafFilter::interventionRoute() {
  modsecurity::ModSecurityIntervention it;
  modsecurity::intervention::reset(&it);
  if (!intervined_ && modsec_transaction_route_->intervention(&it)) {
    // intervined_ must be set to true before sendLocalReply to avoid reentrancy when encoding the
    // reply
    intervined_ = true;
    status_ = it.status;
    ENVOY_LOG(debug, "intervention");
  }
  return intervined_;
}

Http::FilterHeadersStatus WafFilter::getRequestHeadersStatus() {
  if (intervined_) {
    ENVOY_LOG(debug, "StopIteration");
    return Http::FilterHeadersStatus::StopIteration;
  }
  if (request_processed_) {
    ENVOY_LOG(debug, "Continue");
    return Http::FilterHeadersStatus::Continue;
  }
  // If disruptive, hold until request_processed_, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return enabledRuleEngine() ? Http::FilterHeadersStatus::StopIteration
                             : Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus WafFilter::getRequestStatus() {
  if (intervined_) {
    ENVOY_LOG(debug, "StopIterationNoBuffer");
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }
  if (request_processed_) {
    ENVOY_LOG(debug, "Continue");
    return Http::FilterDataStatus::Continue;
  }
  // If disruptive, hold until request_processed_, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return enabledRuleEngine() ? Http::FilterDataStatus::StopIterationAndBuffer
                             : Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus WafFilter::getResponseHeadersStatus() {
  if (intervined_ || response_processed_) {
    // If intervined, let encodeData return the localReply
    ENVOY_LOG(debug, "Continue");
    return Http::FilterHeadersStatus::Continue;
  }
  // If disruptive, hold until response_processed_, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return enabledRuleEngine() ? Http::FilterHeadersStatus::StopIteration
                             : Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus WafFilter::getResponseStatus() {
  if (intervined_ || response_processed_) {
    // If intervined, let encodeData return the localReply
    ENVOY_LOG(debug, "Continue");
    return Http::FilterDataStatus::Continue;
  }
  // If disruptive, hold until response_processed_, otherwise let the data flow.
  ENVOY_LOG(debug, "RuleEngine");
  return enabledRuleEngine() ? Http::FilterDataStatus::StopIterationAndBuffer
                             : Http::FilterDataStatus::Continue;
}

bool WafFilter::enabledRuleEngine() {
  bool enabled_rule_engine = false;
  if (modsec_transaction_) {
    enabled_rule_engine =
        modsec_transaction_->getRuleEngineState() == modsecurity::RulesSet::EnabledRuleEngine;
  }
  if (modsec_transaction_route_) {
    enabled_rule_engine =
        modsec_transaction_route_->getRuleEngineState() == modsecurity::RulesSet::EnabledRuleEngine;
  }
  return enabled_rule_engine;
}

void WafFilter::sendEncodeReply(uint64_t status) {
  if (end_stream_) {
    response_headers_->setStatus(status);
    Buffer::OwnedImpl body("ModSecurity Action.");
    response_headers_->removeContentLength();
    response_headers_->setContentLength(body.length());
    if (encoder_callbacks_->encodingBuffer() == nullptr) {
      encoder_callbacks_->addEncodedData(body, false);
    } else {
      encoder_callbacks_->modifyEncodingBuffer([&body](Buffer::Instance& buffered_body) {
        buffered_body.drain(buffered_body.length());
        buffered_body.move(body, body.length());
      });
    }
  }
}

void WafFilter::onDestroy() {
  if (modsec_transaction_) {
    modsec_transaction_->processLogging();
  }
  if (modsec_transaction_route_) {
    modsec_transaction_route_->processLogging();
  }
}

} // namespace Waf
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
