// TDD tests for Phase 1: Unified behavior data model.
// Entity::behaviors replaces separate Transform + scripts fields.

#include "scene.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Stubs for symbols pulled in by scene.cpp (Lua-free test harness).
lua_State *g_eng_lua_vm = nullptr;
void eng_behavior_release_script_self(lua_State *, ScriptInstance &) {}

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int test_spawn_includes_default_transform() {
    Scene scene;
    uint32_t id = scene.spawn("Bare");
    Entity *e = scene.entity(id);
    TEST_ASSERT(e != nullptr, "entity should exist");
    TEST_ASSERT(e->getTransform() != nullptr, "spawned entity should have Transform");
    TEST_ASSERT(e->behaviors.size() == 1, "spawned entity should have one behavior slot");
    TEST_ASSERT(e->behaviors[0].name == "Transform", "first behavior is Transform");
    return 0;
}

static int test_add_native_transform_behavior() {
    Scene scene;
    scene.insertEntityWithId(50, "WithTransform");
    uint32_t id = 50;
    TEST_ASSERT(scene.entity(id)->getTransform() == nullptr, "bare insert has no transform");
    scene.addBehavior(id, "Transform", true);

    Entity *e = scene.entity(id);
    TEST_ASSERT(e->behaviors.size() == 1, "should have 1 behavior");
    TEST_ASSERT(e->behaviors[0].name == "Transform", "name should be Transform");
    TEST_ASSERT(e->behaviors[0].isNative, "should be native");

    Transform *t = e->getTransform();
    TEST_ASSERT(t != nullptr, "getTransform should return non-null");
    TEST_ASSERT(t->x == 0.0f && t->y == 0.0f, "defaults x=0 y=0");
    TEST_ASSERT(t->sx == 1.0f && t->sy == 1.0f, "defaults sx=1 sy=1");
    return 0;
}

static int test_set_transform_field_through_behavior() {
    Scene scene;
    uint32_t id = scene.spawn("Test");

    scene.setTransformField(id, "x", 42.0f);
    scene.setTransformField(id, "y", -7.5f);
    scene.setTransformField(id, "angle", 90.0f);
    scene.setTransformField(id, "sx", 2.0f);

    Transform *t = scene.entity(id)->getTransform();
    TEST_ASSERT(t != nullptr, "transform should exist");
    TEST_ASSERT(std::fabs(t->x - 42.0f) < 0.001f, "x should be 42");
    TEST_ASSERT(std::fabs(t->y + 7.5f) < 0.001f, "y should be -7.5");
    TEST_ASSERT(std::fabs(t->angle - 90.0f) < 0.001f, "angle should be 90");
    TEST_ASSERT(std::fabs(t->sx - 2.0f) < 0.001f, "sx should be 2");
    return 0;
}

static int test_set_transform_field_on_entity_without_transform() {
    Scene scene;
    scene.insertEntityWithId(51, "NoTransform");
    uint32_t id = 51;
    // Should not crash; silently ignored.
    scene.setTransformField(id, "x", 100.0f);
    TEST_ASSERT(scene.entity(id)->getTransform() == nullptr, "still no transform");
    return 0;
}

static int test_behavior_list_ordering() {
    Scene scene;
    uint32_t id = scene.spawn("Multi");
    scene.addScript(id, "Ship");
    scene.addScript(id, "Collider");

    Entity *e = scene.entity(id);
    TEST_ASSERT(e->behaviors.size() == 3, "should have 3 behaviors");
    TEST_ASSERT(e->behaviors[0].name == "Transform", "first = Transform");
    TEST_ASSERT(e->behaviors[1].name == "Ship", "second = Ship");
    TEST_ASSERT(e->behaviors[2].name == "Collider", "third = Collider");
    return 0;
}

