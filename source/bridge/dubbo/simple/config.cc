#include "source/bridge/dubbo/simple/config.h"

#include "envoy/common/exception.h"

namespace Envoy {
namespace Proxy {
namespace Bridge {
namespace Dubbo {
namespace Simple {

QueryGetter::QueryGetter(const Common::DubboBridge::DubboBridgeContext& context,
                         const Common::DubboBridge::SrcsRequestContext& source)
    : params_(
          Envoy::Http::Utility::parseAndDecodeQueryString(source.getHeader(":path").value_or("/"))),
      ignore_null_map_pair_(context.ignoreMapNullPair()) {
  if (context.lostParameterName()) {
    throw Envoy::EnvoyException(
        "Dubbo bridge: no parameter name is not allowed for query type source");
  }
}

Common::DubboBridge::HessianValueSharedPtr
QueryGetter::getArgumentByParameter(const Common::DubboBridge::Parameter& parameter) const {
  auto iter = params_.find(parameter.name_);
  if (iter == params_.end()) {
    return nullptr;
  }
  Common::DubboBridge::JsonDocument doc;
  doc.Parse(iter->second.data(), iter->second.size());

  if (doc.HasParseError()) {
    throw Envoy::EnvoyException("Dubbo bridge: query value not valid Json string");
  }

  return Common::DubboBridge::Utility::jsonValueToHessianValue(doc, parameter.type_,
                                                               ignore_null_map_pair_);
}

BodyGetter::BodyGetter(const Common::DubboBridge::DubboBridgeContext& context,
                       const Common::DubboBridge::SrcsRequestContext& source)
    : ignore_null_map_pair_(context.ignoreMapNullPair()),
      lost_parameter_name_(context.lostParameterName()) {
  doc_.Parse(source.getBody().data(), source.getBody().size());
  if (doc_.HasParseError()) {
    throw Envoy::EnvoyException("Dubbo bridge: HTTP request body not valid Json string");
  }

  if (lost_parameter_name_) {
    if (!doc_.IsArray()) {
      throw Envoy::EnvoyException("Dubbo bridge: no parameter name is only allowed for HTTP "
                                  "request body of type Json array");
    }
  }
}

Common::DubboBridge::HessianValueSharedPtr
BodyGetter::getArgumentByParameter(const Common::DubboBridge::Parameter& parameter) const {
  switch (doc_.GetType()) {
  case rapidjson::Type::kArrayType: {
    auto doc_array = doc_.GetArray();
    if (doc_array.Size() > parameter.index_) {
      return Common::DubboBridge::Utility::jsonValueToHessianValue(
          doc_array[parameter.index_], parameter.type_, ignore_null_map_pair_);
    }
    return nullptr;
  }
  case rapidjson::Type::kObjectType: {
    auto doc_object = doc_.GetObject();
    auto iter = doc_object.FindMember(parameter.name_);
    if (iter != doc_object.MemberEnd()) {
      return Common::DubboBridge::Utility::jsonValueToHessianValue(iter->value, parameter.type_,
                                                                   ignore_null_map_pair_);
    }
    return nullptr;
  }
  default:
    return nullptr;
  }
}

SimpleArgumentGetter::SimpleArgumentGetter(const Common::DubboBridge::DubboBridgeContext& context,
                                           const Common::DubboBridge::SrcsRequestContext& source) {
  if (context.getParametersSize() == 0) {
    inner_getter_ = std::make_unique<EmptyGetter>();
    return;
  }

  if (context.getArgumentSource() == Common::DubboBridge::DubboBridgeContext::Source::Body) {
    inner_getter_ = std::make_unique<BodyGetter>(context, source);
  } else {
    inner_getter_ = std::make_unique<QueryGetter>(context, source);
  }
}

Common::DubboBridge::ArgumentGetterCreator
SimpleArgumentGetterCreatorFactory::createArgumentGetterCreator(const Protobuf::Message&) {
  return [](const Common::DubboBridge::DubboBridgeContext& context,
            const Common::DubboBridge::SrcsRequestContext& source) {
    return std::make_unique<SimpleArgumentGetter>(context, source);
  };
}

Common::DubboBridge::ResponseGetter::Response
SimpleResponseGetter::getStringFromResponse(const Common::DubboBridge::HessianValuePtr& response,
                                            Type) const {
  return {Common::DubboBridge::Utility::hessianValueToJsonString(response.get()), {}};
}

Common::DubboBridge::ResponseGetterCreator
SimpleResponseGetterCreatorFactory::createResponseGetterCreator(const Protobuf::Message&) {
  return []() { return std::make_unique<SimpleResponseGetter>(); };
}

REGISTER_FACTORY(SimpleArgumentGetterCreatorFactory,
                 Common::DubboBridge::ArgumentGetterCreatorFactory);
REGISTER_FACTORY(SimpleResponseGetterCreatorFactory,
                 Common::DubboBridge::ResponseGetterCreatorFactory);

} // namespace Simple
} // namespace Dubbo
} // namespace Bridge
} // namespace Proxy
} // namespace Envoy
