#include "source/common/dubbo/utility.h"

#include "envoy/common/exception.h"

#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace DubboBridge {

namespace {

constexpr absl::string_view BoxedBooleanType = "java.lang.Boolean";
constexpr absl::string_view BooleanType = "boolean";
constexpr absl::string_view BoxedFloatType = "java.lang.Float";
constexpr absl::string_view BoxedDoubleType = "java.lang.Double";
constexpr absl::string_view BoxedByteType = "java.lang.Byte";
constexpr absl::string_view BoxedShortType = "java.lang.Short";
constexpr absl::string_view BoxedIntegerType = "java.lang.Integer";
constexpr absl::string_view BoxedLongType = "java.lang.Long";
constexpr absl::string_view FloatType = "float";
constexpr absl::string_view DoubleType = "double";
constexpr absl::string_view ByteType = "byte";
constexpr absl::string_view ShortType = "short";
constexpr absl::string_view IntegerType = "int";
constexpr absl::string_view LongType = "long";
constexpr absl::string_view StringType = "java.lang.String";
constexpr absl::string_view BoxedCharType = "java.lang.Character";
constexpr absl::string_view CharType = "char";
constexpr absl::string_view EnumType = "java.lang.Enum";
constexpr absl::string_view DayOfWeekType = "java.time.DayOfWeek";
constexpr absl::string_view DateType = "java.util.Date";
constexpr absl::string_view BigDecimalType = "java.math.BigDecimal";
constexpr absl::string_view BigIntegerType = "java.math.BigInteger";
constexpr absl::string_view VoidType = "java.lang.Void";

constexpr absl::string_view UtilListType = "java.util.List";
constexpr absl::string_view UtilArrayListType = "java.util.ArrayList";
constexpr absl::string_view UtilLinkedListType = "java.util.LinkedList";
constexpr absl::string_view FastJsonArrayType = "com.alibaba.fastjson.JSONArray";

constexpr absl::string_view UtilSetType = "java.util.Set";
constexpr absl::string_view UtilHashSetType = "java.util.HashSet";
constexpr absl::string_view UtilTreeSetType = "java.util.TreeSet";
constexpr absl::string_view UtilLinkedHashSetType = "java.util.LinkedHashSet";

constexpr absl::string_view UtilMapType = "java.util.Map";
constexpr absl::string_view UtilHashMapType = "java.util.HashMap";
constexpr absl::string_view UtilTreeMapType = "java.util.TreeMap";
constexpr absl::string_view UtilLinkedHashMapType = "java.util.LinkedHashMap";

constexpr absl::string_view SqlDateType = "java.sql.Date";
constexpr absl::string_view SqlTimeType = "java.sql.Time";
constexpr absl::string_view SqlTimestampType = "java.sql.Timestamp";

constexpr absl::string_view AtomicIntegerType = "java.util.concurrent.atomic.AtomicInteger";
constexpr absl::string_view AtomicLongType = "java.util.concurrent.atomic.AtomicLong";

using JsonStringWriter = Json::Writer<Json::StringBuffer>;

std::string debugStringOfJsonValue(const JsonValue& value) {
  Json::StringBuffer buffer;
  JsonStringWriter writer(buffer);
  value.Accept(writer);
  return std::string{buffer.GetString(), buffer.GetSize()};
}

HessianValuePtr jsonValueToHessianValueInternal(const JsonValue& value, bool ignore_null_map_pair,
                                                uint32_t depth) {
  if (depth >= 50) {
    return std::make_unique<Hessian2::NullObject>();
  }

  switch (value.GetType()) {
  case Json::Type::kNullType:
    return std::make_unique<Hessian2::NullObject>();
  case Json::Type::kFalseType:
    return std::make_unique<Hessian2::BooleanObject>(false);
  case Json::Type::kTrueType:
    return std::make_unique<Hessian2::BooleanObject>(true);
  case Json::Type::kNumberType:
    if (value.IsInt())
      return std::make_unique<Hessian2::IntegerObject>(value.GetInt());
    if (value.IsInt64())
      return std::make_unique<Hessian2::LongObject>(value.GetInt64());
    return std::make_unique<Hessian2::DoubleObject>(value.GetDouble());
  case Json::Type::kStringType:
    return std::make_unique<Hessian2::StringObject>(
        absl::string_view(value.GetString(), value.GetStringLength()));
  case Json::Type::kArrayType: {
    auto list = std::make_unique<Hessian2::UntypedListObject>();
    auto list_data = list->toMutableUntypedList();

    for (auto item = value.Begin(); item != value.End(); item++) {
      auto item_ptr = jsonValueToHessianValueInternal(*item, ignore_null_map_pair, depth + 1);
      list_data->push_back(std::move(item_ptr));
    }
    return list;
  }
  case Json::Type::kObjectType: {
    auto dict = std::make_unique<Hessian2::UntypedMapObject>();
    auto dict_data = dict->toMutableUntypedMap();
    dict_data->reserve(value.MemberCount());

    for (const auto& pair : value.GetObject()) {
      if (ignore_null_map_pair && pair.value.IsNull()) {
        continue;
      }

      auto key = std::make_unique<Hessian2::StringObject>(
          absl::string_view{pair.name.GetString(), pair.name.GetStringLength()});
      auto val = jsonValueToHessianValueInternal(pair.value, ignore_null_map_pair, depth + 1);
      dict_data->emplace(std::move(key), std::move(val));
    }
    return dict;
  }
  default:
    return std::make_unique<Hessian2::NullObject>();
  }
}

using JsonConverter =
    std::function<HessianValuePtr(const JsonValue& value, bool ignore_null_map_pair)>;
using JsonTypeChecker = std::function<bool(const JsonValue& value)>;

/**
 * Create converter for the type that may can load from string. For example, if the upstream
 * type is integer, and the Json value can be 123 or "123".
 * @param type_checker used to check if the Json value type match the expected parameter type.
 */
JsonConverter loadableJsonConverter(JsonTypeChecker type_checker) {
  return [type_checker](const JsonValue& value, bool ignore_null_map_pair) -> HessianValuePtr {
    if (type_checker(value)) {
      return jsonValueToHessianValueInternal(value, ignore_null_map_pair, 0);
    }

    if (!value.IsString()) {
      throw Envoy::EnvoyException(fmt::format(
          "Dubbo bridge: value '{}' is invalid for current type", debugStringOfJsonValue(value)));
    }

    Json::Document inner_value;
    inner_value.Parse(value.GetString(), value.GetStringLength());

    if (inner_value.HasParseError()) {
      throw Envoy::EnvoyException(fmt::format(
          "Dubbo bridge: value '{}' is invalid for current type", debugStringOfJsonValue(value)));
    }

    if (type_checker(inner_value)) {
      return jsonValueToHessianValueInternal(inner_value, ignore_null_map_pair, 0);
    }

    throw Envoy::EnvoyException(fmt::format("Dubbo bridge: value '{}' is invalid for current type",
                                            debugStringOfJsonValue(value)));
  };
}

JsonConverter strictJsonConverter(JsonTypeChecker type_checker) {
  return [type_checker](const JsonValue& value, bool ignore_null_map_pair) -> HessianValuePtr {
    if (type_checker(value)) {
      return jsonValueToHessianValueInternal(value, ignore_null_map_pair, 0);
    }
    throw Envoy::EnvoyException(fmt::format("Dubbo bridge: value '{}' is invalid for current type",
                                            debugStringOfJsonValue(value)));
  };
}

JsonConverter stringJsonConverter() {
  return [](const JsonValue& value, bool ignore_null_map_pair) -> HessianValuePtr {
    if (value.IsNull() || value.IsString()) {
      return jsonValueToHessianValueInternal(value, ignore_null_map_pair, 0);
    }
    return std::make_unique<Hessian2::StringObject>(debugStringOfJsonValue(value));
  };
}

#define CHECKER(METHOD) [](const JsonValue& v) -> bool { return v.Is##METHOD(); }
#define NULL_OR_CHECKER(METHOD)                                                                    \
  [](const JsonValue& v) -> bool { return v.IsNull() || v.Is##METHOD(); }

#define NOT_EMPTY_STRING                                                                           \
  [](const JsonValue& v) -> bool { return v.IsString() && v.GetStringLength(); }

#define NOT_EMPTY_STRING_OR_CHECKER(METHOD)                                                        \
  [](const JsonValue& v) -> bool {                                                                 \
    return (v.IsString() && v.GetStringLength() != 0) || v.Is##METHOD();                           \
  }

#define NOT_EMPTY_STRING_OR_NULL_OR_CHECKER(METHOD)                                                \
  [](const JsonValue& v) -> bool {                                                                 \
    return (v.IsString() && v.GetStringLength() != 0) || v.IsNull() || v.Is##METHOD();             \
  }

absl::flat_hash_map<absl::string_view, JsonConverter> TypedJsonConverters{
    // Bool.
    {BoxedBooleanType, loadableJsonConverter(NULL_OR_CHECKER(Bool))},

    {BooleanType, loadableJsonConverter(CHECKER(Bool))},

    // Number.
    {BoxedFloatType, loadableJsonConverter(NULL_OR_CHECKER(Number))},

    {FloatType, loadableJsonConverter(CHECKER(Number))},

    {BoxedDoubleType, loadableJsonConverter(NULL_OR_CHECKER(Number))},

    {DoubleType, loadableJsonConverter(CHECKER(Number))},

    {BoxedByteType, loadableJsonConverter(NULL_OR_CHECKER(Int))},

    {ByteType, loadableJsonConverter(CHECKER(Int))},

    {BoxedShortType, loadableJsonConverter(NULL_OR_CHECKER(Int))},

    {ShortType, loadableJsonConverter(CHECKER(Int))},

    {BoxedIntegerType, loadableJsonConverter(NULL_OR_CHECKER(Int64))},

    {IntegerType, loadableJsonConverter(CHECKER(Int64))},

    {BoxedLongType, loadableJsonConverter(NULL_OR_CHECKER(Int64))},

    {LongType, loadableJsonConverter(CHECKER(Int64))},

    // String.
    {StringType, stringJsonConverter()},

    // Char.
    {BoxedCharType, strictJsonConverter(NOT_EMPTY_STRING_OR_CHECKER(Null))},

    {CharType, strictJsonConverter(NOT_EMPTY_STRING)},

    // Others.
    {EnumType, strictJsonConverter(NOT_EMPTY_STRING_OR_CHECKER(Null))},

    {DayOfWeekType, strictJsonConverter(NOT_EMPTY_STRING_OR_CHECKER(Null))},

    {DateType, strictJsonConverter(NOT_EMPTY_STRING_OR_NULL_OR_CHECKER(Number))},

    {BigDecimalType, strictJsonConverter(NOT_EMPTY_STRING_OR_NULL_OR_CHECKER(Number))},

    {BigIntegerType, strictJsonConverter(NOT_EMPTY_STRING_OR_NULL_OR_CHECKER(Number))},

    {VoidType, strictJsonConverter(CHECKER(Null))},

    {UtilListType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilArrayListType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilLinkedListType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {FastJsonArrayType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilSetType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilHashSetType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilTreeSetType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilLinkedHashSetType, loadableJsonConverter(NULL_OR_CHECKER(Array))},

    {UtilMapType, loadableJsonConverter(NULL_OR_CHECKER(Object))},

    {UtilHashMapType, loadableJsonConverter(NULL_OR_CHECKER(Object))},

    {UtilTreeMapType, loadableJsonConverter(NULL_OR_CHECKER(Object))},

    {UtilLinkedHashMapType, loadableJsonConverter(NULL_OR_CHECKER(Object))},
};

constexpr uint64_t LONG_LONG_MASK = 0xffffffff;

uint64_t divideByWord(std::vector<uint64_t>& values, size_t& from) {
  constexpr uint64_t divisor = 1000000000;

  uint64_t remainder = 0;
  bool previous_non_zero = false;

  for (size_t i = from; i < values.size(); i++) {
    ASSERT(values[i] <= LONG_LONG_MASK);

    uint64_t tmp_dividend = ((remainder << 32) | values[i]);
    uint64_t tmp_result = tmp_dividend / divisor;
    uint64_t tmp_remainder = tmp_dividend - tmp_result * divisor;

    if (tmp_result == 0) {
      if (!previous_non_zero) {
        from = i + 1;
      }
    } else {
      previous_non_zero = true;
    }

    values[i] = tmp_result;
    remainder = tmp_remainder;
  }

  return remainder;
};

void checkClassOnlyHasOneValue(const Hessian2::Object::ClassInstance& instance) {
  ASSERT(instance.def_ != nullptr);
  const auto& field_names = instance.def_->field_names_;

  if (field_names.size() != 1 || field_names[0] != "value") {
    throw Envoy::EnvoyException(
        fmt::format("Unknown type definition for: {}", instance.def_->type_));
  }

  if (instance.data_.size() != 1) {
    throw Envoy::EnvoyException(
        fmt::format("Expected fields size: 1 but actual fields size: {} for {}",
                    instance.data_.size(), instance.def_->type_));
  }
}

template <Hessian2::Object::Type T>
void handleValueNumber(const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
  checkClassOnlyHasOneValue(instance);
  // TODO(wbpcode): make this if constexpr.
  if (T == Hessian2::Object::Type::Integer) {
    const auto result = instance.data_[0]->toInteger();
    if (result.has_value()) {
      writer.Int64(result.value());
      return;
    }
  }
  if (T == Hessian2::Object::Type::Long) {
    const auto result = instance.data_[0]->toLong();
    if (result.has_value()) {
      writer.Int64(result.value());
      return;
    }
  }
  if (T == Hessian2::Object::Type::Double) {
    const auto result = instance.data_[0]->toDouble();
    if (result.has_value()) {
      writer.Double(result.value());
      return;
    }
  }
  if (T == Hessian2::Object::Type::Date) {
    const auto result = instance.data_[0]->toDate();
    if (result.has_value()) {
      writer.Int64(result.value().count());
      return;
    }
  }
  writer.Null();
}

using HessianHistory = absl::flat_hash_set<const HessianValue*>;

void hessianValueToJsonStringInternal(const HessianValue* object, JsonStringWriter& writer,
                                      HessianHistory& history);

using ClassConverter =
    std::function<void(const Hessian2::Object::ClassInstance&, JsonStringWriter&)>;

static absl::flat_hash_map<absl::string_view, ClassConverter> TypedClassConverters{
    {BigDecimalType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       checkClassOnlyHasOneValue(instance);
       const auto& hessian2_value = instance.data_[0];

       ASSERT(hessian2_value != nullptr);

       if (hessian2_value->type() != Hessian2::Object::Type::String) {
         writer.Null();
         return;
       }

       const auto& string_value = *(hessian2_value->toString().value());

       writer.RawValue(string_value.data(), string_value.size(), rapidjson::Type::kNumberType);
     }},
    {BigIntegerType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       ASSERT(instance.def_ != nullptr);

       const auto& field_names = instance.def_->field_names_;

       size_t mag_index = 0;
       size_t sig_index = 0;
       bool has_mag = false;
       bool has_sig = false;

       // Get index of `mag` and `signum` fields.
       for (size_t i = 0; i < field_names.size(); i++) {
         if (field_names[i] == "mag") {
           mag_index = i;
           has_mag = true;
         } else if (field_names[i] == "signum") {
           sig_index = i;
           has_sig = true;
         }
       }

       // Check if has `mag` and `signum` fields.
       if (!has_mag || !has_sig) {
         writer.Null();
         return;
       }

       // Check if the indexs are valid.
       if (mag_index >= instance.data_.size() || sig_index >= instance.data_.size()) {
         writer.Null();
         return;
       }

       // Check if `signum` are valid.
       auto sig = instance.data_[sig_index]->toInteger();
       if (!sig.has_value()) {
         writer.Null();
         return;
       }

       // Handle signum. 0 for zero, 1 for positive number, -1 for negative number.
       std::string final_integer_str;
       final_integer_str.reserve(64);
       if (sig.value() == 0) {
         writer.Int(0);
         return;
       } else if (sig.value() < 0) {
         final_integer_str.push_back('-');
       }

       auto values_object = instance.data_[mag_index]->toTypedList();
       if (!values_object.has_value()) {
         writer.Null();
         return;
       }

       std::vector<uint64_t> values;
       values.reserve(values_object.value()->values_.size());
       for (auto& int_value_object : values_object.value()->values_) {
         auto int_value = int_value_object->toInteger();
         if (!int_value.has_value()) {
           writer.Null();
           return;
         }
         values.push_back(int_value.value() & LONG_LONG_MASK);
       }
       ASSERT(values.size() > 0);

       std::vector<uint64_t> final_values;
       final_values.reserve(values.size());

       size_t from = 0;
       while (from < values.size()) {
         final_values.push_back(divideByWord(values, from));
       }
       ASSERT(final_values.size() > 0);
       for (auto iter = final_values.rbegin(); iter < final_values.rend(); iter++) {
         auto tmp = std::to_string(*iter);
         // divisor used to get group is 1000,000,000, so the size of
         // group is less then 10.
         ASSERT(tmp.size() <= 9);

         // Fill leanding zero.
         if (iter != final_values.rbegin()) {
           tmp = fmt::format("{:0>9}", tmp);
         }
         final_integer_str.append(tmp);
       }

       int64_t int64_value = 0;
       if (absl::SimpleAtoi(final_integer_str, &int64_value)) {
         writer.Int64(int64_value);
         return;
       }
       writer.String(final_integer_str.data(), final_integer_str.size());
     }},
    {SqlDateType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       handleValueNumber<Hessian2::Object::Type::Date>(instance, writer);
     }},
    {SqlTimeType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       handleValueNumber<Hessian2::Object::Type::Date>(instance, writer);
     }},
    {SqlTimestampType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       handleValueNumber<Hessian2::Object::Type::Date>(instance, writer);
     }},
    {AtomicIntegerType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       handleValueNumber<Hessian2::Object::Type::Integer>(instance, writer);
     }},
    {AtomicLongType,
     [](const Hessian2::Object::ClassInstance& instance, JsonStringWriter& writer) {
       handleValueNumber<Hessian2::Object::Type::Long>(instance, writer);
     }},
};

