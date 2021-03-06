#include "afk/renderer/ModelRenderSystem.hpp"

#include "afk/io/ModelSource.hpp"
#include "afk/physics/Transform.hpp"
#include "afk/renderer/Model.hpp"

auto Afk::queue_models(entt::registry *registry, Afk::Renderer *renderer) -> void {
  auto render_view = registry->view<Afk::Transform, Afk::ModelSource>();

  for (const auto &entity : render_view) {
    const auto &model_source_component = render_view.get<Afk::ModelSource>(entity);
    const auto &model_transform = render_view.get<Afk::Transform>(entity);
    renderer->queue_draw({model_source_component.name, model_source_component.shader_program_path,
                          model_transform, entity});
  }
}
