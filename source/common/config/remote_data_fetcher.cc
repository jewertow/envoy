#include "source/common/config/remote_data_fetcher.h"

#include "envoy/config/core/v3/http_uri.pb.h"

#include "source/common/common/enum_to_int.h"
#include "source/common/common/hex.h"
#include "source/common/crypto/utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/common/json/json_loader.h"
#include <memory>

namespace Envoy {
namespace Config {
namespace DataFetcher {

RemoteDataFetcher::RemoteDataFetcher(Upstream::ClusterManager& cm,
                                     const envoy::config::core::v3::HttpUri& uri,
                                     const std::string& content_hash,
                                     RemoteDataFetcherCallback& callback)
    : cm_(cm), uri_(uri), content_hash_(content_hash), callback_(callback) {}

RemoteDataFetcher::~RemoteDataFetcher() { cancel(); }

void RemoteDataFetcher::cancel() {
  if (request_) {
    request_->cancel();
    ENVOY_LOG(debug, "fetch remote data [uri = {}]: canceled", uri_.uri());
  }

  request_ = nullptr;
}

void RemoteDataFetcher::fetch() {
  Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri_);
  message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Get);
  ENVOY_LOG(debug, "fetch remote data from [uri = {}]: start", uri_.uri());
  const auto thread_local_cluster = cm_.getThreadLocalCluster(uri_.cluster());
  if (thread_local_cluster != nullptr) {
    request_ = thread_local_cluster->httpAsyncClient().send(
        std::move(message), *this,
        Http::AsyncClient::RequestOptions().setTimeout(
            std::chrono::milliseconds(DurationUtil::durationToMilliseconds(uri_.timeout()))));
  } else {
    ENVOY_LOG(debug, "fetch remote data [uri = {}]: no cluster {}", uri_.uri(), uri_.cluster());
    callback_.onFailure(FailureReason::Network);
  }
}

void RemoteDataFetcher::onSuccess(const Http::AsyncClient::Request&,
                                  Http::ResponseMessagePtr&& response) {
  const uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
  if (status_code == enumToInt(Http::Code::OK)) {
    ENVOY_LOG(debug, "fetch remote data [uri = {}]: success", uri_.uri());
    if (response->body().length() > 0) {
      auto& crypto_util = Envoy::Common::Crypto::UtilitySingleton::get();
      const auto content_hash = Hex::encode(crypto_util.getSha256Digest(response->body()));

      if (content_hash_ != content_hash) {
        ENVOY_LOG(debug, "fetch remote data [uri = {}]: data is invalid", uri_.uri());
        callback_.onFailure(FailureReason::InvalidData);
      } else {
        callback_.onSuccess(response->bodyAsString());
      }
    } else {
      ENVOY_LOG(debug, "fetch remote data [uri = {}]: body is empty", uri_.uri());
      callback_.onFailure(FailureReason::Network);
    }
  } else {
    ENVOY_LOG(debug, "fetch remote data [uri = {}]: response status code {}", uri_.uri(),
              status_code);
    callback_.onFailure(FailureReason::Network);
  }

  request_ = nullptr;
}

void RemoteDataFetcher::onFailure(const Http::AsyncClient::Request&,
                                  Http::AsyncClient::FailureReason reason) {
  ENVOY_LOG(debug, "fetch remote data [uri = {}]: network error {}", uri_.uri(), enumToInt(reason));
  request_ = nullptr;
  callback_.onFailure(FailureReason::Network);
}

OciFetcher::OciFetcher(Upstream::ClusterManager& cm,
                                     const envoy::config::core::v3::HttpUri& uri,
                                     const std::string& token,
                                     const std::string& content_hash,
                                     RemoteDataFetcherCallback& callback)
    : cm_(cm), uri_(uri), token_(token), content_hash_(content_hash), callback_(callback) {}

OciFetcher::~OciFetcher() { cancel(); }

void OciFetcher::cancel() {
  if (request_) {
    request_->cancel();
    ENVOY_LOG(debug, "fetch oci image [uri = {}]: canceled", uri_.uri());
  }

  request_ = nullptr;
}

void OciFetcher::fetch() {
  Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri_);
  message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Get);
  std::string bearer = "Bearer ";
  absl::StrAppend(&bearer, token_);
  message->headers().setAuthorization(bearer);
  // TODO: add "accept: application/vnd.oci.image.manifest.v1+json"
  ENVOY_LOG(info, "fetch oci image from [uri = {}]: start", uri_.uri());
  const auto thread_local_cluster = cm_.getThreadLocalCluster(uri_.cluster());
  if (thread_local_cluster != nullptr) {
    request_ = thread_local_cluster->httpAsyncClient().send(
        std::move(message), *this,
        Http::AsyncClient::RequestOptions().setTimeout(
            std::chrono::milliseconds(DurationUtil::durationToMilliseconds(uri_.timeout()))));
  } else {
    ENVOY_LOG(info, "fetch oci image [uri = {}]: no cluster {}", uri_.uri(), uri_.cluster());
    callback_.onFailure(FailureReason::Network);
  }
}

