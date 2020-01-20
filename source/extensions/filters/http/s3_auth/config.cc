#include "extensions/filters/http/s3_auth/config.h"

#include "envoy/extensions/filters/http/s3_auth/v3/s3_auth.pb.h"
#include "envoy/extensions/filters/http/s3_auth/v3/s3_auth.pb.validate.h"
#include "envoy/registry/registry.h"

#include "extensions/filters/http/s3_auth/s3_auth_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace S3AuthFilter {

Http::FilterFactoryCb S3AuthFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::s3_auth::v3::S3Auth&, const std::string& stats_prefix,
    Server::Configuration::FactoryContext&) {
  return [stats_prefix](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    auto filter = std::make_shared<Filter>(stats_prefix);
    callbacks.addStreamFilter(filter);
  };
}

/**
 * Static registration for the S3 auth filter. @see RegisterFactory.
 */
REGISTER_FACTORY(S3AuthFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace S3AuthFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
