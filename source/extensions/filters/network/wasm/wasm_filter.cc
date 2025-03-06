#include "source/extensions/filters/network/wasm/wasm_filter.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Wasm {

FilterConfig::FilterConfig(const envoy::extensions::filters::network::wasm::v3::Wasm& config,
                           Server::Configuration::FactoryContext& context) {
  auto plugin_config = Common::Wasm::PluginConfig::create(
      config.config(), context.serverFactoryContext(), context.scope(), context.initManager(),
      context.listenerInfo().direction(), &context.listenerInfo().metadata(), false);
  // TODO(jewertow): remove throwing exception
  if (!plugin_config.ok()) {
    throw EnvoyException(
        fmt::format("Unable to create Wasm plugin: {}", plugin_config.status().message()));
  }
  plugin_config_ = std::move(plugin_config.value());
}

Common::Wasm::ContextSharedPtr FilterConfig::createContext() {
  return plugin_config_->createContext();
}

} // namespace Wasm
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
