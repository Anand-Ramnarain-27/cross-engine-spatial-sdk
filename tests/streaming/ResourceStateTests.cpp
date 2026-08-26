#include <catch2/catch_test_macros.hpp>

#include "spatial/streaming/ResourceState.h"

using namespace spatial::streaming;

TEST_CASE("ResourceState follows the documented happy-path chain", "[streaming][state]")
{
    CHECK(isValidTransition(ResourceState::Unloaded, ResourceState::Requested));
    CHECK(isValidTransition(ResourceState::Requested, ResourceState::Loading));
    CHECK(isValidTransition(ResourceState::Loading, ResourceState::LoadedCPU));
    CHECK(isValidTransition(ResourceState::LoadedCPU, ResourceState::UploadPending));
    CHECK(isValidTransition(ResourceState::UploadPending, ResourceState::Resident));
    CHECK(isValidTransition(ResourceState::Resident, ResourceState::UnloadRequested));
    CHECK(isValidTransition(ResourceState::UnloadRequested, ResourceState::Unloading));
    CHECK(isValidTransition(ResourceState::Unloading, ResourceState::Unloaded));
}

TEST_CASE("ResourceState allows cancellation/failure back to Unloaded", "[streaming][state]")
{
    CHECK(isValidTransition(ResourceState::Requested, ResourceState::Unloaded));
    CHECK(isValidTransition(ResourceState::Loading, ResourceState::Unloaded));
    CHECK(isValidTransition(ResourceState::UploadPending, ResourceState::Unloaded));
}

TEST_CASE("ResourceState rejects skipping states", "[streaming][state]")
{
    CHECK_FALSE(isValidTransition(ResourceState::Unloaded, ResourceState::Loading));
    CHECK_FALSE(isValidTransition(ResourceState::Unloaded, ResourceState::Resident));
    CHECK_FALSE(isValidTransition(ResourceState::Requested, ResourceState::LoadedCPU));
    CHECK_FALSE(isValidTransition(ResourceState::Loading, ResourceState::Resident));
    CHECK_FALSE(isValidTransition(ResourceState::Resident, ResourceState::Unloaded));
    CHECK_FALSE(isValidTransition(ResourceState::Resident, ResourceState::Unloading));
}

TEST_CASE("ResourceState rejects moving backward", "[streaming][state]")
{
    CHECK_FALSE(isValidTransition(ResourceState::Loading, ResourceState::Requested));
    CHECK_FALSE(isValidTransition(ResourceState::Resident, ResourceState::LoadedCPU));
    CHECK_FALSE(isValidTransition(ResourceState::Unloading, ResourceState::Resident));
}

TEST_CASE("ResourceState::toString covers every enumerator", "[streaming][state]")
{
    CHECK(toString(ResourceState::Unloaded) == "Unloaded");
    CHECK(toString(ResourceState::Requested) == "Requested");
    CHECK(toString(ResourceState::Loading) == "Loading");
    CHECK(toString(ResourceState::LoadedCPU) == "LoadedCPU");
    CHECK(toString(ResourceState::UploadPending) == "UploadPending");
    CHECK(toString(ResourceState::Resident) == "Resident");
    CHECK(toString(ResourceState::UnloadRequested) == "UnloadRequested");
    CHECK(toString(ResourceState::Unloading) == "Unloading");
}
