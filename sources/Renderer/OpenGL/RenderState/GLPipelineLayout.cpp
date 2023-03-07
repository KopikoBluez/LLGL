/*
 * GLPipelineLayout.cpp
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015-2019 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "GLPipelineLayout.h"
#include "../RenderState/GLStateManager.h"
#include "../Ext/GLExtensionRegistry.h"
#include "../../../Core/Helper.h"
#include <LLGL/Misc/ForRange.h>
#include <algorithm>


namespace LLGL
{


template <typename TContainer>
bool HasAnyNamedEntries(const TContainer& container)
{
    for (const auto& entry : container)
    {
        if (!entry.name.empty())
            return true;
    }
    return false;
}

// Returns true if the specified pipeline layout descriptor contains any names for heap and dynamic resources.
static bool HasAnyNamedResourceBindings(const PipelineLayoutDescriptor& desc)
{
    return
    (
        HasAnyNamedEntries(desc.heapBindings)   ||
        HasAnyNamedEntries(desc.bindings)       ||
        HasAnyNamedEntries(desc.staticSamplers)
    );
}

GLPipelineLayout::GLPipelineLayout(const PipelineLayoutDescriptor& desc) :
    heapBindings_     { desc.heapBindings                 },
    uniforms_         { desc.uniforms                     },
    hasNamedBindings_ { HasAnyNamedResourceBindings(desc) }
{
    resourceNames_.reserve(desc.bindings.size() + desc.staticSamplers.size());
    BuildDynamicResourceBindings(desc.bindings);
    BuildStaticSamplers(desc.staticSamplers);
}

std::uint32_t GLPipelineLayout::GetNumHeapBindings() const
{
    return static_cast<std::uint32_t>(heapBindings_.size());
}

std::uint32_t GLPipelineLayout::GetNumBindings() const
{
    return static_cast<std::uint32_t>(bindings_.size());
}

std::uint32_t GLPipelineLayout::GetNumStaticSamplers() const
{
    #ifdef LLGL_GL_ENABLE_OPENGL2X
    return static_cast<std::uint32_t>(std::max(staticSamplers_.size(), staticSamplersGL2X_.size()));
    #else
    return static_cast<std::uint32_t>(staticSamplers_.size());
    #endif
}

std::uint32_t GLPipelineLayout::GetNumUniforms() const
{
    return static_cast<std::uint32_t>(uniforms_.size());
}

void GLPipelineLayout::BindStaticSamplers(GLStateManager& stateMngr) const
{
    if (!staticSamplerSlots_.empty())
    {
        #ifdef LLGL_GL_ENABLE_OPENGL2X
        if (!HasNativeSamplers())
        {
            for_range(i, staticSamplerSlots_.size())
                stateMngr.BindGL2XSampler(staticSamplerSlots_[i], *staticSamplersGL2X_[i]);
        }
        else
        #endif
        {
            for_range(i, staticSamplerSlots_.size())
                stateMngr.BindSampler(staticSamplerSlots_[i], staticSamplers_[i]->GetID());
        }
    }
}


/*
 * ======= Private: =======
 */

static GLResourceType ToGLResourceType(const BindingDescriptor& desc)
{
    switch (desc.type)
    {
        case ResourceType::Buffer:
            if ((desc.bindFlags & BindFlags::ConstantBuffer) != 0)
                return GLResourceType_UBO;
            if ((desc.bindFlags & (BindFlags::Sampled | BindFlags::Storage)) != 0)
                return GLResourceType_SSBO;
            break;

        case ResourceType::Texture:
            if ((desc.bindFlags & BindFlags::Sampled) != 0)
                return GLResourceType_Texture;
            if ((desc.bindFlags & BindFlags::Storage) != 0)
                return GLResourceType_Image;
            break;

        case ResourceType::Sampler:
            #ifdef LLGL_GL_ENABLE_OPENGL2X
            if (!HasNativeSamplers())
                return GLResourceType_GL2XSampler;
            #endif
            return GLResourceType_Sampler;

        default:
            break;
    }
    return GLResourceType_Invalid;
}

void GLPipelineLayout::BuildDynamicResourceBindings(const std::vector<BindingDescriptor>& bindings)
{
    bindings_.reserve(bindings.size());
    for (const auto& desc : bindings)
    {
        bindings_.push_back(GLPipelineResourceBinding{ ToGLResourceType(desc), static_cast<GLuint>(desc.slot) });
        resourceNames_.push_back(desc.name);
    }
}

void GLPipelineLayout::BuildStaticSamplers(const std::vector<StaticSamplerDescriptor>& staticSamplers)
{
    staticSamplerSlots_.reserve(staticSamplers.size());
    #ifdef LLGL_GL_ENABLE_OPENGL2X
    if (!HasNativeSamplers())
    {
        staticSamplersGL2X_.reserve(staticSamplers.size());
        for (const auto& desc : staticSamplers)
        {
            /* Create GL2.x sampler and store slot and name separately */
            auto sampler = MakeUnique<GL2XSampler>();
            sampler->SetDesc(desc.sampler);
            staticSamplersGL2X_.push_back(std::move(sampler));
            staticSamplerSlots_.push_back(desc.slot);
            resourceNames_.push_back(desc.name);
        }
    }
    else
    #endif
    {
        staticSamplers_.reserve(staticSamplers.size());
        for (const auto& desc : staticSamplers)
        {
            /* Create GL3+ sampler and store slot and name separately */
            auto sampler = MakeUnique<GLSampler>();
            sampler->SetDesc(desc.sampler);
            staticSamplers_.push_back(std::move(sampler));
            staticSamplerSlots_.push_back(desc.slot);
            resourceNames_.push_back(desc.name);
        }
    }
}


} // /namespace LLGL



// ================================================================================