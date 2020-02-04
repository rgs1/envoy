#include "extensions/filters/http/aws_request_signing/aws_request_signing_filter.h"

#include "envoy/extensions/filters/http/aws_request_signing/v3/aws_request_signing.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsRequestSigningFilter {

FilterConfigImpl::FilterConfigImpl(HttpFilters::Common::Aws::SignerPtr&& signer,
                                   const std::string& stats_prefix, Stats::Scope& scope)
    : signer_(std::move(signer)), stats_(Filter::generateStats(stats_prefix, scope)) {}

Filter::Filter(const std::shared_ptr<FilterConfig>& config) : config_(config) {}

HttpFilters::Common::Aws::Signer& FilterConfigImpl::signer() { return *signer_; }

FilterStats& FilterConfigImpl::stats() { return stats_; }

FilterStats Filter::generateStats(const std::string& prefix, Stats::Scope& scope) {
  const std::string final_prefix = prefix + "aws_request_signing.";
  return {ALL_AWS_REQUEST_SIGNING_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::HeaderMap& headers, bool) {
  try {
    config_->signer().sign(headers);
    config_->stats().auth_header_added_.inc();
  } catch (const EnvoyException& e) {
    ENVOY_LOG(debug, "signing failed: {}", e.what());
    config_->stats().signing_failed_.inc();
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance&, bool) {
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus Filter::decodeTrailers(Http::HeaderMap&) {
  return Http::FilterTrailersStatus::Continue;
}

void Filter::setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks&) {}

Http::FilterHeadersStatus Filter::encode100ContinueHeaders(Http::HeaderMap&) {
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::HeaderMap&, bool) {
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance&, bool) {
  return Http::FilterDataStatus::Continue;
}

Http::FilterMetadataStatus Filter::encodeMetadata(Http::MetadataMap&) {
  return Http::FilterMetadataStatus::Continue;
}

Http::FilterTrailersStatus Filter::encodeTrailers(Http::HeaderMap&) {
  return Http::FilterTrailersStatus::Continue;
}

void Filter::setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks&) {}

} // namespace AwsRequestSigningFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
