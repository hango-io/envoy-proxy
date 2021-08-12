#include "filters/http/ip_restriction/ip_restriction.h"

#include "envoy/common/exception.h"

#include "common/common/assert.h"
#include "common/http/utility.h"
#include "common/network/address_impl.h"
#include "common/network/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace IpRestriction {

// used to match the IP in the configuration file to ensure that the IP format is correct
const std::regex BlackOrWhiteListConfig::REGEXIPV4 =
    std::regex("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|["
               "01]?\\d\\d?)$");
const std::regex BlackOrWhiteListConfig::REGEXIPV6 =
    std::regex("^([\\da-fA-F]{1,4}:){7}[\\da-fA-F]{1,4}$");
const std::regex BlackOrWhiteListConfig::IPV4WITHCIDR =
    std::regex("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|["
               "01]?\\d\\d?)/(3[01]|[12]\\d|\\d)");
const std::regex BlackOrWhiteListConfig::IPV6WITHCIDR =
    std::regex("^([\\da-fA-F]{1,4}:){7}[\\da-fA-F]{1,4}/(1[012][0-7]|[1-9]\\d|\\d)");

ListGlobalConfig::ListGlobalConfig(
    const proxy::filters::http::iprestriction::v2::ListGlobalConfig& config,
    Server::Configuration::FactoryContext& context, const std::string& stats_prefix)
    : ip_source_header_(config.ip_source_header()),
      stats_(generateStats(stats_prefix, context.scope())), scope_(context.scope()) {}

// Initialize the configuration class using the configuration type defined in the proto file
BlackOrWhiteListConfig::BlackOrWhiteListConfig(
    const proxy::filters::http::iprestriction::v2::BlackOrWhiteList& list) {
  switch (list.type()) {
  case proxy::filters::http::iprestriction::v2::ListType::BLACK:
    isBlackList_ = true;
    break;
  case proxy::filters::http::iprestriction::v2::ListType::WHITE:
    isBlackList_ = false;
    break;
  default:
    ENVOY_LOG(error, "ListType must be BLACK or WHITE.");
    break;
  }

  int size = list.list_size();
  for (int i = 0; i < size; i++) {
    parseIpEntry(list.list(i));
  }
}

BlackOrWhiteListConfig::IpType BlackOrWhiteListConfig::parseIpType(const std::string& entry) {
  if (std::regex_match(entry, BlackOrWhiteListConfig::REGEXIPV4))
    return BlackOrWhiteListConfig::IpType::RAWIPV4;
  else if (std::regex_match(entry, BlackOrWhiteListConfig::IPV4WITHCIDR))
    return BlackOrWhiteListConfig::IpType::CIDRV4;
  else if (std::regex_match(entry, BlackOrWhiteListConfig::REGEXIPV6))
    return BlackOrWhiteListConfig::IpType::RAWIPV6;
  else if (std::regex_match(entry, BlackOrWhiteListConfig::IPV6WITHCIDR))
    return BlackOrWhiteListConfig::IpType::CIDRV6;
  else
    return BlackOrWhiteListConfig::IpType::NONE;
}

void BlackOrWhiteListConfig::parseIpEntry(const std::string& entry) {
  if (entry.empty()) {
    ENVOY_LOG(error, "Empty IP entry.");
    return;
  }
  switch (parseIpType(entry)) {
  case BlackOrWhiteListConfig::IpType::RAWIPV4: {
    Network::Address::Ipv4Instance temp(entry);
    rawIpV4.insert(temp.ip()->ipv4()->address());
    break;
  }
  case BlackOrWhiteListConfig::IpType::RAWIPV6: {
    Network::Address::Ipv6Instance temp(entry);
    rawIpV6.insert(temp.ip()->addressAsString());
    break;
  }
  case BlackOrWhiteListConfig::IpType::CIDRV4: {
    int cutPoint = entry.find_last_of("/");
    auto ipString = entry.substr(0, cutPoint);
    int length = std::stoi(entry.substr(cutPoint + 1));
    auto addressPtr = std::make_shared<Network::Address::Ipv4Instance>(ipString);
    cidrIpV4_.push_back(Network::Address::CidrRange::create(addressPtr, length));
    break;
  }
  case BlackOrWhiteListConfig::IpType::CIDRV6: {
    int cutPoint = entry.find_last_of("/");
    auto ipString = entry.substr(0, cutPoint);
    int length = std::stoi(entry.substr(cutPoint + 1));
    auto addressPtr = std::make_shared<Network::Address::Ipv6Instance>(ipString);
    cidrIpV6_.push_back(Network::Address::CidrRange::create(addressPtr, length));
    break;
  }
  default:
    ENVOY_LOG(error, "Ip format is not match any ip version: {}", entry);
  }
}