void defaultClassConverter(const Hessian2::Object::ClassInstance& instance,
                           JsonStringWriter& writer, HessianHistory& history) {

  ASSERT(instance.def_ != nullptr);
  writer.StartObject();

  const auto& field_names = instance.def_->field_names_;
  auto valid_field_size = std::min(instance.data_.size(), field_names.size());

  for (size_t i = 0; i < valid_field_size; i++) {
    writer.String(field_names[i].data(), field_names[i].size());
    hessianValueToJsonStringInternal(instance.data_[i].get(), writer, history);
  }
  writer.EndObject();
}

void covertClassToJsonString(const Hessian2::Object::ClassInstance& instance,
                             JsonStringWriter& writer, HessianHistory& history) {
  if (instance.def_ == nullptr) {
    writer.Null();
    return;
  }

  auto iter = TypedClassConverters.find(instance.def_->type_);
  if (iter == TypedClassConverters.end()) {
    defaultClassConverter(instance, writer, history);
    return;
  }
  // No complex type for registered converter, so ignore the history check.
  iter->second(instance, writer);
}

void convertUntypedMapToJsonString(const Hessian2::Object::UntypedMap& untyped_map,
                                   JsonStringWriter& writer, HessianHistory& history) {
  writer.StartObject();
  for (const auto& pair : untyped_map) {

    if (pair.first->type() == Hessian2::Object::Type::String) {
      const auto data = pair.first->toString().value();
      writer.String(data->data(), data->size());
    } else {
      // Handle the key independently.
      const std::string data = Utility::hessianValueToJsonString(pair.first.get());
      writer.String(data.data(), data.size());
    }

    hessianValueToJsonStringInternal(pair.second.get(), writer, history);
  }
  writer.EndObject();
}

