#include "source/extensions/filters/http/wasm/wasm_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Wasm {

FilterConfig::FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
                           Server::Configuration::FactoryContext& context) {
  auto plugin_config = Extensions::Common::Wasm::PluginConfig::create(
      config.config(), context.serverFactoryContext(), context.scope(), context.initManager(),
      context.listenerInfo().direction(), &context.listenerInfo().metadata(), false);
  // TODO(jewertow): remove throwing exception
  if (!plugin_config.ok()) {
    throw EnvoyException(
        fmt::format("Unable to create Wasm plugin: {}", plugin_config.status().message()));
  }
  plugin_config_ = std::move(plugin_config.value());
}

FilterConfig::FilterConfig(const envoy::extensions::filters::http::wasm::v3::Wasm& config,
                           Server::Configuration::UpstreamFactoryContext& context) {
  auto plugin_config = Extensions::Common::Wasm::PluginConfig::create(
      config.config(), context.serverFactoryContext(), context.scope(), context.initManager(),
      envoy::config::core::v3::TrafficDirection::OUTBOUND, nullptr, false);
  // TODO(jewertow): remove throwing exception
  if (!plugin_config.ok()) {
    throw EnvoyException(
        fmt::format("Unable to create Wasm plugin: {}", plugin_config.status().message()));
  }
  plugin_config_ = std::move(plugin_config.value());
}

Extensions::Common::Wasm::ContextSharedPtr FilterConfig::createContext() {
  return plugin_config_->createContext();
}

} // namespace Wasm
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