// Determine whether the input address exists in the record
bool BlackOrWhiteListConfig::checkIpEntry(const Network::Address::Instance* address) const {
  switch (address->ip()->version()) {
  case Network::Address::IpVersion::v4:
    if (rawIpV4.find(address->ip()->ipv4()->address()) != rawIpV4.end())
      return true;
    for (auto& cidrRange : cidrIpV4_) {
      if (cidrRange.isInRange(*address))
        return true;
    }
    return false;
  case Network::Address::IpVersion::v6:
    if (rawIpV6.find(address->ip()->addressAsString()) != rawIpV6.end())
      return true;
    for (auto& cidrRange : cidrIpV6_) {
      if (cidrRange.isInRange(*address))
        return true;
    }
    return false;
  default:
    return false;
  }
}

bool BlackOrWhiteListConfig::isAllowed(const Network::Address::Instance* address) const {
  // List type（black or white） and check result jointly determine whether to release the IP
  // return checkIpEntry(address) != isBlackList_;
  auto result = checkIpEntry(address);
  return ((result && (!isBlackList_)) || ((!result) && isBlackList_));
}

Http::FilterHeadersStatus HttpIpRestrictionFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                                 bool) {

  auto route = decoder_callbacks_->route();
  if (!route.get()) {
    return Http::FilterHeadersStatus::Continue; // no route and do nothing
  }

  auto routeEntry = route->routeEntry();
  if (!routeEntry) {
    return Http::FilterHeadersStatus::Continue;
  }

  auto listConfig =
      Http::Utility::resolveMostSpecificPerFilterConfig<BlackOrWhiteListConfig>(name(), route);

  if (!listConfig) {
    return Http::FilterHeadersStatus::Continue;
  }

  Network::Address::InstanceConstSharedPtr remoteAddress;

  if (config_->ip_source_header_.get().size() == 0) {
    remoteAddress = decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
  } else {
    auto header_entry = headers.get(config_->ip_source_header_);
    if (header_entry.empty()) {
      ENVOY_LOG(warn, "No useful remote address get from header: {}.",
                config_->ip_source_header_.get());
      return Http::FilterHeadersStatus::Continue;
    }
    std::string custom_address = std::string(header_entry[0]->value().getStringView());
    try {
      remoteAddress = Network::Utility::parseInternetAddress(custom_address);
    } catch (const EnvoyException&) {
      ENVOY_LOG(warn, "Source ip from header {} format is error: {}",
                config_->ip_source_header_.get(), custom_address);
      return Http::FilterHeadersStatus::Continue;
    }
  }

  if (!remoteAddress.get() || remoteAddress->type() != Network::Address::Type::Ip) {
    ENVOY_LOG(warn, "No useful remote address get or address type is not ip");
    return Http::FilterHeadersStatus::Continue;
  }

  ENVOY_LOG(debug, "Access from remote address {}.", remoteAddress->ip()->addressAsString());

  if (listConfig->isAllowed(remoteAddress.get())) {
    config_->stats().ok_.inc();
    return Http::FilterHeadersStatus::Continue;
  }

  const static std::function<void(Http::HeaderMap & headers)> add_header =
      [](Http::HeaderMap& headers) {
        const static auto flag_header_key = Http::LowerCaseString("x-ip-restriction-denied");
        const static std::string flag_header_value = "true";
        headers.addReference(flag_header_key, flag_header_value);
      };

  decoder_callbacks_->sendLocalReply(Http::Code::Forbidden, "Your IP is not allowed.", add_header,
                                     absl::nullopt, "forbidden_by_ip_restriction");
  config_->stats().denied_.inc();
  ENVOY_LOG(debug, "Access from address {} is forbidden.", remoteAddress->ip()->addressAsString());

  return Http::FilterHeadersStatus::StopIteration;
}

} // namespace IpRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
