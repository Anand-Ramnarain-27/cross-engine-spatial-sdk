#include <catch2/catch_test_macros.hpp>

#include "spatial/Error.h"

using namespace spatial;

TEST_CASE("Expected<T> holds a value on success", "[data][error]")
{
    Expected<int> e = 42;
    REQUIRE(e.hasValue());
    REQUIRE(static_cast<bool>(e));
    CHECK(e.value() == 42);
}

TEST_CASE("Expected<T> holds an error on failure", "[data][error]")
{
    Expected<int> e = Error{ErrorCode::CorruptTile, "bad data"};
    REQUIRE_FALSE(e.hasValue());
    REQUIRE_FALSE(static_cast<bool>(e));
    CHECK(e.error().code == ErrorCode::CorruptTile);
    CHECK(e.error().message == "bad data");
}

TEST_CASE("Expected<void> distinguishes success from failure", "[data][error]")
{
    Expected<void> ok;
    CHECK(ok.hasValue());

    Expected<void> failed = Error{ErrorCode::OutOfMemory, "no room"};
    CHECK_FALSE(failed.hasValue());
    CHECK(failed.error().code == ErrorCode::OutOfMemory);
}

TEST_CASE("ErrorCode::toString covers every enumerator", "[data][error]")
{
    CHECK(toString(ErrorCode::DatasetNotFound) == "DatasetNotFound");
    CHECK(toString(ErrorCode::InvalidDataset) == "InvalidDataset");
    CHECK(toString(ErrorCode::UnsupportedVersion) == "UnsupportedVersion");
    CHECK(toString(ErrorCode::TileLoadFailed) == "TileLoadFailed");
    CHECK(toString(ErrorCode::CorruptTile) == "CorruptTile");
    CHECK(toString(ErrorCode::OutOfMemory) == "OutOfMemory");
    CHECK(toString(ErrorCode::GPUUploadFailed) == "GPUUploadFailed");
    CHECK(toString(ErrorCode::InvalidState) == "InvalidState");
}