void hessianValueToJsonStringInternal(const HessianValue* object, JsonStringWriter& writer,
                                      HessianHistory& history) {
  if (object == nullptr) {
    writer.Null();
    return;
  }

  switch (object->type()) {
  case Hessian2::Object::Type::String: {
    const auto& data = *object->toString().value();
    writer.String(data.data(), data.size());
    return;
  }
  case Hessian2::Object::Type::Long: {
    writer.Int64(object->toLong().value());
    return;
  }
  case Hessian2::Object::Type::Integer: {
    writer.Int(object->toInteger().value());
    return;
  }
  case Hessian2::Object::Type::Boolean: {
    writer.Bool(object->toBoolean().value());
    return;
  }
  case Hessian2::Object::Type::Double: {
    writer.Double(object->toDouble().value());
    return;
  }
  case Hessian2::Object::Type::Date: {
    const auto data = object->toDate().value();
    writer.Int64(data.count());
    return;
  }
  case Hessian2::Object::Type::Binary: {
    const auto& data = *object->toBinary().value();
    if (data.size() == 0) {
      writer.String("");
    } else {
      writer.String((char*)&data[0], data.size());
    }
    return;
  }
  case Hessian2::Object::Type::Null:
    writer.Null();
    return;
  case Hessian2::Object::Type::TypedList:
  case Hessian2::Object::Type::UntypedList: {
    const auto& data = object->type() == Hessian2::Object::Type::TypedList
                           ? object->toTypedList().value()->values_
                           : *object->toUntypedList().value();
    writer.StartArray();
    history.insert(object);
    for (size_t i = 0; i < data.size(); i++) {
      hessianValueToJsonStringInternal(data[i].get(), writer, history);
    }
    history.erase(object);
    writer.EndArray();
    return;
  }
  case Hessian2::Object::Type::TypedMap:
  case Hessian2::Object::Type::UntypedMap: {
    const auto& data = object->type() == Hessian2::Object::Type::TypedMap
                           ? object->toTypedMap().value()->field_name_and_value_
                           : *object->toUntypedMap().value();
    history.insert(object);
    convertUntypedMapToJsonString(data, writer, history);
    history.erase(object);
    return;
  }
  case Hessian2::Object::Type::Class: {
    const auto& data = *object->toClassInstance().value();
    history.insert(object);
    covertClassToJsonString(data, writer, history);
    history.erase(object);
    return;
  }
  case Hessian2::Object::Type::Ref: {
    const auto data = object->toRefDest().value();
    if (history.contains(data)) {
      writer.Null();
    } else {
      history.insert(object);
      hessianValueToJsonStringInternal(data, writer, history);
      history.erase(object);
    }
    return;
  }
  default:
    writer.Null();
    return;
  }
}

} // namespace

