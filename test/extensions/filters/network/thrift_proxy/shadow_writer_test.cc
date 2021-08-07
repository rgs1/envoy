#include <memory>

#include "envoy/extensions/filters/network/thrift_proxy/v3/thrift_proxy.pb.h"
#include "envoy/tcp/conn_pool.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/extensions/filters/network/thrift_proxy/app_exception_impl.h"
#include "source/extensions/filters/network/thrift_proxy/router/shadow_writer_impl.h"

#include "test/extensions/filters/network/thrift_proxy/mocks.h"
#include "test/extensions/filters/network/thrift_proxy/utility.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/factory_context.h"
#include "test/mocks/upstream/host.h"
#include "test/test_common/printers.h"
#include "test/test_common/registry.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ThriftProxy {
namespace Router {

struct MockNullResponseDecoder : public NullResponseDecoder {
  MockNullResponseDecoder(Transport& transport, Protocol& protocol)
      : NullResponseDecoder(transport, protocol) {}

  MOCK_METHOD(ThriftFilters::ResponseStatus, upstreamData, (Buffer::Instance & data), ());
};

class ShadowWriterTest : public testing::Test {
public:
  ShadowWriterTest() {
    shadow_writer_ = std::make_shared<ShadowWriterImpl>(cm_, "test", context_.scope(), dispatcher_);
    metadata_ = std::make_shared<MessageMetadata>();
    metadata_->setMethodName("ping");
    metadata_->setMessageType(MessageType::Call);
    metadata_->setSequenceId(1);

    host_ = std::make_shared<NiceMock<Upstream::MockHost>>();
  }

  void testOnUpstreamData(MessageType message_type = MessageType::Reply, bool success = true,
                          bool on_data_throw_app_exception = false,
                          bool on_data_throw_regular_exception = false) {
    NiceMock<Network::MockClientConnection> connection;

    EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(&cluster_));
    EXPECT_CALL(*cluster_.cluster_.info_, maintenanceMode()).WillOnce(Return(false));
    EXPECT_CALL(cluster_, tcpConnPool(_, _))
        .WillOnce(Return(Upstream::TcpPoolData([]() {}, &conn_pool_)));
    EXPECT_CALL(conn_pool_, newConnection(_))
        .WillOnce(Invoke(
            [&](Tcp::ConnectionPool::Callbacks& callbacks) -> Tcp::ConnectionPool::Cancellable* {
              auto data =
                  std::make_unique<NiceMock<Envoy::Tcp::ConnectionPool::MockConnectionData>>();
              EXPECT_CALL(*data, connectionState())
                  .WillRepeatedly(Invoke([&]() -> Tcp::ConnectionPool::ConnectionState* {
                    return conn_state_.get();
                  }));
              EXPECT_CALL(*data, setConnectionState_(_))
                  .WillOnce(Invoke([&](Tcp::ConnectionPool::ConnectionStatePtr& cs) -> void {
                    conn_state_.swap(cs);
                  }));

              EXPECT_CALL(*data, connection()).WillRepeatedly(ReturnRef(connection));
              callbacks.onPoolReady(std::move(data), host_);
              return nullptr;
            }));

    ShadowRouterImpl shadow_router(*shadow_writer_, "shadow_cluster", metadata_,
                                   TransportType::Framed, ProtocolType::Binary);
    EXPECT_TRUE(shadow_router.createUpstreamRequest());

    // Exercise methods are no-ops by design.
    shadow_router.resetDownstreamConnection();
    shadow_router.onAboveWriteBufferHighWatermark();
    shadow_router.onBelowWriteBufferLowWatermark();
    shadow_router.downstreamConnection();
    shadow_router.metadataMatchCriteria();

    EXPECT_CALL(connection, write(_, false));
    shadow_router.messageEnd();

    // Prepare response metadata & data processing.
    MessageMetadataSharedPtr response_metadata = std::make_shared<MessageMetadata>();
    response_metadata->setMessageType(message_type);
    response_metadata->setSequenceId(1);

    auto transport_ptr =
        NamedTransportConfigFactory::getFactory(TransportType::Framed).createTransport();
    auto protocol_ptr =
        NamedProtocolConfigFactory::getFactory(ProtocolType::Binary).createProtocol();
    auto decoder_ptr = std::make_unique<MockNullResponseDecoder>(*transport_ptr, *protocol_ptr);
    decoder_ptr->messageBegin(response_metadata);
    decoder_ptr->success_ = success;

    if (on_data_throw_regular_exception || on_data_throw_app_exception) {
      EXPECT_CALL(connection, close(_));
      EXPECT_CALL(*decoder_ptr, upstreamData(_))
          .WillOnce(Return(ThriftFilters::ResponseStatus::Reset));
    } else {
      EXPECT_CALL(*decoder_ptr, upstreamData(_))
          .WillOnce(Return(ThriftFilters::ResponseStatus::Complete));
    }

