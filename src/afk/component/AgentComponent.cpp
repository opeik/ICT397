#include "AgentComponent.hpp"

#include "afk/Afk.hpp"
#include "afk/ai/behaviour/Chase.hpp"

using Afk::AI::AgentComponent;

AgentComponent::AgentComponent(GameObject e, const glm::vec3 &pos, dtCrowdAgentParams &p) {
  this->owning_entity = e;

  auto nearest_pos = Afk::Engine::get().crowds.nearest_pos(pos, 10.f);
  if (nearest_pos.has_value()) {
    this->id = Afk::Engine::get().crowds.current_crowd().addAgent(&nearest_pos->x, &p);
  } else {
    throw std::runtime_error{"Could not find position for agent"};
  }
}
AgentComponent::~AgentComponent() {
  Afk::Engine::get().crowds.current_crowd().removeAgent(this->id);
}

auto AgentComponent::chase(const GameObject &target) -> void {
  this->current_behaviour = Behaviour{new Afk::AI::Chase{target, 500.f}};
}

auto AgentComponent::update() -> void {
  if (this->current_behaviour == nullptr) {
    return;
  }
  auto &tf = Afk::Engine::get().registry.get<Afk::Transform>(this->owning_entity);
  auto agent = Afk::Engine::get().crowds.current_crowd().getAgent(this->id);
  if (!agent->active) {
    throw new std::runtime_error{"Agent not active!"};
  }
  // Update our position to the current agent pos
  tf.translation.x = agent->npos[0];
  tf.translation.y = agent->npos[1];
  tf.translation.z = agent->npos[2];
  // Update the agent goal
  auto next_pos = current_behaviour->update(tf.translation);
  Afk::Engine::get().crowds.request_move(this->id, next_pos);
}