void BufferWriter::rawWrite(const void* data, uint64_t size) { buffer_.add(data, size); }

void BufferWriter::rawWrite(absl::string_view data) { buffer_.add(data); }

void BufferReader::rawReadNBytes(void* data, size_t len, size_t offset_value) {
  buffer_.copyOut(offset() + offset_value, len, data);
}

HessianValuePtr Utility::jsonValueToHessianValue(const JsonValue& value, absl::string_view type,
                                                 bool ignore_null_map_pair) {
  auto iter = TypedJsonConverters.find(type);
  if (iter != TypedJsonConverters.end()) {
    return iter->second(value, ignore_null_map_pair);
  }
  return jsonValueToHessianValueInternal(value, ignore_null_map_pair, 0);
}

std::string Utility::hessianValueToJsonString(const HessianValue* value) {
  Json::StringBuffer buffer;
  JsonStringWriter writer(buffer);

  absl::flat_hash_set<const HessianValue*> history;
  hessianValueToJsonStringInternal(value, writer, history);

  return std::string(buffer.GetString(), buffer.GetSize());
}

namespace {

constexpr absl::string_view DUBBO_VERSION_ENCODE = "2.0.2";
constexpr uint16_t DUBBO_MAGIC_NUMBER = 0xdabb;
constexpr uint8_t DUBBO_FLAG_REQ_HESSIAN2 = 0xc2;
// Generic dubbo call has three arguments. The first argument is actual method name. The second
// argument is parameter types of actual method. The third argument is parameter list of actual
// method.
constexpr absl::string_view DUBBO_GENERIC_PARAM_TYPES =
    "Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/Object;";
// Generic dubbo call use `$invoke` method as common entrypoint and proxy.
constexpr absl::string_view DUBBO_GENERIC_METHOD = "$invoke";
constexpr uint64_t DUBBO_HEADER_SIZE = 16;

int64_t dubboRequestId() {
  static std::atomic<int64_t> GLOBAL_REQUEST_ID{};
  return GLOBAL_REQUEST_ID++;
}

void populateSingleGenericType(absl::Span<std::string> path, absl::string_view type,
                               Hessian2::Object& hessian2_ob) {
  static constexpr absl::string_view ALL = "*";
  static constexpr absl::string_view CLASS_KEY = "class";

  auto hessian2_type = hessian2_ob.type();

  if (path.empty()) {
    if (hessian2_type == Hessian2::Object::Type::UntypedMap) {
      hessian2_ob.toMutableUntypedMap()->emplace(
          std::make_unique<Hessian2::StringObject>(CLASS_KEY),
          std::make_unique<Hessian2::StringObject>(type));
    }
    return;
  }

  const auto& first_sub_path = path[0];

  if (first_sub_path == ALL) {
    if (hessian2_type == Hessian2::Object::Type::UntypedList) {
      for (auto& sub_ob : *hessian2_ob.toMutableUntypedList()) {
        populateSingleGenericType(path.subspan(1), type, *sub_ob);
      }
    }
    if (hessian2_type == Hessian2::Object::Type::UntypedMap) {
      for (auto& sub_ob : *hessian2_ob.toMutableUntypedMap()) {
        populateSingleGenericType(path.subspan(1), type, *sub_ob.second);
      }
    }
    // `*` should only be used for map or list.
    return;
  }

  // First sub path is specific string key and object type is not map.
  if (hessian2_type != Hessian2::Object::Type::UntypedMap) {
    return;
  }

  auto hessian2_map = hessian2_ob.toMutableUntypedMap();
  auto iter = hessian2_map->find(std::make_unique<Hessian2::StringObject>(first_sub_path));
  if (iter == hessian2_map->end()) {
    return;
  }
  populateSingleGenericType(path.subspan(1), type, *(iter->second));
}

void populateGenericTypes(const GenericTypes& generic_types, Hessian2::Object& hessian2_ob) {

  for (const auto& generic_type : generic_types) {
    populateSingleGenericType(
        absl::Span<std::string>(const_cast<std::string*>(generic_type.splited_path.data()),
                                generic_type.splited_path.size()),
        generic_type.type, hessian2_ob);
  }
}

// // Help function for header context.
// GenericTypesSharedPtr parseGenericTypesFromStringView(absl::string_view generic) {
//   auto result = std::make_shared<GenericTypes>();
//   for (auto path_type : absl::StrSplit(generic, ',', absl::SkipEmpty())) {
//     std::pair<absl::string_view, absl::string_view> path_type_pair =
//         absl::StrSplit(path_type, absl::MaxSplits(':', 1));

//     if (path_type_pair.first.empty() || path_type_pair.second.empty()) {
//       throw Envoy::EnvoyException(
//           fmt::format("Dubbo bridge: error format generic config: {}", generic));
//     }

//     result->push_back(GenericType());

//     result->back().path = std::string(path_type_pair.first);
//     result->back().type = std::string(path_type_pair.second);
//     result->back().splited_path = absl::StrSplit(result->back().path, '.', absl::SkipEmpty());
//   }
//   return result;
// }

} // namespace

