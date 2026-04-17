#include <gtest/gtest.h>

#include "runtime/lb/round_robin.h"
#include "runtime/registry/service_registry.h"
#include "runtime/upstream/upstream.h"

using runtime::lb::RoundRobinLoadBalancer;
using runtime::registry::ServiceRegistry;
using runtime::upstream::Upstream;

// ---------------------------------------------------------------------------
// RoundRobinLoadBalancer tests
// ---------------------------------------------------------------------------

TEST(RoundRobinLbTest, AlternatesAcrossHealthyBackends) {
    Upstream us("user_service");
    us.AddBackend("127.0.0.1", 8081);
    us.AddBackend("127.0.0.1", 8082);

    RoundRobinLoadBalancer lb;

    auto* first  = lb.Select(us);
    auto* second = lb.Select(us);
    auto* third  = lb.Select(us);
    auto* fourth = lb.Select(us);

    ASSERT_NE(first,  nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->port,  8081);
    EXPECT_EQ(second->port, 8082);
    EXPECT_EQ(third->port,  8081);
    EXPECT_EQ(fourth->port, 8082);
}

TEST(RoundRobinLbTest, NeverSelectsUnhealthyBackend) {
    Upstream us("order_service");
    us.AddBackend("127.0.0.1", 9001);
    us.AddBackend("127.0.0.1", 9002);

    // Simulate health checker marking the first backend down.
    us.Backends()[0]->healthy.store(false, std::memory_order_relaxed);

    RoundRobinLoadBalancer lb;
    for (int i = 0; i < 8; ++i) {
        auto* selected = lb.Select(us);
        ASSERT_NE(selected, nullptr);
        EXPECT_EQ(selected->port, 9002)
            << "Unhealthy backend must never be selected (iteration " << i << ")";
    }
}

TEST(RoundRobinLbTest, ReturnsNullWhenAllUnhealthy) {
    Upstream us("down_service");
    us.AddBackend("127.0.0.1", 7001);
    us.Backends()[0]->healthy.store(false, std::memory_order_relaxed);

    RoundRobinLoadBalancer lb;
    EXPECT_EQ(lb.Select(us), nullptr);
}

TEST(RoundRobinLbTest, ReturnsNullForEmptyUpstream) {
    Upstream us("empty_service");
    RoundRobinLoadBalancer lb;
    EXPECT_EQ(lb.Select(us), nullptr);
}

TEST(RoundRobinLbTest, RecoveryAfterMarkingBackendHealthyAgain) {
    Upstream us("recovery_service");
    us.AddBackend("127.0.0.1", 5001);
    us.AddBackend("127.0.0.1", 5002);

    us.Backends()[1]->healthy.store(false, std::memory_order_relaxed);

    RoundRobinLoadBalancer lb;

    for (int i = 0; i < 4; ++i) {
        auto* s = lb.Select(us);
        ASSERT_NE(s, nullptr);
        EXPECT_EQ(s->port, 5001);
    }

    // Simulate recovery.
    us.Backends()[1]->healthy.store(true, std::memory_order_relaxed);

    bool saw_5001 = false, saw_5002 = false;
    for (int i = 0; i < 8; ++i) {
        auto* s = lb.Select(us);
        ASSERT_NE(s, nullptr);
        if (s->port == 5001) saw_5001 = true;
        if (s->port == 5002) saw_5002 = true;
    }
    EXPECT_TRUE(saw_5001);
    EXPECT_TRUE(saw_5002);
}

// ---------------------------------------------------------------------------
// ServiceRegistry tests
// ---------------------------------------------------------------------------

TEST(ServiceRegistryTest, RegisterAndGet) {
    ServiceRegistry reg;

    Upstream us("payment_service");
    us.AddBackend("10.0.0.1", 3000);
    us.AddBackend("10.0.0.2", 3000, 2);  // weight 2

    EXPECT_TRUE(reg.Register("payment_service", std::move(us)));

    auto* found = reg.Get("payment_service");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Name(), "payment_service");
    ASSERT_EQ(found->Backends().size(), 2u);
    EXPECT_EQ(found->Backends()[1]->weight, 2);
}

TEST(ServiceRegistryTest, RegisterReturnsFalseForEmptyName) {
    ServiceRegistry reg;
    Upstream us("svc");
    us.AddBackend("10.0.0.1", 80);
    EXPECT_FALSE(reg.Register("", std::move(us)));
    EXPECT_TRUE(reg.Empty());
}

TEST(ServiceRegistryTest, GetMissingServiceReturnsNull) {
    ServiceRegistry reg;
    EXPECT_EQ(reg.Get("nonexistent"), nullptr);
}

TEST(ServiceRegistryTest, HealthyCountExcludesUnhealthyBackends) {
    ServiceRegistry reg;

    Upstream us("cache_service");
    us.AddBackend("10.0.0.1", 6379);
    us.AddBackend("10.0.0.2", 6379);
    us.AddBackend("10.0.0.3", 6379);
    us.Backends()[1]->healthy.store(false, std::memory_order_relaxed);

    reg.Register("cache_service", std::move(us));

    auto* found = reg.Get("cache_service");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->HealthyCount(), 2);
}

TEST(ServiceRegistryTest, RegisterOverwritesPreviousEntry) {
    ServiceRegistry reg;

    Upstream v1("svc");
    v1.AddBackend("10.0.0.1", 80);
    reg.Register("svc", std::move(v1));

    Upstream v2("svc");
    v2.AddBackend("10.0.0.2", 80);
    v2.AddBackend("10.0.0.3", 80);
    reg.Register("svc", std::move(v2));

    auto* found = reg.Get("svc");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Backends().size(), 2u)
        << "Second Register() call should overwrite the first";
}

TEST(ServiceRegistryTest, BackendAddressFormat) {
    Upstream us("addr_test");
    us.AddBackend("192.168.1.10", 8080);
    EXPECT_EQ(us.Backends()[0]->Address(), "192.168.1.10:8080");
}

TEST(ServiceRegistryTest, SizeAndEmpty) {
    ServiceRegistry reg;
    EXPECT_TRUE(reg.Empty());
    EXPECT_EQ(reg.Size(), 0u);

    Upstream us("svc");
    us.AddBackend("127.0.0.1", 9000);
    reg.Register("svc", std::move(us));

    EXPECT_FALSE(reg.Empty());
    EXPECT_EQ(reg.Size(), 1u);
}
