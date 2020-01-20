.. _config_http_filters_s3_auth:

Tap
===

* :ref:`v2 API reference <envoy_api_msg_config.filter.http.tap.v2alpha.S3Auth>`
* This filter should be configured with the name *envoy.filters.http.s3_auth*.

.. attention::

  The tap filter is experimental and is currently under active development.

The HTTP S3 auth filter is used to access authenticated buckets in S3. It uses the
existing AWS Credential Provider to get the secrets used for generating the required
headers.

Example configuration
---------------------

Example filter configuration:

.. code-block:: yaml

  name: envoy.filters.http.s3_auth
  typed_config:
    "@type": type.googleapis.com/envoy.config.filter.http.s3_auth.v2alpha.S3Auth


Statistics
----------

The S3 auth filter outputs statistics in the *http.<stat_prefix>.s3_auth.* namespace. The :ref:`stat prefix
<envoy_api_field_config.filter.network.http_connection_manager.v2.HttpConnectionManager.stat_prefix>`
comes from the owning HTTP connection manager.

.. csv-table::
  :header: Name, Type, Description
  :widths: 1, 1, 2

  auth_header_added, Counter, Total authentication headers added to requests