Parameter::Parameter(size_t index, const ProtoDubboBridgeContext::Parameter& config,
                     bool ignore_null_map_pair)
    : index_(index), type_(config.type()), name_(config.name()), required_(config.required()) {

  // Default value is only worked when the `required` is false.
  if (!config.default_().empty()) {
    rapidjson::Document default_document;
    default_document.Parse(config.default_().c_str(), config.default_().size());
    if (default_document.HasParseError()) {
      throw Envoy::EnvoyException(
          fmt::format("Dubbo bridge: default value: {}/{} of {} is invalid Json", name_, type_,
                      config.default_()));
    }
    default_ = Utility::jsonValueToHessianValue(default_document, type_, ignore_null_map_pair);
  } else {
    default_ = std::make_shared<Hessian2::NullObject>();
  }

  generic_ = std::make_unique<GenericTypes>();

  for (const auto& path_type : config.generic()) {
    generic_->push_back(GenericType());
    generic_->back().path = std::string(path_type.path());
    generic_->back().type = std::string(path_type.type());
    generic_->back().splited_path = absl::StrSplit(generic_->back().path, '.', absl::SkipEmpty());
  }
}

SrcsRequestContext::SrcsRequestContext(const Envoy::Http::RequestHeaderMap& headers,
                                       Envoy::Buffer::Instance& buffer)
    : headers_(headers),
      linearized_body_(static_cast<const char*>(buffer.linearize(buffer.length())),
                       buffer.length()) {}