static int test_behavior_remove() {
    Scene scene;
    uint32_t id = scene.spawn("Remove");
    scene.addScript(id, "Ship");

    bool ok = scene.removeBehavior(id, 0);
    TEST_ASSERT(ok, "remove should succeed");

    Entity *e = scene.entity(id);
    TEST_ASSERT(e->behaviors.size() == 1, "should have 1 behavior left");
    TEST_ASSERT(e->behaviors[0].name == "Ship", "remaining should be Ship");
    TEST_ASSERT(e->getTransform() == nullptr, "transform should be gone");
    return 0;
}

static int test_behavior_reorder() {
    Scene scene;
    uint32_t id = scene.spawn("Reorder");
    scene.addScript(id, "Ship");
    scene.addScript(id, "Collider");

    bool ok = scene.reorderBehavior(id, 0, 2);
    TEST_ASSERT(ok, "reorder should succeed");

    Entity *e = scene.entity(id);
    TEST_ASSERT(e->behaviors[0].name == "Ship", "first = Ship");
    TEST_ASSERT(e->behaviors[1].name == "Collider", "second = Collider");
    TEST_ASSERT(e->behaviors[2].name == "Transform", "third = Transform");
    TEST_ASSERT(e->getTransform() != nullptr, "transform still accessible");
    return 0;
}

static int test_world_matrices_use_behavior_transform() {
    Scene scene;
    uint32_t parent = scene.spawn("Parent");
    scene.setTransformField(parent, "x", 100.0f);

    uint32_t child = scene.spawn("Child");
    scene.setTransformField(child, "x", 20.0f);
    scene.setParent(child, parent);

    auto wm = scene.computeWorldMatrices();
    TEST_ASSERT(wm.count(child) == 1, "child should have world matrix");
    float wx = wm[child].tx;
    TEST_ASSERT(std::fabs(wx - 120.0f) < 0.01f,
                "child world x should be parent(100) + local(20) = 120");
    return 0;
}

static int test_world_matrices_entity_without_transform() {
    Scene scene;
    scene.insertEntityWithId(60, "Folder");
    uint32_t id = 60;
    // No transform added — should produce identity matrix.
    auto wm = scene.computeWorldMatrices();
    TEST_ASSERT(wm.count(id) == 1, "entity should still have a world matrix entry");
    TEST_ASSERT(std::fabs(wm[id].tx) < 0.001f, "identity tx = 0");
    TEST_ASSERT(std::fabs(wm[id].ty) < 0.001f, "identity ty = 0");
    return 0;
}

static int test_add_script_backward_compat() {
    Scene scene;
    uint32_t id = scene.spawn("Compat");
    scene.addScript(id, "Ship");

    Entity *e = scene.entity(id);
    TEST_ASSERT(e->behaviors.size() == 2, "should have 2 behaviors");
    TEST_ASSERT(e->behaviors[1].name == "Ship", "script name = Ship");
    TEST_ASSERT(!e->behaviors[1].isNative, "script should not be native");
    return 0;
}

static int test_get_script_behaviors_only() {
    Scene scene;
    uint32_t id = scene.spawn("Mixed");
    scene.addScript(id, "Ship");
    scene.addScript(id, "Collider");

    Entity *e = scene.entity(id);
    int scriptCount = 0;
    for (auto &b : e->behaviors)
        if (!b.isNative) scriptCount++;
    TEST_ASSERT(scriptCount == 2, "should have 2 script behaviors");
    return 0;
}

int main() {
    int failures = 0;

#define RUN(fn) do { if (fn()) { failures++; std::fprintf(stderr, "  in " #fn "\n"); } } while(0)

    RUN(test_spawn_includes_default_transform);
    RUN(test_add_native_transform_behavior);
    RUN(test_set_transform_field_through_behavior);
    RUN(test_set_transform_field_on_entity_without_transform);
    RUN(test_behavior_list_ordering);
    RUN(test_behavior_remove);
    RUN(test_behavior_reorder);
    RUN(test_world_matrices_use_behavior_transform);
    RUN(test_world_matrices_entity_without_transform);
    RUN(test_add_script_backward_compat);
    RUN(test_get_script_behaviors_only);

#undef RUN

    if (failures == 0)
        std::printf("All behavior model tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) failed.\n", failures);
    return failures;
}