void OciFetcher::onSuccess(const Http::AsyncClient::Request&,
                                  Http::ResponseMessagePtr&& response) {
  const uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
  if (status_code == enumToInt(Http::Code::OK)) {
    ENVOY_LOG(info, "fetch oci image [uri = {}, body = {}]: success", uri_.uri(), response->body().toString());
    if (response->body().length() > 0) {
      auto& crypto_util = Envoy::Common::Crypto::UtilitySingleton::get();
      const auto content_hash = Hex::encode(crypto_util.getSha256Digest(response->body()));

      // TODO(jewertow)
      // if (content_hash_ != content_hash) {
      //   ENVOY_LOG(info, "fetch oci image [uri = {}]: data is invalid", uri_.uri());
      //   callback_.onFailure(FailureReason::InvalidData);
      // } else {
      std::string body = response->bodyAsString();
      // Parse manifests response
      std::string digest_value;
      if (!body.empty()) {
        try {
          Json::ObjectSharedPtr json_body =
              THROW_OR_RETURN_VALUE(Json::Factory::loadFromString(body), Json::ObjectSharedPtr);
          auto layers = THROW_OR_RETURN_VALUE(json_body->getObjectArray("layers"),
                                    std::vector<Json::ObjectSharedPtr>);
          auto digest = layers[0]->getString("digest", "");
          if (digest->empty()) {
            ENVOY_LOG(error, "fetch oci image [uri = {}, body = {}]: could not parse digest", uri_.uri(), response->body().toString());  
          } else {
            ENVOY_LOG(info, "fetch oci image [uri = {}, digest = {}]: found digest", uri_.uri(), digest->c_str());
            callback_.onSuccess(digest.value());
          }
        } catch (...) {
          ENVOY_LOG(error, "fetch oci image [uri = {}, body = {}]: failed to parse response body to JSON", uri_.uri(), response->body().toString());
        }
      }
    } else {
      ENVOY_LOG(info, "fetch oci image [uri = {}]: body is empty", uri_.uri());
      callback_.onFailure(FailureReason::Network);
    }
  } else {
    ENVOY_LOG(info, "fetch oci image [uri = {}, body = {}]: response status code {}", uri_.uri(), response->body().toString(),
              status_code);
    callback_.onFailure(FailureReason::Network);
  }

  request_ = nullptr;
}

void OciFetcher::onFailure(const Http::AsyncClient::Request&,
                                  Http::AsyncClient::FailureReason reason) {
  ENVOY_LOG(info, "fetch oci image [uri = {}]: network error {}", uri_.uri(), enumToInt(reason));
  request_ = nullptr;
  callback_.onFailure(FailureReason::Network);
}

OciBlobFetcher::OciBlobFetcher(Upstream::ClusterManager& cm,
                                     const envoy::config::core::v3::HttpUri& uri,
                                     const std::string& token,
                                     const std::string& digest,
                                     const std::string& content_hash,
                                     RemoteDataFetcherCallback& callback)
    : cm_(cm), uri_(uri), token_(token), digest_(digest), content_hash_(content_hash), callback_(callback) {}

OciBlobFetcher::~OciBlobFetcher() { cancel(); }

void OciBlobFetcher::cancel() {
  if (request_) {
    request_->cancel();
    ENVOY_LOG(debug, "fetch oci image blob [uri = {}]: canceled", uri_.uri());
  }

  request_ = nullptr;
}

void OciBlobFetcher::fetch() {
  Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri_);
  message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Get);
  std::string bearer = "Bearer ";
  absl::StrAppend(&bearer, token_);
  message->headers().setAuthorization(bearer);
  // TODO: add "accept: application/vnd.oci.image.manifest.v1+json"
  ENVOY_LOG(info, "fetch oci image blob from [uri = {}]: start", uri_.uri());
  const auto thread_local_cluster = cm_.getThreadLocalCluster(uri_.cluster());
  if (thread_local_cluster != nullptr) {
    request_ = thread_local_cluster->httpAsyncClient().send(
        std::move(message), *this,
        Http::AsyncClient::RequestOptions().setTimeout(
            std::chrono::milliseconds(DurationUtil::durationToMilliseconds(uri_.timeout()))));
  } else {
    ENVOY_LOG(info, "fetch oci image blob [uri = {}]: no cluster {}", uri_.uri(), uri_.cluster());
    callback_.onFailure(FailureReason::Network);
  }
}

void OciBlobFetcher::onSuccess(const Http::AsyncClient::Request&,
                                  Http::ResponseMessagePtr&& response) {
  const uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
  if (status_code == enumToInt(Http::Code::OK)) {
    ENVOY_LOG(info, "fetch oci image blob [uri = {}, body = {}]: success", uri_.uri(), response->body().toString());
    if (response->body().length() > 0) {
      auto& crypto_util = Envoy::Common::Crypto::UtilitySingleton::get();
      const auto content_hash = Hex::encode(crypto_util.getSha256Digest(response->body()));

      // TODO(jewertow)
      // if (content_hash_ != content_hash) {
      //   ENVOY_LOG(info, "fetch oci image [uri = {}]: data is invalid", uri_.uri());
      //   callback_.onFailure(FailureReason::InvalidData);
      // } else {

      callback_.onSuccess(response->bodyAsString());
    } else {
      ENVOY_LOG(info, "fetch oci image blob [uri = {}]: body is empty", uri_.uri());
      callback_.onFailure(FailureReason::Network);
    }
  } else {
    ENVOY_LOG(info, "fetch oci image blob [uri = {}, body = {}]: response status code {}", uri_.uri(), response->body().toString(),
              status_code);
    callback_.onFailure(FailureReason::Network);
  }

  request_ = nullptr;
}

void OciBlobFetcher::onFailure(const Http::AsyncClient::Request&,
                                  Http::AsyncClient::FailureReason reason) {
  ENVOY_LOG(info, "fetch oci image blob [uri = {}]: network error {}", uri_.uri(), enumToInt(reason));
  request_ = nullptr;
  callback_.onFailure(FailureReason::Network);
}

} // namespace DataFetcher
} // namespace Config
} // namespace Envoy
