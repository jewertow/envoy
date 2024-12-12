#include "source/extensions/common/wasm/oci_async_datasource.h"

#include "envoy/config/core/v3/base.pb.h"

#include "source/common/config/utility.h"

#include "fmt/format.h"
#include <string>

namespace Envoy {

OciManifestProvider::OciManifestProvider(
    Upstream::ClusterManager& cm, Init::Manager& manager,
    const envoy::config::core::v3::HttpUri uri, std::string token, std::string sha256,
    bool allow_empty, OciManifestCb&& callback)
    : allow_empty_(allow_empty), callback_(std::move(callback)),
      fetcher_(std::make_unique<Config::DataFetcher::OciImageManifestFetcher>(cm, uri, token, sha256, *this)),
      init_target_("OciManifestProvider", [this]() { start(); }) {

  manager.add(init_target_);
}

OciBlobProvider::OciBlobProvider(
    Upstream::ClusterManager& cm, Init::Manager& manager,
    const envoy::config::core::v3::HttpUri uri, std::string token, std::string digest, std::string sha256,
    bool allow_empty, OciBlobCb&& callback)
    : allow_empty_(allow_empty), callback_(std::move(callback)),
      fetcher_(std::make_unique<Config::DataFetcher::OciBlobFetcher>(cm, uri, token, digest, sha256, *this)),
      init_target_("OciBlobProvider", [this]() { start(); }) {

  manager.add(init_target_);
}

} // namespace Envoy