DubboBridgeContext::DubboBridgeContext(const ProtoDubboBridgeContext& context)
    : service_(context.service()), version_(context.version()), method_(context.method()),
      ignore_null_map_pair_(context.ignore_null_map_pair()) {

  if (!context.group().empty()) {
    static_attachs_.insert({"group", context.group()});
  }

  for (const auto& attach : context.attachments()) {
    switch (attach.value_source_case()) {
    case ProtoDubboBridgeContext::Attachment::kStatic:
      static_attachs_.insert({attach.name(), attach.static_()});
      break;
    case ProtoDubboBridgeContext::Attachment::kHeader:
      header_attachs_.insert({attach.name(), Envoy::Http::LowerCaseString(attach.header()).get()});
      break;
    case ProtoDubboBridgeContext::Attachment::kCookie:
      cookie_attachs_.insert({attach.name(), attach.cookie()});
    default:
      break;
    }
  }

  for (int i = 0; i < context.parameters_size(); i++) {
    const auto& param = context.parameters(i);
    lost_parameter_name_ = lost_parameter_name_ || param.name().empty();
    params_.push_back(std::make_unique<Parameter>(i, param, ignore_null_map_pair_));
  }

  if (context.source() == ProtoDubboBridgeContext::HTTP_QUERY) {
    argument_source_ = Source::Query;
  } else {
    argument_source_ = Source::Body;
  }
}

