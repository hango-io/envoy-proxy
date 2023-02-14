#pragma once

#include <memory>
#include <vector>

#include "source/common/common/logger.h"

#include "librdkafka/rdkafkacpp.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Kafka {

class KafkaProducer : public Logger::Loggable<Logger::Id::client> {
public:
  KafkaProducer(const std::string& brokers, const std::string& topic);
  ~KafkaProducer();

  void produce(const std::string& message);

private:
  void createProducer();
  std::string brokers_string_{}, topic_string_{};

  std::shared_ptr<RdKafka::Producer> producer_{nullptr};
  std::shared_ptr<RdKafka::Conf> conf_{nullptr};
  std::shared_ptr<RdKafka::Conf> topicConf_{nullptr};
  std::shared_ptr<RdKafka::Topic> topic_{nullptr};
};

} // namespace Kafka
} // namespace Common
} // namespace Proxy
} // namespace Envoy
