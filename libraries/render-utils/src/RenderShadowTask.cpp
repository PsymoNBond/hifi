//
//  RenderShadowTask.cpp
//  render-utils/src/
//
//  Created by Zach Pomerantz on 1/7/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <gpu/Context.h>

#include <ViewFrustum.h>

#include "render/Context.h"
#include "DeferredLightingEffect.h"
#include "FramebufferCache.h"

#include "RenderShadowTask.h"

#include "model_shadow_vert.h"
#include "skin_model_shadow_vert.h"

#include "model_shadow_frag.h"
#include "skin_model_shadow_frag.h"

using namespace render;

void RenderShadowMap::run(const render::SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext,
                          const render::ShapesIDsBounds& inShapes) {
    assert(renderContext->getArgs());
    assert(renderContext->getArgs()->_viewFrustum);

    const auto& lightStage = DependencyManager::get<DeferredLightingEffect>()->getLightStage();
    const auto globalLight = lightStage.lights[0];
    const auto& shadow = globalLight->shadow;
    const auto& fbo = shadow.framebuffer;

    RenderArgs* args = renderContext->getArgs();
    gpu::doInBatch(args->_context, [&](gpu::Batch& batch) {
        args->_batch = &batch;

        glm::ivec4 viewport{0, 0, fbo->getWidth(), fbo->getHeight()};
        batch.setViewportTransform(viewport);
        batch.setStateScissorRect(viewport);

        batch.setFramebuffer(fbo);
        batch.clearFramebuffer(
            gpu::Framebuffer::BUFFER_COLOR0 | gpu::Framebuffer::BUFFER_DEPTH,
            vec4(vec3(1.0, 1.0, 1.0), 1.0), 1.0, 0, true);

        batch.setProjectionTransform(shadow.getProjection());
        batch.setViewTransform(shadow.getView());

        auto shadowPipeline = _shapePlumber->pickPipeline(args, ShapeKey());
        auto shadowSkinnedPipeline = _shapePlumber->pickPipeline(args, ShapeKey::Builder().withSkinned());
        args->_pipeline = shadowPipeline;
        batch.setPipeline(shadowPipeline->pipeline);

        std::vector<ShapeKey> skinnedShapeKeys{};
        for (auto items : inShapes) {
            if (items.first.isSkinned()) {
                skinnedShapeKeys.push_back(items.first);
            } else {
                renderItems(sceneContext, renderContext, items.second);
            }
        }

        args->_pipeline = shadowSkinnedPipeline;
        batch.setPipeline(shadowSkinnedPipeline->pipeline);
        for (const auto& key : skinnedShapeKeys) {
            renderItems(sceneContext, renderContext, inShapes.at(key));
        }

        args->_pipeline = nullptr;
        args->_batch = nullptr;
    });
}

RenderShadowTask::RenderShadowTask() : Task() {
    // Prepare the ShapePipeline
    ShapePlumberPointer shapePlumber = std::make_shared<ShapePlumber>();
    {
        auto state = std::make_shared<gpu::State>();
        state->setCullMode(gpu::State::CULL_BACK);
        state->setDepthTest(true, true, gpu::LESS_EQUAL);

        auto modelVertex = gpu::Shader::createVertex(std::string(model_shadow_vert));
        auto modelPixel = gpu::Shader::createPixel(std::string(model_shadow_frag));
        gpu::ShaderPointer modelProgram = gpu::Shader::createProgram(modelVertex, modelPixel);
        shapePlumber->addPipeline(
            ShapeKey::Filter::Builder().withoutSkinned(),
            modelProgram, state);

        auto skinVertex = gpu::Shader::createVertex(std::string(skin_model_shadow_vert));
        auto skinPixel = gpu::Shader::createPixel(std::string(skin_model_shadow_frag));
        gpu::ShaderPointer skinProgram = gpu::Shader::createProgram(skinVertex, skinPixel);
        shapePlumber->addPipeline(
            ShapeKey::Filter::Builder().withSkinned(),
            skinProgram, state);
    }

    // CPU: Fetch shadow-casting opaques
    auto fetchedItems = addJob<FetchItems>("FetchShadowMap");

    // CPU: Cull against KeyLight frustum (nearby viewing camera)
    auto culledItems = addJob<CullItems<RenderDetails::SHADOW_ITEM>>("CullShadowMap", fetchedItems);

    // CPU: Sort by pipeline
    auto sortedShapes = addJob<PipelineSortShapes>("PipelineSortShadowSort", culledItems);

    // CPU: Sort front to back
    auto shadowShapes = addJob<DepthSortShapes>("DepthSortShadowMap", sortedShapes);

    // GPU: Render to shadow map
    addJob<RenderShadowMap>("RenderShadowMap", shadowShapes, shapePlumber);
}

void RenderShadowTask::run(const SceneContextPointer& sceneContext, const render::RenderContextPointer& renderContext) {
    assert(sceneContext);
    RenderArgs* args = renderContext->getArgs();

    // sanity checks
    if (!sceneContext->_scene || !args) {
        return;
    }

    const auto& lightStage = DependencyManager::get<DeferredLightingEffect>()->getLightStage();
    const auto globalLight = lightStage.lights[0];

    // If the global light is not set, bail
    if (!globalLight) {
        return;
    }

    ViewFrustum* viewFrustum = args->_viewFrustum;

    auto nearClip = viewFrustum->getNearClip();
    const int SHADOW_NEAR_DEPTH = -2;
    const int SHADOW_FAR_DEPTH = 20;
    globalLight->shadow.setKeylightFrustum(viewFrustum, nearClip + SHADOW_NEAR_DEPTH, nearClip + SHADOW_FAR_DEPTH);

    // Set the keylight frustum
    args->_viewFrustum = globalLight->shadow.getFrustum().get();

    for (auto job : _jobs) {
        job.run(sceneContext, renderContext);
    }

    // Reset the view frustum
    args->_viewFrustum = viewFrustum;
};