void DubboBridgeContext::writeDubboRequest(Envoy::Buffer::OwnedImpl& buffer,
                                           const SrcsRequestContext& context,
                                           const ArgumentGetter& getter) const {
  /**
   * Encode a Dubbo Request (DubboHeader + Body)
   *
   ***********   Dubbo Header  **********
   ** typedef struct
   ** {
   **     u_char magic_0;
   **     u_char magic_1;
   **     u_char type;
   **     u_char status;
   **
   **     uint64_t reqid;
   **
   **     uint32_t payloadlen;
   ** } dubbo_header_t
   **
   *************  Dubbo body  **********
   *
   **/
  buffer.writeBEInt<uint16_t>(DUBBO_MAGIC_NUMBER);
  buffer.writeByte(DUBBO_FLAG_REQ_HESSIAN2);
  buffer.writeByte(static_cast<uint8_t>(0));
  buffer.writeBEInt<uint64_t>(dubboRequestId());

  Envoy::Buffer::OwnedImpl dubbo_body;
  writeRequestBody(dubbo_body, context, getter);

  buffer.writeBEInt<uint32_t>(dubbo_body.length());
  buffer.move(dubbo_body);
}

void DubboBridgeContext::writeRequestBody(Envoy::Buffer::OwnedImpl& buffer,
                                          const SrcsRequestContext& context,
                                          const ArgumentGetter& getter) const {
  Hessian2::WriterPtr writer = std::make_unique<BufferWriter>(buffer);
  Hessian2::Encoder encoder(std::move(writer));

  encoder.encode(DUBBO_VERSION_ENCODE);
  encoder.encode(service_);
  encoder.encode(version_);
  encoder.encode(DUBBO_GENERIC_METHOD);

  // Encode parameter type strings.
  encoder.encode(DUBBO_GENERIC_PARAM_TYPES);

  // Encode actual dubbo request method.
  encoder.encode(method_);

  // int32_t is used for the dubbo request parameter length.
  int32_t params_length = params_.size();

  // Encode actual dubbo request method parameter type strings.
  writeListBegin(encoder, buffer, params_length);
  for (const auto& parameter : params_) {
    encoder.encode(parameter->type_);
  }

  // Encode actual dubbo request method parameters.
  writeListBegin(encoder, buffer, params_length);
  for (const auto& parameter : params_) {
    auto object = getter.getArgumentByParameter(*parameter);

    if (object != nullptr) {
      if (parameter->generic_ != nullptr) {
        populateGenericTypes(*parameter->generic_, *object);
      }
    } else {
      if (parameter->required_) {
        throw Envoy::EnvoyException(
            fmt::format("Dubbo bridge: {}/{}/{} is required but not provided", parameter->index_,
                        parameter->name_, parameter->type_));
      } else {
        object = parameter->default_;
      }
    }
    encoder.encode(*object);
  }

  // Encode attachements.
  buffer.writeByte('H');
  writeStaticAttachments(encoder);
  writeHeaderAttachments(encoder, context);
  writeCookieAttachments(encoder, context);

  auto opt_attachments = getter.attachmentsFromGetter();
  if (opt_attachments.has_value()) {
    for (const auto& attachement : opt_attachments.value().get()) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), trace,
                          "Try write static: {}/{} to dubbo attachment", attachement.first,
                          attachement.second);
      encoder.encode(attachement.first);
      encoder.encode(attachement.second);
    }
  }

  buffer.writeByte('Z');
}