    shadow_router.upstream_response_callbacks_ =
        std::make_unique<ShadowUpstreamResponseCallbacksImpl>(*decoder_ptr);

    Buffer::OwnedImpl response_buffer;
    shadow_router.onUpstreamData(response_buffer, false);

    if (on_data_throw_regular_exception || on_data_throw_app_exception) {
      return;
    }

    // Check stats.
    switch (message_type) {
    case MessageType::Reply:
      EXPECT_EQ(1UL, cluster_.cluster_.info_->statsScope()
                         .counterFromString("thrift.upstream_resp_reply")
                         .value());
      if (success) {
        EXPECT_EQ(1UL, cluster_.cluster_.info_->statsScope()
                           .counterFromString("thrift.upstream_resp_success")
                           .value());
      } else {
        EXPECT_EQ(1UL, cluster_.cluster_.info_->statsScope()
                           .counterFromString("thrift.upstream_resp_error")
                           .value());
      }
      break;
    case MessageType::Exception:
      EXPECT_EQ(1UL, cluster_.cluster_.info_->statsScope()
                         .counterFromString("thrift.upstream_resp_exception")
                         .value());
      break;
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  NiceMock<Upstream::MockThreadLocalCluster> cluster_;
  Tcp::ConnectionPool::ConnectionStatePtr conn_state_;
  NiceMock<Upstream::MockClusterManager> cm_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  Envoy::ConnectionPool::MockCancellable cancellable_;
  MessageMetadataSharedPtr metadata_;
  NiceMock<Tcp::ConnectionPool::MockInstance> conn_pool_;
  std::shared_ptr<NiceMock<Upstream::MockHost>> host_;
  std::shared_ptr<ShadowWriterImpl> shadow_writer_;
};

TEST_F(ShadowWriterTest, SubmitClusterNotFound) {
  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(nullptr));
  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_EQ(absl::nullopt, router_handle);
}

TEST_F(ShadowWriterTest, SubmitClusterInMaintenance) {
  std::shared_ptr<Upstream::MockThreadLocalCluster> cluster =
      std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
  EXPECT_CALL(*cluster->cluster_.info_, maintenanceMode()).WillOnce(Return(true));
  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(cluster.get()));
  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_EQ(absl::nullopt, router_handle);
}

TEST_F(ShadowWriterTest, SubmitNoHealthyUpstream) {
  metadata_->setMessageType(MessageType::Oneway);

  std::shared_ptr<Upstream::MockThreadLocalCluster> cluster =
      std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(cluster.get()));
  EXPECT_CALL(*cluster->cluster_.info_, maintenanceMode()).WillOnce(Return(false));
  EXPECT_CALL(*cluster, tcpConnPool(_, _)).WillOnce(Return(absl::nullopt));
  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_EQ(absl::nullopt, router_handle);

  // We still count the request, even if it didn't go through.
  EXPECT_EQ(
      1UL,
      cluster->cluster_.info_->statsScope().counterFromString("thrift.upstream_rq_oneway").value());
}

TEST_F(ShadowWriterTest, SubmitConnectionNotReady) {
  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(&cluster_));
  EXPECT_CALL(*cluster_.cluster_.info_, maintenanceMode()).WillOnce(Return(false));
  EXPECT_CALL(cluster_, tcpConnPool(_, _))
      .WillOnce(Return(Upstream::TcpPoolData([]() {}, &conn_pool_)));
  EXPECT_CALL(cancellable_, cancel(_));
  EXPECT_CALL(conn_pool_, newConnection(_))
      .WillOnce(Invoke([&](Tcp::ConnectionPool::Callbacks&) -> Tcp::ConnectionPool::Cancellable* {
        return &cancellable_;
      }));
  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_NE(absl::nullopt, router_handle);
  EXPECT_TRUE(router_handle.value().get().waitingForConnection());

  EXPECT_EQ(
      1UL,
      cluster_.cluster_.info_->statsScope().counterFromString("thrift.upstream_rq_call").value());
}

