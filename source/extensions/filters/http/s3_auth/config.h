#pragma once

#include "envoy/extensions/filters/http/s3_auth/v3/s3_auth.pb.h"
#include "envoy/extensions/filters/http/s3_auth/v3/s3_auth.pb.validate.h"

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace S3AuthFilter {

/**
 * Config registration for the S3 auth filter.
 */
class S3AuthFilterFactory
    : public Common::FactoryBase<envoy::extensions::filters::http::s3_auth::v3::S3Auth> {
public:
  S3AuthFilterFactory() : FactoryBase(HttpFilterNames::get().S3Auth) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::s3_auth::v3::S3Auth& proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;
};

} // namespace S3AuthFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