using Envoy::Extensions::NetworkFilters::DubboProxy::MessageType;
using Envoy::Extensions::NetworkFilters::DubboProxy::RpcResponseType;

DubboResponse::Status DubboResponse::decodeDubboFromBuffer(Envoy::Buffer::Instance& buffer,
                                                           DubboResponse& response) {

  if (!response.context_) {
    response.context_ = response.impl_.decodeHeader(buffer, response.metadata_).first;
  }

  // Waiting for the whole request or response.
  if (!response.context_ || buffer.length() < response.context_->bodySize() + DUBBO_HEADER_SIZE) {
    return Status::WAITING;
  }

  // Handle heartbeat directly.
  while (response.metadata_->messageType() == MessageType::HeartbeatRequest ||
         response.metadata_->messageType() == MessageType::HeartbeatResponse) {

    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), info,
                        "Heartbeat package from dubbo server");

    // Drain header and body of heartbeat request/response.
    buffer.drain(response.context_->bodySize() + DUBBO_HEADER_SIZE);

    // Reset heartbeat request/response.
    response.reset();
    ASSERT(response.context_ == nullptr);

    // Try re-decode response from buffer.
    response.context_ = response.impl_.decodeHeader(buffer, response.metadata_).first;
    // Waiting for the whole request or response.
    if (!response.context_ || buffer.length() < response.context_->bodySize() + DUBBO_HEADER_SIZE) {
      return Status::WAITING;
    }
  }

  ASSERT(response.metadata_->messageType() != MessageType::HeartbeatRequest &&
         response.metadata_->messageType() != MessageType::HeartbeatResponse);
  ASSERT(buffer.length() >= response.context_->bodySize() + DUBBO_HEADER_SIZE);

  switch (response.metadata_->messageType()) {
  case MessageType::Response:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), trace,
                        "Response package from dubbo server");
    break;
  case MessageType::Request:
  case MessageType::Oneway:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), error,
                        "Request package from dubbo server");
    ASSERT(false);
    FALLTHRU;
  case MessageType::Exception:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), error,
                        "Exception package from dubbo server");
    FALLTHRU;
  default:
    throw Envoy::EnvoyException(fmt::format("Expected response message but get {}",
                                            size_t(response.metadata_->messageType())));
  }

  // Drain header of response.
  buffer.drain(16);

  Hessian2::Decoder decoder(std::make_unique<BufferReader>(buffer));

  if (response.metadata_->responseStatus() !=
      Envoy::Extensions::NetworkFilters::DubboProxy::ResponseStatus::Ok) {
    response.value_ = decoder.decode<Hessian2::Object>();

    if (response.context_->bodySize() < decoder.offset()) {
      throw Envoy::EnvoyException(fmt::format("RpcResult size({}) large than body size({})",
                                              decoder.offset(), response.context_->bodySize()));
    }

    response.is_exception_ = true;
    response.close_connection_ = true;
    return Status::OK;
  }

  auto type_data = decoder.decode<int32_t>();

  if (!type_data.get()) {
    throw Envoy::EnvoyException("No response type get from dubbo response and do nothing");
  }

  RpcResponseType type = static_cast<RpcResponseType>(*type_data);

  bool has_value = false;
  bool has_exception = false;
  switch (type) {
  case RpcResponseType::ResponseWithException:
  case RpcResponseType::ResponseWithExceptionWithAttachments:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), error,
                        "Exception package from dubbo server");
    has_value = true;
    has_exception = true;
    break;
  case RpcResponseType::ResponseWithValue:
  case RpcResponseType::ResponseValueWithAttachments:
    has_value = true;
    break;
  case RpcResponseType::ResponseWithNullValue:
  case RpcResponseType::ResponseNullValueWithAttachments:
    has_value = false;
    break;
  default:
    throw Envoy::EnvoyException(
        fmt::format("not supported return type {}", static_cast<uint8_t>(type)));
  }

  if (has_value) {
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), debug,
                        "Decode response binary to hessian object");
    response.value_ = decoder.decode<Hessian2::Object>();
    response.is_exception_ = has_exception;
  }

  if (response.context_->bodySize() < decoder.offset()) {
    throw Envoy::EnvoyException(fmt::format("RpcResult size({}) large than body size({})",
                                            decoder.offset(), response.context_->bodySize()));
  }

  return Status::OK;
}

} // namespace DubboBridge
} // namespace Common
} // namespace Proxy
} // namespace Envoy
