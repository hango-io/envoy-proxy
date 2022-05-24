#include "source/common/kafka/producer.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Kafka {

KafkaProducer::KafkaProducer(const std::string& brokers, const std::string& topic)
    : brokers_string_(brokers), topic_string_(topic) {
  createProducer();
}

KafkaProducer::~KafkaProducer() {
  if (!topic_.get() || !producer_.get()) {
    return;
  }
  // flush all message
  auto code = producer_->flush(20);
  if (code != RdKafka::ErrorCode::ERR_NO_ERROR) {
    ENVOY_LOG(error, "Failed flush messages to server. Error: {}", RdKafka::err2str(code));
  }
}

void KafkaProducer::produce(const std::string& message) {
  // TODO no retry after init failed.
  if (!topic_.get() || !producer_.get()) {
    ENVOY_LOG(debug, "No valid topic handler or producer and recreate it.");
    createProducer();
    if (!topic_.get() || !producer_.get()) {
      ENVOY_LOG(error, "No valid topic handler or producer and failed to recreate it.");
      return;
    }
  }
  int32_t partition = RdKafka::Topic::PARTITION_UA;
  auto code = producer_->produce(topic_.get(), partition, RdKafka::Producer::RK_MSG_COPY,
                                 (void*)message.data(), message.size(), nullptr, 0, nullptr);
  if (code != RdKafka::ErrorCode::ERR_NO_ERROR) {
    ENVOY_LOG(error, "Failed send message to server. Error: {}", RdKafka::err2str(code));
  }
}

void KafkaProducer::createProducer() {
  if (brokers_string_.empty() || topic_string_.empty()) {
    ENVOY_LOG(error, "No broker or topic provided. Please check your config.");
    return;
  }

  ENVOY_LOG(debug, "Create kafka producer by brokers: {} and topic: {}", brokers_string_,
            topic_string_);

  std::string errStr;
  conf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
  topicConf_.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));

  if (!conf_.get() || !topicConf_.get()) {
    ENVOY_LOG(error, "Failed create producer conf or top conf.");
    return;
  }

  auto reuslt = conf_->set("bootstrap.servers", brokers_string_, errStr);
  if (reuslt != RdKafka::Conf::ConfResult::CONF_OK) {
    ENVOY_LOG(error, "Failed init producer conf. Err: {}", errStr);
    return;
  }

  producer_.reset(RdKafka::Producer::create(conf_.get(), errStr));

  if (!producer_.get()) {
    ENVOY_LOG(error, "Failed create producer. Please check your config. Err: {}", errStr);
    return;
  }

  topic_.reset(RdKafka::Topic::create(producer_.get(), topic_string_, topicConf_.get(), errStr));
  if (!topic_.get()) {
    ENVOY_LOG(error, "Fail create topic handler. Please check your config. Err: {}", errStr);
    return;
  }
}

} // namespace Kafka
} // namespace Common
} // namespace Proxy
} // namespace Envoy
