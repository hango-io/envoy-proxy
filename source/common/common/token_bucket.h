#pragma once

#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"

#include "source/common/common/lock_guard.h"
#include "source/common/common/thread.h"
#include "source/common/common/token_bucket_impl.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Common {

class AccurateTokenBucket : public Envoy::TokenBucket {
public:
  AccurateTokenBucket(Envoy::Server::Configuration::ServerFactoryContext& context,
                      uint64_t max_tokens, double fill_rate = 1)
      : impl_(max_tokens, context.timeSource(), fill_rate) {}

  // TokenBucket
  uint64_t consume(uint64_t tokens, bool allow_partial) override {
    Thread::LockGuard guard(lock_);
    return impl_.consume(tokens, allow_partial);
  }
  uint64_t consume(uint64_t tokens, bool allow_partial,
                   std::chrono::milliseconds& time_to_next_token) override {
    Thread::LockGuard guard(lock_);
    return impl_.consume(tokens, allow_partial, time_to_next_token);
  }
  std::chrono::milliseconds nextTokenAvailable() override {
    Thread::LockGuard guard(lock_);
    return impl_.nextTokenAvailable();
  }
  void maybeReset(uint64_t num_tokens) override {
    Thread::LockGuard guard(lock_);
    return impl_.maybeReset(num_tokens);
  }

private:
  TokenBucketImpl impl_ GUARDED_BY(lock_);
  Envoy::Thread::MutexBasicLockable lock_;
};

class TheadLocalTokenBucket : public Envoy::TokenBucket {
public:
  struct ThreadLocalImpl : public ThreadLocal::ThreadLocalObject {
    ThreadLocalImpl(uint64_t max_tokens, TimeSource& time_source, double fill_rate = 1)
        : impl_(max_tokens, time_source, fill_rate) {}
    TokenBucketImpl impl_;
  };

  using TypedSlot = Envoy::ThreadLocal::TypedSlot<ThreadLocalImpl>;
  using TypedSlotPtr = std::unique_ptr<TypedSlot>;

  TheadLocalTokenBucket(Envoy::Server::Configuration::ServerFactoryContext& context,
                        uint64_t max_tokens, double fill_rate = 1);

  // TokenBucket
  uint64_t consume(uint64_t tokens, bool allow_partial) override {
    return impl_slot_ref_->impl_.consume(tokens, allow_partial);
  }
  uint64_t consume(uint64_t tokens, bool allow_partial,
                   std::chrono::milliseconds& time_to_next_token) override {
    return impl_slot_ref_->impl_.consume(tokens, allow_partial, time_to_next_token);
  }
  std::chrono::milliseconds nextTokenAvailable() override {
    return impl_slot_ref_->impl_.nextTokenAvailable();
  }

  void maybeReset(uint64_t num_tokens) override;

  ~TheadLocalTokenBucket() {
    if (!main_dispatcher_.isThreadSafe()) {
      auto shared_ptr_wrapper = std::make_shared<TypedSlotPtr>(std::move(impl_slot_ptr_));
      main_dispatcher_.post([shared_ptr_wrapper] { shared_ptr_wrapper->reset(); });
    }
  }

private:
  TypedSlotPtr impl_slot_ptr_;
  // ref to impl_slot_ptr_.
  TypedSlot& impl_slot_ref_;

  Event::Dispatcher& main_dispatcher_;
};

} // namespace Common
} // namespace Common
} // namespace Proxy
} // namespace Envoy
