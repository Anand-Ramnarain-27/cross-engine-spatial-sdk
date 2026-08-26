#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "FlyCamera.h"

using namespace spatial::core;
using namespace viewer;
using Catch::Approx;

TEST_CASE("FlyCamera faces -Z with zero right/up drift at yaw=0, pitch=0", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.yawRadians = 0.0f;
    camera.pitchRadians = 0.0f;

    CHECK(nearlyEqual(camera.forward(), Vec3{0, 0, -1}));
    CHECK(nearlyEqual(camera.right(), Vec3{1, 0, 0}));
}

TEST_CASE("FlyCamera::look moves the mouse up to increase pitch (look up)", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.pitchRadians = 0.0f;
    camera.mouseSensitivity = 0.01f;

    camera.look(0.0f, -10.0f); // moving the mouse up produces a negative dy
    CHECK(camera.pitchRadians > 0.0f);
    CHECK(camera.forward().y > 0.0f); // looking upward
}

TEST_CASE("FlyCamera::look moves the mouse right to increase yaw", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.yawRadians = 0.0f;
    camera.mouseSensitivity = 0.01f;

    camera.look(10.0f, 0.0f);
    CHECK(camera.yawRadians > 0.0f);
}

TEST_CASE("FlyCamera::look clamps pitch short of straight up/down", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.pitchRadians = 0.0f;
    camera.mouseSensitivity = 1.0f;

    for (int i = 0; i < 100; ++i)
    {
        camera.look(0.0f, -1000.0f); // repeatedly try to look far past straight up
    }

    CHECK(camera.pitchRadians < std::numbers::pi_v<float> / 2.0f);
    CHECK(camera.pitchRadians > 0.0f);
}

TEST_CASE("FlyCamera::move translates along forward for local +Z", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.position = Vec3{0, 0, 0};
    camera.yawRadians = 0.0f;
    camera.pitchRadians = 0.0f;
    camera.moveSpeed = 10.0f;

    camera.move(Vec3{0, 0, 1}, 1.0f); // 1 second forward at 10 units/sec
    CHECK(nearlyEqual(camera.position, Vec3{0, 0, -10}, 1e-3f));
}

TEST_CASE("FlyCamera::move translates along right for local +X", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.position = Vec3{0, 0, 0};
    camera.yawRadians = 0.0f;
    camera.pitchRadians = 0.0f;
    camera.moveSpeed = 5.0f;

    camera.move(Vec3{1, 0, 0}, 2.0f); // 2 seconds right at 5 units/sec
    CHECK(nearlyEqual(camera.position, Vec3{10, 0, 0}, 1e-3f));
}

TEST_CASE("FlyCamera::move translates along world up for local +Y", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.position = Vec3{0, 0, 0};
    camera.moveSpeed = 3.0f;

    camera.move(Vec3{0, 1, 0}, 1.0f);
    CHECK(nearlyEqual(camera.position, Vec3{0, 3, 0}, 1e-3f));
}

TEST_CASE("FlyCamera::viewMatrix maps the eye position to the view-space origin", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.position = Vec3{5, 10, 20};
    camera.yawRadians = 0.4f;
    camera.pitchRadians = -0.2f;

    const Mat4 view = camera.viewMatrix();
    CHECK(nearlyEqual(view.transformPoint(camera.position), Vec3{0, 0, 0}, 1e-3f));
}

TEST_CASE("FlyCamera::toCameraParams carries position, forward, fov, and viewport height through", "[examples][flycamera]")
{
    FlyCamera camera;
    camera.position = Vec3{1, 2, 3};
    camera.yawRadians = 0.0f;
    camera.pitchRadians = 0.0f;

    const CameraParams params = camera.toCameraParams(900.0f, 1.2f);
    CHECK(nearlyEqual(params.position, Vec3{1, 2, 3}));
    CHECK(nearlyEqual(params.forward, camera.forward()));
    CHECK(params.viewportHeightPx == Approx(900.0f));
    CHECK(params.verticalFovRadians == Approx(1.2f));
}
