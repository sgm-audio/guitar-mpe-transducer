#include <catch2/catch_test_macros.hpp>
#include <transducer/common/ring_buffer.h>
#include <transducer/common/double_buffer_seqlock.h>

TEST_CASE("SPSCRingBuffer basic push and pop", "[ring_buffer]") {
    transducer::common::SPSCRingBuffer<float, 64> rb;

    REQUIRE(rb.is_empty());
    REQUIRE(!rb.is_full());
    REQUIRE(rb.size() == 0);

    // Push 10 elements
    for (int i = 0; i < 10; ++i) {
        REQUIRE(rb.push(static_cast<float>(i)));
    }
    REQUIRE(rb.size() == 10);
    REQUIRE(!rb.is_empty());

    // Pop 10 elements and verify values
    for (int i = 0; i < 10; ++i) {
        float val = 0.0f;
        REQUIRE(rb.pop(val));
        REQUIRE(val == static_cast<float>(i));
    }
    REQUIRE(rb.is_empty());
}

TEST_CASE("DoubleBufferSeqLock snapshot consistency", "[seqlock]") {
    struct TestData {
        uint32_t a;
        float b;
    };

    transducer::common::DoubleBufferSeqLock<TestData> lock;
    TestData* write_slot = lock.begin_write();
    write_slot->a = 42;
    write_slot->b = 3.14159f;
    lock.end_write();

    TestData out{};
    REQUIRE(lock.read(out));
    REQUIRE(out.a == 42);
    REQUIRE(out.b == 3.14159f);
}