TEST_F(ShadowWriterTest, ShadowRequestPoolReady) {
  NiceMock<Network::MockClientConnection> connection;

  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(&cluster_));
  EXPECT_CALL(*cluster_.cluster_.info_, maintenanceMode()).WillOnce(Return(false));
  EXPECT_CALL(cluster_, tcpConnPool(_, _))
      .WillOnce(Return(Upstream::TcpPoolData([]() {}, &conn_pool_)));
  EXPECT_CALL(conn_pool_, newConnection(_))
      .WillOnce(Invoke(
          [&](Tcp::ConnectionPool::Callbacks& callbacks) -> Tcp::ConnectionPool::Cancellable* {
            auto data =
                std::make_unique<NiceMock<Envoy::Tcp::ConnectionPool::MockConnectionData>>();
            EXPECT_CALL(*data, connectionState())
                .WillRepeatedly(Invoke(
                    [&]() -> Tcp::ConnectionPool::ConnectionState* { return conn_state_.get(); }));
            EXPECT_CALL(*data, setConnectionState_(_))
                .WillOnce(Invoke([&](Tcp::ConnectionPool::ConnectionStatePtr& cs) -> void {
                  conn_state_.swap(cs);
                }));
            EXPECT_CALL(*data, connection()).WillRepeatedly(ReturnRef(connection));
            callbacks.onPoolReady(std::move(data), host_);
            return nullptr;
          }));

  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_NE(absl::nullopt, router_handle);
  EXPECT_CALL(connection, write(_, false));

  auto& request_owner = router_handle.value().get().requestOwner();

  Buffer::OwnedImpl passthrough_data;
  FieldType field_type;
  FieldType key_type;
  FieldType value_type;
  int16_t field_id;
  bool bool_value;
  uint8_t byte_value;
  int16_t int16_value;
  int32_t int32_value;
  int64_t int64_value;
  double double_value;
  uint32_t container_size;

  EXPECT_EQ(FilterStatus::Continue, request_owner.transportBegin(nullptr));
  EXPECT_EQ(FilterStatus::Continue, request_owner.passthroughData(passthrough_data));
  EXPECT_EQ(FilterStatus::Continue, request_owner.structBegin(""));
  EXPECT_EQ(FilterStatus::Continue, request_owner.fieldBegin("", field_type, field_id));
  EXPECT_EQ(FilterStatus::Continue, request_owner.fieldEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.structEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.boolValue(bool_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.byteValue(byte_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.int16Value(int16_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.int32Value(int32_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.int64Value(int64_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.doubleValue(double_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.stringValue(""));
  EXPECT_EQ(FilterStatus::Continue, request_owner.mapBegin(key_type, value_type, container_size));
  EXPECT_EQ(FilterStatus::Continue, request_owner.mapEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.listBegin(field_type, container_size));
  EXPECT_EQ(FilterStatus::Continue, request_owner.listEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.setBegin(field_type, container_size));
  EXPECT_EQ(FilterStatus::Continue, request_owner.setEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.messageEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.transportEnd());

  EXPECT_CALL(connection, close(_));
  shadow_writer_ = nullptr;

  EXPECT_EQ(
      1UL,
      cluster_.cluster_.info_->statsScope().counterFromString("thrift.upstream_rq_call").value());
}

TEST_F(ShadowWriterTest, ShadowRequestWriteBeforePoolReady) {
  Tcp::ConnectionPool::Callbacks* callbacks;

  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(&cluster_));
  EXPECT_CALL(*cluster_.cluster_.info_, maintenanceMode()).WillOnce(Return(false));
  EXPECT_CALL(cluster_, tcpConnPool(_, _))
      .WillOnce(Return(Upstream::TcpPoolData([]() {}, &conn_pool_)));
  EXPECT_CALL(conn_pool_, newConnection(_))
      .WillOnce(
          Invoke([&](Tcp::ConnectionPool::Callbacks& cb) -> Tcp::ConnectionPool::Cancellable* {
            callbacks = &cb;
            return &cancellable_;
          }));

  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_NE(absl::nullopt, router_handle);

  // Write before connection is ready.
  auto& request_owner = router_handle.value().get().requestOwner();

  Buffer::OwnedImpl passthrough_data;
  FieldType field_type;
  FieldType key_type;
  FieldType value_type;
  int16_t field_id;
  bool bool_value;
  uint8_t byte_value;
  int16_t int16_value;
  int32_t int32_value;
  int64_t int64_value;
  double double_value;
  uint32_t container_size;

  EXPECT_EQ(FilterStatus::Continue, request_owner.transportBegin(nullptr));
  EXPECT_EQ(FilterStatus::Continue, request_owner.passthroughData(passthrough_data));
  EXPECT_EQ(FilterStatus::Continue, request_owner.structBegin(""));
  EXPECT_EQ(FilterStatus::Continue, request_owner.fieldBegin("", field_type, field_id));
  EXPECT_EQ(FilterStatus::Continue, request_owner.fieldEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.structEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.boolValue(bool_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.byteValue(byte_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.int16Value(int16_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.int32Value(int32_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.int64Value(int64_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.doubleValue(double_value));
  EXPECT_EQ(FilterStatus::Continue, request_owner.stringValue(""));
  EXPECT_EQ(FilterStatus::Continue, request_owner.mapBegin(key_type, value_type, container_size));
  EXPECT_EQ(FilterStatus::Continue, request_owner.mapEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.listBegin(field_type, container_size));
  EXPECT_EQ(FilterStatus::Continue, request_owner.listEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.setBegin(field_type, container_size));
  EXPECT_EQ(FilterStatus::Continue, request_owner.setEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.messageEnd());
  EXPECT_EQ(FilterStatus::Continue, request_owner.transportEnd());

  NiceMock<Network::MockClientConnection> connection;
  auto data = std::make_unique<NiceMock<Envoy::Tcp::ConnectionPool::MockConnectionData>>();
  EXPECT_CALL(*data, connection()).WillRepeatedly(ReturnRef(connection));
  EXPECT_CALL(*data, connectionState())
      .WillRepeatedly(
          Invoke([&]() -> Tcp::ConnectionPool::ConnectionState* { return conn_state_.get(); }));
  EXPECT_CALL(*data, setConnectionState_(_))
      .WillOnce(Invoke(
          [&](Tcp::ConnectionPool::ConnectionStatePtr& cs) -> void { conn_state_.swap(cs); }));

  EXPECT_CALL(connection, write(_, false));
  callbacks->onPoolReady(std::move(data), host_);

  EXPECT_CALL(connection, close(_));
  shadow_writer_ = nullptr;

  EXPECT_EQ(
      1UL,
      cluster_.cluster_.info_->statsScope().counterFromString("thrift.upstream_rq_call").value());
}

TEST_F(ShadowWriterTest, ShadowRequestPoolFailure) {
  EXPECT_CALL(cm_, getThreadLocalCluster(_)).WillOnce(Return(&cluster_));
  EXPECT_CALL(*cluster_.cluster_.info_, maintenanceMode()).WillOnce(Return(false));
  EXPECT_CALL(cluster_, tcpConnPool(_, _))
      .WillOnce(Return(Upstream::TcpPoolData([]() {}, &conn_pool_)));
  EXPECT_CALL(conn_pool_, newConnection(_))
      .WillOnce(Invoke([&](Tcp::ConnectionPool::Callbacks& callbacks)
                           -> Tcp::ConnectionPool::Cancellable* {
        auto data = std::make_unique<NiceMock<Envoy::Tcp::ConnectionPool::MockConnectionData>>();
        EXPECT_CALL(*data, connection()).Times(0);
        callbacks.onPoolFailure(ConnectionPool::PoolFailureReason::Overflow, "failure", nullptr);
        return nullptr;
      }));

  auto router_handle = shadow_writer_->submit("shadow_cluster", metadata_, TransportType::Framed,
                                              ProtocolType::Binary);
  EXPECT_NE(absl::nullopt, router_handle);
  router_handle.value().get().requestOwner().messageEnd();
}

TEST_F(ShadowWriterTest, ShadowRequestOnUpstreamDataReplySuccess) {
  testOnUpstreamData(MessageType::Reply, true);
}

TEST_F(ShadowWriterTest, ShadowRequestOnUpstreamDataReplyError) {
  testOnUpstreamData(MessageType::Reply, false);
}

TEST_F(ShadowWriterTest, ShadowRequestOnUpstreamDataReplyException) {
  testOnUpstreamData(MessageType::Reply, false);
}

TEST_F(ShadowWriterTest, ShadowRequestOnUpstreamDataAppException) {
  testOnUpstreamData(MessageType::Reply, false, true, false);
}

TEST_F(ShadowWriterTest, ShadowRequestOnUpstreamDataRegularException) {
  testOnUpstreamData(MessageType::Reply, false, false, true);
}

TEST_F(ShadowWriterTest, TestNullResponseDecoder) {
  auto transport_ptr =
      NamedTransportConfigFactory::getFactory(TransportType::Framed).createTransport();
  auto protocol_ptr = NamedProtocolConfigFactory::getFactory(ProtocolType::Binary).createProtocol();
  auto decoder_ptr = std::make_unique<NullResponseDecoder>(*transport_ptr, *protocol_ptr);

  decoder_ptr->newDecoderEventHandler();
  EXPECT_FALSE(decoder_ptr->passthroughEnabled());

  EXPECT_EQ(FilterStatus::Continue, decoder_ptr->messageBegin(metadata_));

  Buffer::OwnedImpl buffer;
  EXPECT_EQ(ThriftFilters::ResponseStatus::MoreData, decoder_ptr->upstreamData(buffer));

  EXPECT_EQ(FilterStatus::Continue, decoder_ptr->messageEnd());

  FieldType field_type;
  int16_t field_id;
  EXPECT_EQ(FilterStatus::Continue, decoder_ptr->fieldBegin("", field_type, field_id));

  EXPECT_EQ(FilterStatus::Continue, decoder_ptr->transportBegin(nullptr));
  EXPECT_EQ(FilterStatus::Continue, decoder_ptr->transportEnd());
}

} // namespace Router
} // namespace ThriftProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
