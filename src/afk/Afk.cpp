#include "afk/Afk.hpp"

#include <memory>
#include <string>
#include <utility>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include "afk/asset/AssetFactory.hpp"
#include "afk/component/AgentComponent.hpp"
#include "afk/component/AnimComponent.hpp"
#include "afk/component/GameObject.hpp"
#include "afk/component/ScriptsComponent.hpp"
#include "afk/component/TagComponent.hpp"
#include "afk/debug/Assert.hpp"
#include "afk/io/Log.hpp"
#include "afk/io/ModelSource.hpp"
#include "afk/physics/PhysicsBody.hpp"
#include "afk/physics/RigidBodyType.hpp"
#include "afk/physics/shape/Box.hpp"
#include "afk/physics/shape/Sphere.hpp"
#include "afk/renderer/ModelRenderSystem.hpp"
#include "afk/script/Bindings.hpp"
#include "afk/script/LuaInclude.hpp"

using namespace std::string_literals;

using glm::vec3;
using glm::vec4;

using Afk::Engine;
using Afk::Event;
using Afk::Texture;
using Action   = Afk::Event::Action;
using Movement = Afk::Camera::Movement;

auto Engine::initialize() -> void {
  afk_assert(!this->is_initialized, "Engine already initialized");

  this->renderer.initialize();
  this->event_manager.initialize(this->renderer.window);
  //  this->renderer.set_wireframe(true);

  // load the navmesh before adding components to things
  // init crowds with the navmesh
  // then add components

  this->ui.initialize(this->renderer.window);
  this->lua = luaL_newstate();
  luaL_openlibs(this->lua);
  Afk::add_engine_bindings(this->lua);

  this->terrain_manager.initialize();
  const int terrain_width  = 128;
  const int terrain_length = 128;
  this->terrain_manager.generate_terrain(terrain_width, terrain_length, 0.05f, 7.5f);
  this->renderer.load_model(this->terrain_manager.get_model());

  auto terrain_entity           = registry.create();
  auto terrain_transform        = Transform{terrain_entity};
  terrain_transform.translation = glm::vec3{0.0f, -10.0f, 0.0f};
  registry.assign<Afk::ModelSource>(terrain_entity, terrain_entity,
                                    terrain_manager.get_model().file_path,
                                    "shader/terrain.prog");
  registry.assign<Afk::Model>(terrain_entity, terrain_entity, terrain_manager.get_model());
  registry.assign<Afk::Transform>(terrain_entity, terrain_transform);
  registry.assign<Afk::PhysicsBody>(terrain_entity, terrain_entity, &this->physics_body_system,
                                    terrain_transform, 0.3f, 0.0f, 0.0f, 0.0f,
                                    true, Afk::RigidBodyType::STATIC,
                                    this->terrain_manager.height_map);
  auto terrain_tags = TagComponent{terrain_entity};
  terrain_tags.tags.insert(TagComponent::Tag::TERRAIN);
  registry.assign<Afk::TagComponent>(terrain_entity, terrain_tags);

  auto box_entity           = registry.create();
  auto box_transform        = Transform{box_entity};
  box_transform.translation = glm::vec3{0.0f, -15.0f, 0.0f};
  box_transform.scale       = glm::vec3(5.0f);
  auto box_model            = Model(box_entity, "res/model/box/box.obj");
  this->renderer.load_model(box_model);
  registry.assign<Afk::ModelSource>(box_entity, box_entity, box_model.file_path,
                                    "shader/default.prog");
  registry.assign<Afk::Model>(box_entity, box_model);
  registry.assign<Afk::Transform>(box_entity, box_transform);
  registry.assign<Afk::PhysicsBody>(
      box_entity, box_entity, &this->physics_body_system, box_transform, 0.3f, 0.0f,
      0.0f, 0.0f, true, Afk::RigidBodyType::STATIC, Afk::Box(0.9f, 0.9f, 0.9f));
  auto box_tags = TagComponent{terrain_entity};
  box_tags.tags.insert(TagComponent::Tag::TERRAIN);
  registry.assign<Afk::TagComponent>(box_entity, box_tags);

  // auto basketball =
  // std::get<Asset::Asset::Object>(
  Afk::Asset::game_asset_factory("asset/basketball.lua"); //.data)
                                                          // .ent;

  // nav mesh needs to be generated BEFORE agents, otherwise agents may be added to the nav mesh
  this->nav_mesh_manager.initialise("res/gen/navmesh/human.nmesh");
  //  this->nav_mesh_manager.initialise("res/gen/navmesh/solo_navmesh.bin", this->terrain_manager.get_model().meshes[0], terrain_transform);
  this->crowds.init(this->nav_mesh_manager.get_nav_mesh());

  auto camera_transform        = Transform{camera_entity};
  camera_transform.translation = glm::vec3{0.0f, 50.0f, 0.0f};
  registry.assign<Afk::Transform>(camera_entity, camera_transform);
  registry.assign<Afk::PhysicsBody>(camera_entity, camera_entity, &this->physics_body_system,
                                    camera_transform, 0.0f, 0.3f, 0.3f, 100.0f, true,
                                    Afk::RigidBodyType::DYNAMIC, Afk::Sphere(0.75f));
  auto camera_tags = TagComponent{terrain_entity};
  camera_tags.tags.insert(TagComponent::Tag::PLAYER);
  registry.assign<Afk::TagComponent>(camera_entity, camera_tags);
  registry
      .assign<Afk::ScriptsComponent>(camera_entity, camera_entity, this->lua)
      .add_script("script/component/health.lua", &this->event_manager)
      .add_script("script/component/handle_collision_events.lua", &this->event_manager)
      .add_script("script/component/camera_keyboard_jetpack_control.lua", &this->event_manager)
      .add_script("script/component/camera_mouse_control.lua", &this->event_manager)
      .add_script("script/component/debug.lua", &this->event_manager);

  std::vector<entt::entity> agents{};
  for (std::size_t i = 0; i < 20; ++i) {
    agents.push_back(registry.create());
    dtCrowdAgentParams p        = {};
    p.radius                    = .1f;
    p.maxSpeed                  = 1;
    p.maxAcceleration           = 1;
    p.height                    = 1;
    auto agent_transform        = Afk::Transform{agents[i]};
    agent_transform.translation = {5 - (i / 10.f), -10, 5 - (i / 10.f)};
    agent_transform.scale       = {.1f, .1f, .1f};
    registry.assign<Afk::Transform>(agents[i], agent_transform);
    registry.assign<Afk::ModelSource>(agents[i], agents[i], "res/model/man/man.glb",
                                      "shader/animation.prog");
    registry.assign<Afk::AI::AgentComponent>(agents[i], agents[i],
                                             agent_transform.translation, p);
    registry.assign<Afk::PhysicsBody>(
        agents[i], agents[i], &this->physics_body_system, agent_transform, 0.3f, 0.0f,
        0.0f, 0.0f, true, Afk::RigidBodyType::STATIC, Afk::Capsule{0.3f, 1.0f});
    registry.assign<Afk::AnimComponent>(
        agents[i], agents[i], Afk::AnimComponent::Status::Playing, "Walk", 0.0f);
  }
  registry.get<Afk::AI::AgentComponent>(agents[0]).move_to({25, -5, 25});
  registry.get<Afk::AI::AgentComponent>(agents[1]).chase(camera_entity, 10.f);
  registry.get<Afk::AI::AgentComponent>(agents[2]).flee(camera_entity, 10.f);
  const Afk::AI::Path path = {{2.8f, -9.f, 3.f}, {14.f, -8.f, 4.f}, {20.f, -10.f, -3.5f}};
  registry.get<Afk::AI::AgentComponent>(agents[3]).path(path, 2.f);
  for (std::size_t ag = 4; ag < 20; ++ag) {
    registry.get<Afk::AI::AgentComponent>(agents[ag]).wander(glm::vec3{0.f, 0.f, 0.f}, 20.f, 5.f);
  }

  std::vector<GameObject> deathboxes = {};
  for (auto i = 0; i < 5; i++) {
    deathboxes.push_back(registry.create());
    auto deathbox_tags = TagComponent{deathboxes[i]};
    deathbox_tags.tags.insert(TagComponent::Tag::DEATHZONE);
    registry.assign<Afk::TagComponent>(deathboxes[i], deathbox_tags);
  }

  auto &deathbox_transform = registry.assign<Afk::Transform>(deathboxes[0], Transform{});
  deathbox_transform.translation  = glm::vec3{0.0f, -30.0f, 0.0f};
  deathbox_transform.scale       = glm::vec3(10000.0f, 0.1f, 10000.0f);
  registry.assign<Afk::PhysicsBody>(deathboxes[0], deathboxes[0], &this->physics_body_system,
                                    deathbox_transform, 0.0f, 0.0f, 0.0f, 0.0f,
                                    false, Afk::RigidBodyType::STATIC,
                                    Afk::Box(1.0f, 1.0f, 1.0f));

  deathbox_transform = registry.assign<Afk::Transform>(deathboxes[1], Transform{});
  deathbox_transform.translation  = glm::vec3{-((terrain_width+1)/2.0), 0.0f, 0.0f};
  deathbox_transform.scale       = glm::vec3(0.1f, 10000.0f, 10000.0f);
  registry.assign<Afk::PhysicsBody>(deathboxes[1], deathboxes[1], &this->physics_body_system,
                                    deathbox_transform, 0.0f, 0.0f, 0.0f, 0.0f,
                                    false, Afk::RigidBodyType::STATIC,
                                    Afk::Box(1.0f, 1.0f, 1.0f));

  deathbox_transform = registry.assign<Afk::Transform>(deathboxes[2], Transform{});
  deathbox_transform.translation  = glm::vec3{(terrain_width-1)/2.0, 0.0f, 0.0f};
  deathbox_transform.scale       = glm::vec3(0.1f, 10000.0f, 10000.0f);
  registry.assign<Afk::PhysicsBody>(deathboxes[2], deathboxes[2], &this->physics_body_system,
                                    deathbox_transform, 0.0f, 0.0f, 0.0f, 0.0f,
                                    false, Afk::RigidBodyType::STATIC,
                                    Afk::Box(1.0f, 1.0f, 1.0f));

  deathbox_transform = registry.assign<Afk::Transform>(deathboxes[3], Transform{});
  deathbox_transform.translation  = glm::vec3{0.0f, 0.0f, -((terrain_width+1)/2.0)};
  deathbox_transform.scale       = glm::vec3(10000.0f, 10000.0f, 0.1f);
  registry.assign<Afk::PhysicsBody>(deathboxes[3], deathboxes[3], &this->physics_body_system,
                                    deathbox_transform, 0.0f, 0.0f, 0.0f, 0.0f,
                                    false, Afk::RigidBodyType::STATIC,
                                    Afk::Box(1.0f, 1.0f, 1.0f));

  deathbox_transform = registry.assign<Afk::Transform>(deathboxes[4], Transform{});
  deathbox_transform.translation  = glm::vec3{0.0f, 0.0f, (terrain_width-1)/2.0};
  deathbox_transform.scale       = glm::vec3(10000.0f, 10000.0f, 0.1f);
  registry.assign<Afk::PhysicsBody>(deathboxes[4], deathboxes[4], &this->physics_body_system,
                                    deathbox_transform, 0.0f, 0.0f, 0.0f, 0.0f,
                                    false, Afk::RigidBodyType::STATIC,
                                    Afk::Box(1.0f, 1.0f, 1.0f));

  this->difficulty_manager.init(AI::DifficultyManager::Difficulty::NORMAL);

  this->is_initialized = true;
}

