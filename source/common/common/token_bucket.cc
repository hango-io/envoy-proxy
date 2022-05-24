#include "source/common/common/token_bucket.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Common {

TheadLocalTokenBucket::TheadLocalTokenBucket(
    Envoy::Server::Configuration::ServerFactoryContext& context, uint64_t max_tokens,
    double fill_rate)
    : impl_slot_ptr_(TypedSlot::makeUnique(context.threadLocal())), impl_slot_ref_(*impl_slot_ptr_),
      main_dispatcher_(context.mainThreadDispatcher()) {
  impl_slot_ref_.set([&context, max_tokens, fill_rate](Event::Dispatcher&) {
    return std::make_unique<ThreadLocalImpl>(max_tokens, context.timeSource(), fill_rate);
  });
}

void TheadLocalTokenBucket::maybeReset(uint64_t num_tokens) {
  // Can only reset by main thread for ThreadLocalTokenBucket
  if (main_dispatcher_.isThreadSafe()) {
    impl_slot_ref_.runOnAllThreads(
        [num_tokens](OptRef<ThreadLocalImpl> tls) { tls->impl_.maybeReset(num_tokens); });
  }
}

} // namespace Common
} // namespace Common
} // namespace Proxy
} // namespace Envoy
