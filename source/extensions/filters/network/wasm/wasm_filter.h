#pragma once

#include <memory>

#include "envoy/extensions/filters/network/wasm/v3/wasm.pb.validate.h"
#include "envoy/network/filter.h"
#include "envoy/server/filter_config.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/extensions/common/wasm/remote_async_datasource.h"
#include "source/extensions/common/wasm/wasm.h"
#include "source/extensions/filters/network/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Wasm {

class FilterConfig {
public:
  FilterConfig(const envoy::extensions::filters::network::wasm::v3::Wasm& proto_config,
               Server::Configuration::FactoryContext& context);

  Extensions::Common::Wasm::ContextSharedPtr createContext();

private:
  Extensions::Common::Wasm::PluginConfigPtr plugin_config_;
};

using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

} // namespace Wasm
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
