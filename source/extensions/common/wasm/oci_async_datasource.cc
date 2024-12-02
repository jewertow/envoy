#include "source/extensions/common/wasm/oci_async_datasource.h"

#include "envoy/config/core/v3/base.pb.h"

#include "source/common/config/utility.h"

#include "fmt/format.h"
#include <string>

namespace Envoy {

// Default Parameters of the jittered backoff strategy.
// static constexpr uint32_t RetryInitialDelayMilliseconds = 1000;
// static constexpr uint32_t RetryMaxDelayMilliseconds = 10 * 1000;
// static constexpr uint32_t RetryCount = 1;

OciAsyncDataProvider::OciAsyncDataProvider(
    Upstream::ClusterManager& cm, Init::Manager& manager,
    const envoy::config::core::v3::HttpUri uri, std::string token, std::string sha256,
    bool allow_empty, OciAsyncDataSourceCb&& callback)
    : allow_empty_(allow_empty), callback_(std::move(callback)),
      fetcher_(std::make_unique<Config::DataFetcher::OciFetcher>(cm, uri, token, sha256, *this)),
      init_target_("OciAsyncDataProvider", [this]() { start(); }) {

  // auto strategy_or_error = Config::Utility::prepareJitteredExponentialBackOffStrategy(
  //     source, random, RetryInitialDelayMilliseconds, RetryMaxDelayMilliseconds);
  // THROW_IF_NOT_OK_REF(strategy_or_error.status());
  // backoff_strategy_ = std::move(strategy_or_error.value());

  // retry_timer_ = dispatcher.createTimer([this]() -> void { start(); });

  manager.add(init_target_);
}

} // namespace Envoy