auto Engine::get() -> Engine & {
  static auto instance = Engine{};

  return instance;
}

auto Engine::exit() -> void {
  lua_close(this->lua);
  this->is_running = false;
}

auto Engine::render() -> void {
  Afk::queue_models(&this->registry, &this->renderer);

  this->renderer.clear_screen({135.0f, 206.0f, 235.0f, 1.0f});
  this->ui.prepare();
  this->renderer.draw();
  this->event_manager.pump_render();
  this->ui.draw();
  this->renderer.swap_buffers();
}

auto Engine::update() -> void {
  this->event_manager.pump_events();
  this->crowds.update(this->get_delta_time());
  for (auto &agent_ent : this->registry.view<Afk::AI::AgentComponent>()) {
    auto &agent = this->registry.get<Afk::AI::AgentComponent>(agent_ent);
    agent.update();
  }

  if (glfwWindowShouldClose(this->renderer.window)) {
    this->is_running = false;
  }

  if (this->ui.show_menu) {
    glfwSetInputMode(this->renderer.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  } else {
    glfwSetInputMode(this->renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }

  // this->update_camera();

  this->physics_body_system.update(&this->registry, this->get_delta_time());

  ++this->frame_count;
  this->last_update = Afk::Engine::get_time();
}

auto Engine::get_time() -> float {
  return static_cast<float>(glfwGetTime());
}

auto Engine::get_delta_time() -> float {
  return this->get_time() - this->last_update;
}

auto Engine::get_is_running() const -> bool {
  return this->is_running;
}
