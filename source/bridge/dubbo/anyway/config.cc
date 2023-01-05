#include "source/bridge/dubbo/anyway/config.h"

#include "envoy/common/exception.h"

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Proxy {
namespace Bridge {
namespace Dubbo {
namespace Anyway {

using namespace Common::DubboBridge;

namespace {

// bool isBffGateway() {
//   static const bool is_bff_gateway = std::getenv("ENVOY_BFF_GATEWAY") != nullptr;
//   return is_bff_gateway;
// }

absl::string_view gatewayInstance() {
  static const std::string instance =
      std::getenv("INSTANCE_IP") != nullptr ? std::getenv("INSTANCE_IP") : "0.0.0.0";
  return instance;
}

DubboBridgeContext::Source downgradeArgument(DubboBridgeContext::Source from,
                                             absl::string_view body, absl::string_view url) {
  if (from == DubboBridgeContext::Source::Body) {
    return body.empty() ? DubboBridgeContext::Source::Query : DubboBridgeContext::Source::Body;
  } else {
    auto querys = Envoy::Http::Utility::parseAndDecodeQueryString(url);
    return querys.empty() ? DubboBridgeContext::Source::Body : DubboBridgeContext::Source::Query;
  }
}

DubboBridgeContext::Source finallyDowngradeSource(const DubboBridgeContext& context,
                                                  const SrcsRequestContext& source) {

  bool is_get_method = source.getHeader(":method").value_or("") == "GET";

  // GET method downgrade
  if (is_get_method) {
    // have fieldMapping
    if (!context.lostParameterName()) {
      // get argument
      if (context.getArgumentSource() == DubboBridgeContext::Source::Body) {
        if (source.getBody().empty()) {
          return DubboBridgeContext::Source::Query;
        } else {
          return DubboBridgeContext::Source::Body;
        }
      }
    }
    return DubboBridgeContext::Source::Query;
  }

  // NO GET Method downgrade
  return downgradeArgument(context.getArgumentSource(), source.getBody(),
                           source.getHeader(":path").value_or("/"));
}

constexpr absl::string_view NULL_VIEW = "null";

#define CHECK_NULL_VALUE(X)                                                                        \
  if (X == NULL_VIEW) {                                                                            \
    return std::make_unique<Hessian2::NullObject>();                                               \
  }

#define CAN_NOT_BE_EMPTY_AND_NULL(X)                                                               \
  if (X == NULL_VIEW || X.empty()) {                                                               \
    throw Envoy::EnvoyException(                                                                   \
        "Dubbo bridge: empty string or null is not valid for this parameter");                     \
  }

#define CAN_NOT_BE_EMPTY_BUT_NULL(X)                                                               \
  if (X.empty()) {                                                                                 \
    throw Envoy::EnvoyException("Dubbo bridge: empty string is not valid for this parameter");     \
  }                                                                                                \
  CHECK_NULL_VALUE(X);

#define SIMPLE_CONVERTER(TYPE, CONVERTER, TARGET_TYPE, X)                                          \
  TYPE value{};                                                                                    \
  if (!CONVERTER(X, &value)) {                                                                     \
    throw Envoy::EnvoyException(                                                                   \
        fmt::format("Dubbo bridge: invalid value '{}' for type '{}'", view, typeid(TYPE).name())); \
  }                                                                                                \
  return std::make_unique<TARGET_TYPE>(value);

#define NOT_NULLABLE_SIMPLE_CONVERTER(TYPE, CONVERTER, TARGET_TYPE, X)                             \
  CAN_NOT_BE_EMPTY_AND_NULL(X);                                                                    \
  SIMPLE_CONVERTER(TYPE, CONVERTER, TARGET_TYPE, X);

#define NULLABLE_SIMPLE_CONVERTER(TYPE, CONVERTER, TARGET_TYPE, X)                                 \
  CAN_NOT_BE_EMPTY_BUT_NULL(X);                                                                    \
  SIMPLE_CONVERTER(TYPE, CONVERTER, TARGET_TYPE, X);

using Converter = std::function<HessianValuePtr(absl::string_view)>;

static absl::flat_hash_map<std::string, Converter> TypedStringConverters{
    // Bool.
    {"java.lang.Boolean",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(bool, absl::SimpleAtob, Hessian2::BooleanObject, view);
     }},

    {"boolean",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(bool, absl::SimpleAtob, Hessian2::BooleanObject, view);
     }},

    // Number.
    {"java.lang.Float",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(double, absl::SimpleAtod, Hessian2::DoubleObject, view);
     }},

    {"java.lang.Double",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(double, absl::SimpleAtod, Hessian2::DoubleObject, view);
     }},

    {"java.lang.Byte",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(int32_t, absl::SimpleAtoi, Hessian2::IntegerObject, view);
     }},

    {"java.lang.Short",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(int32_t, absl::SimpleAtoi, Hessian2::IntegerObject, view);
     }},

    {"java.lang.Integer",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(int32_t, absl::SimpleAtoi, Hessian2::IntegerObject, view);
     }},

    {"java.lang.Long",
     [](absl::string_view view) -> HessianValuePtr {
       NULLABLE_SIMPLE_CONVERTER(int64_t, absl::SimpleAtoi, Hessian2::LongObject, view);
     }},

    {"float",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(double, absl::SimpleAtod, Hessian2::DoubleObject, view);
     }},

    {"double",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(double, absl::SimpleAtod, Hessian2::DoubleObject, view);
     }},

    {"byte",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(int32_t, absl::SimpleAtoi, Hessian2::IntegerObject, view);
     }},

    {"short",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(int32_t, absl::SimpleAtoi, Hessian2::IntegerObject, view);
     }},

    {"int",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(int32_t, absl::SimpleAtoi, Hessian2::IntegerObject, view);
     }},

    {"long",
     [](absl::string_view view) -> HessianValuePtr {
       NOT_NULLABLE_SIMPLE_CONVERTER(int64_t, absl::SimpleAtoi, Hessian2::LongObject, view);
     }},

    // Char.
    {"java.lang.Character",
     [](absl::string_view view) -> HessianValuePtr {
       CAN_NOT_BE_EMPTY_BUT_NULL(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    {"char",
     [](absl::string_view view) -> HessianValuePtr {
       CAN_NOT_BE_EMPTY_AND_NULL(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    // String.
    {"java.lang.String",
     [](absl::string_view view) -> HessianValuePtr {
       CHECK_NULL_VALUE(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    // Others.
    {"java.lang.Enum",
     [](absl::string_view view) -> HessianValuePtr {
       CAN_NOT_BE_EMPTY_BUT_NULL(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    {"java.math.BigDecimal",
     [](absl::string_view view) -> HessianValuePtr {
       CAN_NOT_BE_EMPTY_BUT_NULL(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    {"java.math.BigInteger",
     [](absl::string_view view) -> HessianValuePtr {
       CAN_NOT_BE_EMPTY_BUT_NULL(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    {"java.time.DayOfWeek",
     [](absl::string_view view) -> HessianValuePtr {
       CAN_NOT_BE_EMPTY_BUT_NULL(view);
       return std::make_unique<Hessian2::StringObject>(view);
     }},

    {"java.lang.Void",
     [](absl::string_view) -> HessianValuePtr { return std::make_unique<Hessian2::NullObject>(); }},
};

HessianValuePtr defaultStringConverter(absl::string_view value, bool ignore_map_null_pair) {
  rapidjson::Document document;

  if (value.size() > 0) {
    document.Parse(std::string(value).c_str(), value.size());
    if (document.HasParseError()) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: complex type is not valid Json string and please check your type");
    }
  } else {
    throw Envoy::EnvoyException("Dubbo bridge: empty string is not valid for this parameter");
  }

  return Utility::jsonValueToHessianValue(document, "", ignore_map_null_pair);
}

HessianValuePtr stringValueToHessianValue(absl::string_view text, absl::string_view type,
                                          bool ignore_map_null_pair) {
  auto iter = TypedStringConverters.find(type);
  if (iter == TypedStringConverters.end()) {
    return defaultStringConverter(text, ignore_map_null_pair);
  } else {
    return iter->second(text);
  }
}

enum RequestBodyType { NoneBody, JsonBody, FormBody, TextBody };

class QueryArgumentGetter : public ArgumentGetter {
public:
  QueryArgumentGetter(const DubboBridgeContext& context, const SrcsRequestContext& source) {
    parameter_size_ = context.getParametersSize();
    lost_parameter_name_ = context.lostParameterName();
    ignore_null_map_pair_ = context.ignoreMapNullPair();

    if (lost_parameter_name_ && parameter_size_ > 1) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: lost parameter name but multiple arguments is necessary for query source");
    }

    queries_ =
        Envoy::Http::Utility::parseAndDecodeQueryString(source.getHeader(":path").value_or("/"));
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const override {
    if (parameter_size_ == 1) {
      if (lost_parameter_name_) {
        if (queries_.size() == 1) {
          return stringValueToHessianValue(queries_.begin()->second, parameter.type_,
                                           ignore_null_map_pair_);
        } else if (queries_.size() == 0) {
          return nullptr;
        } else {
          throw Envoy::EnvoyException(
              "Dubbo bridge: lost parameter name but multiple queries is provided");
        }
      } else {
        if (queries_.size() == 1) {
          return stringValueToHessianValue(queries_.begin()->second, parameter.type_,
                                           ignore_null_map_pair_);
        }
        auto iter = queries_.find(parameter.name_);
        if (iter == queries_.end()) {
          return nullptr;
        }
        return stringValueToHessianValue(iter->second, parameter.type_, ignore_null_map_pair_);
      }
    } else {
      // The case that has multiple parameters and lost parameter name will be rejected by the
      // constructor.
      ASSERT(parameter_size_ > 1);
      ASSERT(!lost_parameter_name_);
      auto iter = queries_.find(parameter.name_);
      if (iter == queries_.end()) {
        return nullptr;
      }
      return stringValueToHessianValue(iter->second, parameter.type_, ignore_null_map_pair_);
    }

    return nullptr;
  }

  size_t parameter_size_{};
  bool lost_parameter_name_{};
  bool ignore_null_map_pair_{};
  Envoy::Http::Utility::QueryParams queries_;
};

class TextBodyArgumentGetter : public ArgumentGetter {
public:
  TextBodyArgumentGetter(const DubboBridgeContext& context, const SrcsRequestContext& source) {
    if (context.getParametersSize() > 1) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: multiple parameters is not allowd for plain body source");
    }
    ignore_null_map_pair_ = context.ignoreMapNullPair();

    raw_body_view_ = source.getBody();
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const override {
    return stringValueToHessianValue(raw_body_view_, parameter.type_, ignore_null_map_pair_);
  }

  bool ignore_null_map_pair_{};
  absl::string_view raw_body_view_;
};

class JsonBodyArgumentGetter : public ArgumentGetter {
public:
  JsonBodyArgumentGetter(const DubboBridgeContext& context, const SrcsRequestContext& source) {
    parameters_size_ = context.getParametersSize();
    lost_parameter_name_ = context.lostParameterName();
    ignore_null_map_pair_ = context.ignoreMapNullPair();

    if (!source.getBody().empty()) {
      doc_.Parse(source.getBody().data(), source.getBody().size());
      if (doc_.HasParseError()) {
        if (parameters_size_ == 1) {
          doc_.SetString(source.getBody().data(), source.getBody().size());
        } else {
          throw Envoy::EnvoyException("Dubbo bridge: error Json body for multiple parameters");
        }
      }
    } else {
      doc_.SetString(source.getBody().data(), source.getBody().size());
    }
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const override {
    if (parameters_size_ == 1) {
      if (lost_parameter_name_) {
        return Utility::jsonValueToHessianValue(doc_, parameter.type_, ignore_null_map_pair_);
      } else {
        if (doc_.IsObject()) {
          auto memeter_iter = doc_.GetObject().FindMember(parameter.name_.c_str());
          if (memeter_iter != doc_.GetObject().MemberEnd()) {
            return Utility::jsonValueToHessianValue(memeter_iter->value, parameter.type_,
                                                    ignore_null_map_pair_);
          } else {
            return nullptr;
          }
        }
        return Utility::jsonValueToHessianValue(doc_, parameter.type_, ignore_null_map_pair_);
      }
    } else {
      ASSERT(parameters_size_ > 1);
      if (lost_parameter_name_) {
        if (doc_.IsArray()) {
          if (parameter.index_ >= doc_.GetArray().Size()) {
            return nullptr;
          }
          return Utility::jsonValueToHessianValue(doc_.GetArray()[parameter.index_],
                                                  parameter.type_, ignore_null_map_pair_);
        }
        return nullptr;
      } else {
        if (doc_.IsObject()) {
          auto memeter_iter = doc_.GetObject().FindMember(parameter.name_.c_str());
          if (memeter_iter != doc_.GetObject().MemberEnd()) {
            return Utility::jsonValueToHessianValue(memeter_iter->value, parameter.type_,
                                                    ignore_null_map_pair_);
          }
        }
        return nullptr;
      }
    }
  }

  bool ignore_null_map_pair_{};
  size_t parameters_size_{};
  bool lost_parameter_name_{};
  Json::Document doc_;
};

class FormBodyArgumentGetter : public ArgumentGetter {
public:
  FormBodyArgumentGetter(const DubboBridgeContext& context, const SrcsRequestContext& source) {
    parameter_size_ = context.getParametersSize();
    lost_parameter_name_ = context.lostParameterName();
    ignore_null_map_pair_ = context.ignoreMapNullPair();

    if (lost_parameter_name_ && parameter_size_ > 1) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: lost parameter name but multiple arguments is necessary for query source");
    }

    queries_ = Envoy::Http::Utility::parseFromBody(source.getBody());
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const override {
    if (parameter_size_ == 1) {
      if (lost_parameter_name_) {
        Json::Document document;
        auto& allocator = document.GetAllocator();
        document.SetObject();
        for (auto it = queries_.begin(); it != queries_.end(); it++) {
          rapidjson::Value key(rapidjson::kStringType);
          rapidjson::Value val(rapidjson::kStringType);
          key.SetString(it->first.c_str(), it->first.size(), allocator);
          val.SetString(it->second.c_str(), it->second.size(), allocator);
          document.AddMember(std::move(key), std::move(val), allocator);
        }
        return Utility::jsonValueToHessianValue(document, parameter.type_, ignore_null_map_pair_);
      } else {
        auto iter = queries_.find(parameter.name_);
        if (iter == queries_.end()) {
          return nullptr;
        }
        return stringValueToHessianValue(iter->second, parameter.type_, ignore_null_map_pair_);
      }
    } else {
      // The case that has multiple parameters and lost parameter name will be rejected by the
      // constructor.
      ASSERT(parameter_size_ > 1);
      ASSERT(!lost_parameter_name_);
      auto iter = queries_.find(parameter.name_);
      if (iter == queries_.end()) {
        return nullptr;
      }
      return stringValueToHessianValue(iter->second, parameter.type_, ignore_null_map_pair_);
    }

    return nullptr;
  }

  size_t parameter_size_{};
  bool lost_parameter_name_{};
  bool ignore_null_map_pair_{};
  Envoy::Http::Utility::QueryParams queries_;
};

class EmptyGetter : public ArgumentGetter {
public:
  HessianValueSharedPtr getArgumentByParameter(const Parameter&) const override { return nullptr; }
};

} // namespace

class AnywayArgumentGetter : public ArgumentGetter {
public:
  AnywayArgumentGetter(const DubboBridgeContext& context, const SrcsRequestContext& source,
                       absl::optional<absl::string_view> prefix) {
    if (prefix.has_value()) {
      source.iterateHeaders([this, prefix](absl::string_view key, absl::string_view value) {
        if (absl::StartsWith(key, prefix.value())) {
          attatchments_.emplace(std::string(key), std::string(value));
        }
        return true;
      });
    }

    // No parameter.
    if (context.getParametersSize() == 0) {
      inner_getter_ = std::make_unique<EmptyGetter>();
      return;
    }

    const auto downgrade_source = finallyDowngradeSource(context, source);

    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), debug,
                        "downgrade source: {}", downgrade_source);

    if (downgrade_source == DubboBridgeContext::Source::Query) {
      inner_getter_ = std::make_unique<QueryArgumentGetter>(context, source);
      return;
    }

    RequestBodyType body_type = RequestBodyType::JsonBody;
    const auto content_type = source.getHeader("content-type");
    if (content_type.has_value()) {
      if (absl::StrContains(content_type.value(), "application/x-www-form-urlencoded")) {
        body_type = RequestBodyType::FormBody;
      } else if (absl::StrContains(content_type.value(), "text/plain")) {
        body_type = RequestBodyType::TextBody;
      }
    }

    if (body_type == RequestBodyType::JsonBody) {
      inner_getter_ = std::make_unique<JsonBodyArgumentGetter>(context, source);
      return;
    }

    if (body_type == RequestBodyType::FormBody) {
      inner_getter_ = std::make_unique<FormBodyArgumentGetter>(context, source);
      return;
    }

    if (body_type == RequestBodyType::TextBody) {
      inner_getter_ = std::make_unique<TextBodyArgumentGetter>(context, source);
      return;
    }

    throw Envoy::EnvoyException("Dubbo bridge: this should never be reached");
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const override {
    ASSERT(inner_getter_ != nullptr);
    return inner_getter_->getArgumentByParameter(parameter);
  }

  OptRef<const FlatStringMap> attachmentsFromGetter() const override {
    return makeOptRef<const FlatStringMap>(attatchments_);
  }

private:
  ArgumentGetterPtr inner_getter_;
  FlatStringMap attatchments_;
};

class AnywayResponseGetter : public ResponseGetter {
public:
  AnywayResponseGetter() = default;

  Response getStringFromResponse(const HessianValuePtr& response, Type type) const override {
    if (response == nullptr) {
      return {"null", {}};
    }

    if (type == Type::Exception) {
      auto result_value = Utility::hessianValueToJsonString(response.get());
      ASSERT(!result_value.empty());

      static const std::string format_string = "{{ \"instance\": \"{}\", \"result\": {} }}";
      return {fmt::format(format_string, gatewayInstance(), result_value),
              610}; // 610 for exception
                    // respnose.
    } else if (type == Type::Local) {
      auto result_value = Utility::hessianValueToJsonString(response.get());
      ASSERT(!result_value.empty());

      static const std::string format_string = "{{ \"instance\": \"{}\", \"result\": {} }}";
      return {fmt::format(format_string, gatewayInstance(), result_value), 400}; // 400 for local
                                                                                 // response.
    }

    switch (response->type()) {
    case Hessian2::Object::Type::Binary: {
      auto& typed_value = *(response->toBinary().value());

      return {std::string(reinterpret_cast<const char*>(typed_value.data()), typed_value.size()),
              200};
    }
    case Hessian2::Object::Type::String: {
      return {*response->toString().value(), 200};
    }
    default: {
      return {Utility::hessianValueToJsonString(response.get()), 200};
    }
    }
  }
};

ArgumentGetterCreator
AnywayArgumentGetterCreatorFactory::createArgumentGetterCreator(const Protobuf::Message&) {
  // If custom prefix is necessary, capture it here in the closure.
  // const std::string prefix = message.prefix();
  return [/*prefix*/](const DubboBridgeContext& context, const SrcsRequestContext& source) {
    return std::make_unique<AnywayArgumentGetter>(context, source, absl::string_view("x-nsf-"));
  };
}

ResponseGetterCreator
AnywayResponseGetterCreatorFactory::createResponseGetterCreator(const Protobuf::Message&) {
  return []() { return std::make_unique<AnywayResponseGetter>(); };
}

class BffArgumentGetter : public ArgumentGetter {
public:
  BffArgumentGetter(const DubboBridgeContext& context, const SrcsRequestContext& source,
                    absl::optional<absl::string_view> prefix) {

    doc_.Parse(source.getBody().data(), source.getBody().size());

    if (doc_.HasParseError()) {
      throw Envoy::EnvoyException("Dubbo bridge: non Json body for the BFF gateway");
    }
    if (!doc_.IsObject()) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: request body is not Json Object for the BFF gateway");
    }

    if (context.getParametersSize() == 0) {
      return;
    }

    auto iter = doc_.FindMember("paramValues");
    if (iter != doc_.MemberEnd() && iter->value.IsArray()) {
      member_ = std::move(iter->value);
    } else {
      throw Envoy::EnvoyException(
          "Dubbo bridge: no param values or param values is not array for BFF");
    }

    if (prefix.has_value()) {
      source.iterateHeaders([this, prefix](absl::string_view key, absl::string_view value) {
        if (absl::StartsWith(key, prefix.value())) {
          attatchments_.emplace(std::string(key), std::string(value));
        }
        return true;
      });
    }

    auto attach_iter = doc_.FindMember("attachments");
    if (attach_iter != doc_.MemberEnd() && attach_iter->value.IsObject()) {
      for (const auto& pair : attach_iter->value.GetObject()) {
        if (pair.value.IsString()) {
          attatchments_.emplace(std::string(pair.name.GetString(), pair.name.GetStringLength()),
                                std::string(pair.value.GetString(), pair.value.GetStringLength()));
        }
      }
    }
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const override {
    ASSERT(member_.IsArray());
    if (parameter.index_ >= member_.GetArray().Size()) {
      return nullptr;
    }
    return Utility::jsonValueToHessianValue(member_.GetArray()[parameter.index_], parameter.type_,
                                            false);
  }

  OptRef<const FlatStringMap> attachmentsFromGetter() const override {
    return makeOptRef<const FlatStringMap>(attatchments_);
  }

  Json::Document doc_;
  Json::Value member_;
  FlatStringMap attatchments_;
};

class BffResponseGetter : public ResponseGetter {
public:
  BffResponseGetter() = default;

  Response getStringFromResponse(const HessianValuePtr& response, Type type) const override {
    auto result_value = Utility::hessianValueToJsonString(response.get());
    ASSERT(!result_value.empty());
    static const std::string format_string =
        "{{ \"instance\": \"{}\", \"code\": {}, \"systemId\": \"000361\", \"result\": {} }}";

    uint32_t code = 0;
    uint32_t status_code = 200;
    if (type == Type::Exception) {
      code = 610;
      status_code = 610;
    } else if (type == Type::Local) {
      code = 415;
      status_code = 400;
    }
    return {fmt::format(format_string, gatewayInstance(), code, result_value), status_code};
  }
};

ArgumentGetterCreator
BffAnywayArgumentGetterCreatorFactory::createArgumentGetterCreator(const Protobuf::Message&) {
  return [](const DubboBridgeContext& context, const SrcsRequestContext& source) {
    return std::make_unique<BffArgumentGetter>(context, source, absl::string_view("x-nsf-"));
  };
}

ResponseGetterCreator
BffAnywayResponseGetterCreatorFactory::createResponseGetterCreator(const Protobuf::Message&) {
  return []() { return std::make_unique<BffResponseGetter>(); };
}

Common::DubboBridge::DynamicContextCreator
BffDynamicContextCreatorFactory::createDynamicContextCreator(const Protobuf::Message& message) {
  proxy::bridge::dubbo::anyway::v3::BffDynamicContextConfig& bff_dynamic_context_config =
      dynamic_cast<proxy::bridge::dubbo::anyway::v3::BffDynamicContextConfig&>(
          const_cast<Protobuf::Message&>(message));

  return [bff_dynamic_context_config](
             const SrcsRequestContext& source) -> DubboBridgeContextSharedPtr {
    static constexpr absl::string_view ServiceKey = "x-dubbo-service";
    static constexpr absl::string_view VersionKey = "x-dubbo-version";
    static constexpr absl::string_view MethodKey = "x-dubbo-method";
    static constexpr absl::string_view GroupKey = "x-dubbo-group";
    static constexpr absl::string_view ParamsKey = "x-dubbo-params";

    ProtoDubboBridgeContext proto_context;

    // Service.
    auto service = source.getHeader(ServiceKey);
    if (!service.has_value()) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: no interface name provided and cannot covert HTTP to Dubbo");
    }
    proto_context.set_service(std::string(service.value()));

    // Method.
    auto method = source.getHeader(MethodKey);
    if (!method.has_value()) {
      throw Envoy::EnvoyException(
          "Dubbo bridge: no method name provided and cannot covert HTTP to Dubbo");
    }
    proto_context.set_method(std::string(method.value()));

    // Version.
    auto version = source.getHeader(VersionKey);
    proto_context.set_version(version.has_value() ? std::string(version.value())
                                                  : std::string("0.0.0"));

    // Service group.
    auto group = source.getHeader(GroupKey);
    if (group.has_value()) {
      auto attach = proto_context.mutable_attachments()->Add();
      attach->set_name("group");
      attach->set_static_(std::string(group.value()));
    }

    // Params.
    auto params = source.getHeader(ParamsKey);
    if (params.has_value()) {
      absl::string_view params_string = Envoy::StringUtil::trim(params.value());
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), trace,
                          "Dubbo bridge: params type string: {}", params_string);
      if (!params_string.empty()) {
        // a:b;c:d;e:f -> [ {a, b}, {c, d}, {e, f} ]
        // No extra space.
        for (absl::string_view pair : absl::StrSplit(params_string, ';')) {
          std::pair<absl::string_view, absl::string_view> name_type =
              absl::StrSplit(pair, absl::MaxSplits(':', 1));
          if (name_type.second.empty()) {
            throw Envoy::EnvoyException("Dubbo bridge: empty parameter type is not allowed");
          }
          auto parameter = proto_context.add_parameters();
          parameter->set_name(std::string(name_type.first));
          parameter->set_type(std::string(name_type.second));
        }
      }
    } else {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), trace,
                          "No params type provided and may be current method does not need it");
    }

    *proto_context.mutable_attachments() = bff_dynamic_context_config.attachments();
    return std::make_shared<DubboBridgeContext>(proto_context);
  };
}

REGISTER_FACTORY(AnywayArgumentGetterCreatorFactory, ArgumentGetterCreatorFactory);
REGISTER_FACTORY(AnywayResponseGetterCreatorFactory, ResponseGetterCreatorFactory);

REGISTER_FACTORY(BffAnywayArgumentGetterCreatorFactory, ArgumentGetterCreatorFactory);
REGISTER_FACTORY(BffAnywayResponseGetterCreatorFactory, ResponseGetterCreatorFactory);

REGISTER_FACTORY(BffDynamicContextCreatorFactory, DynamicContextCreatorFactory);

} // namespace Anyway
} // namespace Dubbo
} // namespace Bridge
} // namespace Proxy
} // namespace Envoy
