// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Precompiled.h"
#include "Render/Render.h"
#include "RenderInternal.h"
#include "Core/JointPose.h"
#include "RBackEnd.h"

BE_NAMESPACE_BEGIN

void Batch::DrawPrimitives() const {
    rhi.BindBuffer(RHI::IndexBuffer, indexBuffer);

    if (numIndirectCommands > 0) {
        rhi.MultiDrawElementsIndirect(RHI::TrianglesPrim, sizeof(TriIndex), 0, numIndirectCommands, sizeof(RHI::DrawElementsIndirectCommand));
    } else if (numInstances > 0) {
        rhi.DrawElementsInstanced(RHI::TrianglesPrim, startIndex, r_singleTriangle.GetBool() ? 3 : numIndexes, sizeof(TriIndex), 0, numInstances);
    } else {
        rhi.DrawElements(RHI::TrianglesPrim, startIndex, r_singleTriangle.GetBool() ? 3 : numIndexes, sizeof(TriIndex), 0);
    }

    int instanceCount = Max(numInstances, 1);

    if (flushType == ShadowFlush) {
        backEnd.ctx->renderCounter.shadowDrawCalls++;
        backEnd.ctx->renderCounter.shadowDrawIndexes += numIndexes * instanceCount;
        backEnd.ctx->renderCounter.shadowDrawVerts += numVerts * instanceCount;
    }

    backEnd.ctx->renderCounter.drawCalls++;
    backEnd.ctx->renderCounter.drawIndexes += numIndexes * instanceCount;
    backEnd.ctx->renderCounter.drawVerts += numVerts * instanceCount;
}

void Batch::SetShaderProperties(const Shader *shader, const StrHashMap<Shader::Property> &shaderProperties) const {
    const auto &propertyInfoHashMap = shader->GetPropertyInfoHashMap();

    // Iterate over all shader property specs
    for (int i = 0; i < propertyInfoHashMap.Count(); i++) {
        const auto *entry = propertyInfoHashMap.GetByIndex(i);
        const auto &key = entry->first;
        const auto &propInfo = entry->second;

        // Skip if it is a shader define
        if (propInfo.GetFlags() & PropertyInfo::ShaderDefineFlag) {
            continue;
        }

        // Skip if not exist in shader properties
        const auto *propEntry = shaderProperties.Get(key);
        if (!propEntry) {
            continue;
        }

        const Shader::Property &prop = propEntry->second;

        switch (propInfo.GetType()) {
        case Variant::IntType:
            shader->SetConstant1i(key, prop.data.As<int>());
            break;
        case Variant::PointType:
            shader->SetConstant2i(key, prop.data.As<Point>());
            break;
        case Variant::RectType:
            shader->SetConstant4i(key, prop.data.As<Rect>());
            break;
        case Variant::FloatType:
            shader->SetConstant1f(key, prop.data.As<float>());
            break;
        case Variant::Vec2Type:
            shader->SetConstant2f(key, prop.data.As<Vec2>());
            break;
        case Variant::Vec3Type:
            shader->SetConstant3f(key, prop.data.As<Vec3>());
            break;
        case Variant::Vec4Type:
            shader->SetConstant4f(key, prop.data.As<Vec4>());
            break;
        case Variant::Color3Type:
            shader->SetConstant3f(key, rhi.IsSRGBWriteEnabled() ? prop.data.As<Color3>().SRGBToLinear() : prop.data.As<Color3>());
            break;
        case Variant::Color4Type:
            shader->SetConstant4f(key, rhi.IsSRGBWriteEnabled() ? prop.data.As<Color4>().SRGBToLinear() : prop.data.As<Color4>());
            break;
        case Variant::Mat2Type:
            shader->SetConstant2x2f(key, true, prop.data.As<Mat2>());
            break;
        case Variant::Mat3Type:
            shader->SetConstant3x3f(key, true, prop.data.As<Mat3>());
            break;
        case Variant::Mat4Type:
            shader->SetConstant4x4f(key, true, prop.data.As<Mat4>());
            break;
        case Variant::GuidType: // 
            shader->SetTexture(key, prop.texture);
            break;
        default:
            assert(0);
            break;
        }
    }
}

const Texture *Batch::TextureFromShaderProperties(const Material::ShaderPass *mtrlPass, const Str &textureName) const {
    const auto *entry = mtrlPass->shader->GetPropertyInfoHashMap().Get(textureName);
    if (!entry) {
        return nullptr;
    }

    const auto &propInfo = entry->second;
    if ((propInfo.GetFlags() & PropertyInfo::ShaderDefineFlag) || (propInfo.GetType() != Variant::GuidType)) {
        return nullptr;
    }

    const Shader::Property &prop = mtrlPass->shaderProperties.Get(textureName)->second;
    return prop.texture;
}

void Batch::SetMatrixConstants(const Shader *shader) const {
    if (shader->builtInConstantIndices[Shader::ModelViewMatrixConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ModelViewMatrixConst], true, backEnd.modelViewMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ViewMatrixConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ViewMatrixConst], true, backEnd.viewMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ProjectionMatrixConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ProjectionMatrixConst], true, backEnd.projMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ViewProjectionMatrixConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ViewProjectionMatrixConst], true, backEnd.viewProjMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ModelViewProjectionMatrixConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ModelViewProjectionMatrixConst], true, backEnd.modelViewProjMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ModelViewMatrixTransposeConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ModelViewMatrixTransposeConst], false, backEnd.modelViewMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ProjectionMatrixTransposeConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ProjectionMatrixTransposeConst], false, backEnd.projMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ViewMatrixTransposeConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ViewMatrixTransposeConst], false, backEnd.viewMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ViewProjectionMatrixTransposeConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ViewProjectionMatrixTransposeConst], false, backEnd.viewProjMatrix);
    }

    if (shader->builtInConstantIndices[Shader::ModelViewProjectionMatrixTransposeConst] >= 0) {
        shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ModelViewProjectionMatrixTransposeConst], false, backEnd.modelViewProjMatrix);
    }
}

void Batch::SetVertexColorConstants(const Shader *shader, const Material::VertexColorMode &vertexColor) const {
    Vec4 vertexColorScale;
    Vec4 vertexColorAdd;

    if (vertexColor == Material::ModulateVertexColor) {
        vertexColorScale.Set(1.0f, 1.0f, 1.0f, 1.0f);
        vertexColorAdd.Set(0.0f, 0.0f, 0.0f, 0.0f);
    } else if (vertexColor == Material::InverseModulateVertexColor) {
        vertexColorScale.Set(-1.0f, -1.0f, -1.0f, 1.0f);
        vertexColorAdd.Set(1.0f, 1.0f, 1.0f, 0.0f);
    } else {
        vertexColorScale.Set(0.0f, 0.0f, 0.0f, 0.0f);
        vertexColorAdd.Set(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    shader->SetConstant4f(shader->builtInConstantIndices[Shader::VertexColorScaleConst], vertexColorScale);
    shader->SetConstant4f(shader->builtInConstantIndices[Shader::VertexColorAddConst], vertexColorAdd);
}

void Batch::SetSkinningConstants(const Shader *shader, const SkinningJointCache *cache) const {
    if (renderGlobal.skinningMethod == SkinningJointCache::CpuSkinning) {
        return;
    }

    if (renderGlobal.skinningMethod == SkinningJointCache::VertexShaderSkinning) {
        shader->SetConstantArray4f(shader->builtInConstantIndices[Shader::JointsConst], cache->numJoints * 3, cache->skinningJoints[0].Ptr());
    } else if (renderGlobal.skinningMethod == SkinningJointCache::VertexTextureFetchSkinning) {
        const Texture *jointsMapTexture = cache->bufferCache.texture;

        shader->SetTexture(shader->builtInSamplerUnits[Shader::JointsMapSampler], jointsMapTexture);

        if (renderGlobal.vtUpdateMethod == BufferCacheManager::TboUpdate) {
            if (numInstances == 0) {
                shader->SetConstant1i(shader->builtInConstantIndices[Shader::SkinningBaseTcConst], cache->bufferCache.tcBase[0]);
            }
        } else {
            shader->SetConstant2f(shader->builtInConstantIndices[Shader::InvJointsMapSizeConst], Vec2(1.0f / jointsMapTexture->GetWidth(), 1.0f / jointsMapTexture->GetHeight()));

            if (numInstances == 0) {
                shader->SetConstant2f(shader->builtInConstantIndices[Shader::SkinningBaseTcConst], Vec2(cache->bufferCache.tcBase[0], cache->bufferCache.tcBase[1]));
            }
        }

        if (r_usePostProcessing.GetBool() && (r_motionBlur.GetInteger() & 2)) {
            shader->SetConstant2i(shader->builtInConstantIndices[Shader::JointIndexOffsetConst], cache->jointIndexOffset);
        }
    }
}

void Batch::SetEntityConstants(const Material::ShaderPass *mtrlPass, const Shader *shader) const {
    if (subMesh->useGpuSkinning) {
        SetSkinningConstants(shader, surfSpace->def->GetState().mesh->skinningJointCache);
    }

    if (numIndirectCommands > 0) {
        rhi.BindBuffer(RHI::DrawIndirectBuffer, indirectBuffer);
        rhi.BufferDiscardWrite(indirectBuffer, numIndirectCommands * sizeof(indirectCommands[0]), indirectCommands);
    } else if (numInstances > 0) {
        int bufferOffset = backEnd.instanceBufferCache->offset + instanceStartIndex * rhi.HWLimit().uniformBufferOffsetAlignment;
        int bufferSize = (instanceEndIndex - instanceStartIndex + 1) * rhi.HWLimit().uniformBufferOffsetAlignment;

        // 0-indexed buffer for instance buffer
        rhi.BindIndexedBufferRange(RHI::UniformBuffer, 0, backEnd.instanceBufferCache->buffer, bufferOffset, bufferSize);
        shader->SetConstantBuffer("instanceDataBuffer", 0);

        shader->SetConstantArray1i(shader->builtInConstantIndices[Shader::InstanceIndexesConst], numInstances, instanceLocalIndexes);
    } else {
        if (shader->builtInConstantIndices[Shader::LocalToWorldMatrixConst] >= 0) {
            const Mat3x4 &localToWorldMatrix = surfSpace->def->GetObjectToWorldMatrix();
            shader->SetConstant4x3f(shader->builtInConstantIndices[Shader::LocalToWorldMatrixConst], true, localToWorldMatrix);
        }

        if (shader->builtInConstantIndices[Shader::WorldToLocalMatrixConst] >= 0) {
            Mat3 worldToLocalMatrix = surfSpace->def->GetState().axis.Transpose();
            shader->SetConstant3x3f(shader->builtInConstantIndices[Shader::WorldToLocalMatrixConst], false, worldToLocalMatrix);
        }

        if (shader->builtInConstantIndices[Shader::ConstantColorConst] >= 0) {
            const Color4 &color = mtrlPass->useOwnerColor ? reinterpret_cast<const Color4 &>(surfSpace->def->GetState().materialParms[RenderObject::RedParm]) : mtrlPass->constantColor;
            shader->SetConstant4f(shader->builtInConstantIndices[Shader::ConstantColorConst], color);
        }
    }
}

void Batch::SetProbeConstants(const Shader *shader) const {
    if (surfSpace->envProbeInfo[0].envProbe) {
        const EnvProbeBlendInfo &probe0 = surfSpace->envProbeInfo[0];

        Vec3 probe0Extent = probe0.envProbe->GetWorldAABB().Extents();

        shader->SetTexture("probe0DiffuseCubeMap", probe0.envProbe->GetDiffuseProbeTexture());
        shader->SetTexture("probe0SpecularCubeMap", probe0.envProbe->GetSpecularProbeTexture());
        shader->SetConstant1f("probe0SpecularCubeMapMaxMipLevel", Math::Log(2.0f, probe0.envProbe->GetSpecularProbeTexture()->GetWidth()));
        shader->SetConstant4f("probe0Position", Vec4(probe0.envProbe->GetBoxCenter(), probe0.envProbe->IsBoxProjection() ? 1.0f : 0.0f));
        shader->SetConstant3f("probe0Mins", -probe0Extent);
        shader->SetConstant3f("probe0Maxs", +probe0Extent);
        shader->SetConstant1f("probeLerp", probe0.weight);
    }

    if (surfSpace->envProbeInfo[1].envProbe) {
        const EnvProbeBlendInfo &probe1 = surfSpace->envProbeInfo[1];

        Vec3 probe1Extent = probe1.envProbe->GetWorldAABB().Extents();

        shader->SetTexture("probe1DiffuseCubeMap", probe1.envProbe->GetDiffuseProbeTexture());
        shader->SetTexture("probe1SpecularCubeMap", probe1.envProbe->GetSpecularProbeTexture());
        shader->SetConstant1f("probe1SpecularCubeMapMaxMipLevel", Math::Log(2.0f, probe1.envProbe->GetSpecularProbeTexture()->GetWidth()));
        shader->SetConstant4f("probe1Position", Vec4(probe1.envProbe->GetBoxCenter(), probe1.envProbe->IsBoxProjection() ? 1.0f : 0.0f));
        shader->SetConstant3f("probe1Mins", -probe1Extent);
        shader->SetConstant3f("probe1Maxs", +probe1Extent);
    }
}

void Batch::SetMaterialConstants(const Material::ShaderPass *mtrlPass, const Shader *shader) const {
    if (shader->builtInConstantIndices[Shader::TextureMatrixSConst] >= 0) {
        Vec4 textureMatrixS = Vec4(mtrlPass->tcScale[0], 0.0f, 0.0f, mtrlPass->tcTranslation[0]);
        Vec4 textureMatrixT = Vec4(0.0f, mtrlPass->tcScale[1], 0.0f, mtrlPass->tcTranslation[1]);

        shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixSConst], textureMatrixS);
        shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixTConst], textureMatrixT);
    }

    if (shader->builtInConstantIndices[Shader::PerforatedAlphaConst] >= 0) {
        shader->SetConstant1f(shader->builtInConstantIndices[Shader::PerforatedAlphaConst], mtrlPass->cutoffAlpha);
    }

    SetVertexColorConstants(shader, mtrlPass->vertexColorMode);
}

void Batch::RenderColor(const Material::ShaderPass *mtrlPass, const Color4 &color) const {
    Shader *shader = ShaderManager::constantColorShader;

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    shader->SetConstant4f("color", color);

    DrawPrimitives();
}

void Batch::RenderSelection(const Material::ShaderPass *mtrlPass, const Vec3 &idInVec3) const {
    Shader *shader = ShaderManager::selectionIdShader;

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    shader->Bind();

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        const Texture *baseTexture = mtrlPass->shader ? TextureFromShaderProperties(mtrlPass, "albedoMap") : mtrlPass->texture;
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);

        Vec4 textureMatrixS = Vec4(mtrlPass->tcScale[0], 0.0f, 0.0f, mtrlPass->tcTranslation[0]);
        Vec4 textureMatrixT = Vec4(0.0f, mtrlPass->tcScale[1], 0.0f, mtrlPass->tcTranslation[1]);

        shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixSConst], textureMatrixS);
        shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixTConst], textureMatrixT);

        shader->SetConstant1f(shader->builtInConstantIndices[Shader::PerforatedAlphaConst], mtrlPass->cutoffAlpha);
    }

    shader->SetConstant3f("id", idInVec3);

    DrawPrimitives();
}

void Batch::RenderDepth(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = ShaderManager::depthShader;

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }
    
    shader->Bind();

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        const Texture *baseTexture = mtrlPass->shader ? TextureFromShaderProperties(mtrlPass, "albedoMap") : mtrlPass->texture;
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);

        SetMaterialConstants(mtrlPass, shader);
    }

    DrawPrimitives();
}

void Batch::RenderDepthNormal(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = ShaderManager::depthNormalShader;

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        const Texture *baseTexture = mtrlPass->shader ? TextureFromShaderProperties(mtrlPass, "albedoMap") : mtrlPass->texture;
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);

        SetMaterialConstants(mtrlPass, shader);
    }

    DrawPrimitives();
}

void Batch::RenderVelocity(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = ShaderManager::objectMotionBlurShader;

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        if (shader->gpuSkinningVersion[subMesh->gpuSkinningVersionIndex]) {
            shader = shader->gpuSkinningVersion[subMesh->gpuSkinningVersionIndex];
        }
    }

    shader->Bind();

    SetMatrixConstants(shader);

    Mat4 prevModelViewMatrix = backEnd.camera->def->GetViewMatrix() * surfSpace->def->GetPrevObjectToWorldMatrix();
    //shader->SetConstantMatrix4fv("prevModelViewMatrix", 1, true, prevModelViewMatrix);

    Mat4 prevModelViewProjMatrix = backEnd.camera->def->GetProjMatrix() * prevModelViewMatrix;
    shader->SetConstant4x4f("prevModelViewProjectionMatrix", true, prevModelViewProjMatrix);

    shader->SetConstant1f("shutterSpeed", r_motionBlur_ShutterSpeed.GetFloat() / backEnd.ctx->frameTime);
    //shader->SetConstant1f("motionBlurID", (float)surfSpace->id);

    shader->SetTexture("depthMap", backEnd.ctx->screenDepthTexture);

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        const Texture *baseTexture = mtrlPass->shader ? TextureFromShaderProperties(mtrlPass, "albedoMap") : mtrlPass->texture;
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);
        
        Vec4 textureMatrixS = Vec4(mtrlPass->tcScale[0], 0.0f, 0.0f, mtrlPass->tcTranslation[0]);
        Vec4 textureMatrixT = Vec4(0.0f, mtrlPass->tcScale[1], 0.0f, mtrlPass->tcTranslation[1]);

        shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixSConst], textureMatrixS);
        shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixTConst], textureMatrixT);

        shader->SetConstant1f(shader->builtInConstantIndices[Shader::PerforatedAlphaConst], mtrlPass->cutoffAlpha);
    }

    if (subMesh->useGpuSkinning) {
        SetSkinningConstants(shader, surfSpace->def->GetState().mesh->skinningJointCache);
    }

    DrawPrimitives();
}

void Batch::RenderGeneric(const Material::ShaderPass *mtrlPass) const {
    Shader *shader;

    if (mtrlPass->shader) {
        shader = mtrlPass->shader;

        if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
            if (shader->GetPerforatedVersion()) {
                shader = shader->GetPerforatedVersion();
            }
        }

        if (subMesh->useGpuSkinning) {
            Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
            if (skinningShader) {
                shader = skinningShader;
            }
        }

        if (numInstances > 0) {
            if (shader->GetGPUInstancingVersion()) {
                shader = shader->GetGPUInstancingVersion();
            }
        }

        shader->Bind();

        SetShaderProperties(shader, mtrlPass->shaderProperties);
    } else {
        shader = ShaderManager::standardDefaultShader;
                
        if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
            if (shader->GetPerforatedVersion()) {
                shader = shader->GetPerforatedVersion();
            }
        }

        if (subMesh->useGpuSkinning) {
            Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
            if (skinningShader) {
                shader = skinningShader;
            }
        }

        if (numInstances > 0) {
            if (shader->GetGPUInstancingVersion()) {
                shader = shader->GetGPUInstancingVersion();
            }
        }

        shader->Bind();
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], mtrlPass->texture);
    }

    shader->SetConstant1f("ambientScale", 1.0f);

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    SetMaterialConstants(mtrlPass, shader);

    shader->SetConstant3f(shader->builtInConstantIndices[Shader::ViewOriginConst], backEnd.camera->def->GetState().origin);

    DrawPrimitives();
}

void Batch::RenderAmbient(const Material::ShaderPass *mtrlPass, float ambientScale) const {
    Shader *shader = ShaderManager::standardDefaultShader;

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    const Texture *baseTexture = mtrlPass->shader ? TextureFromShaderProperties(mtrlPass, "albedoMap") : mtrlPass->texture;
    shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);

    shader->SetConstant1f("ambientScale", ambientScale);

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    SetMaterialConstants(mtrlPass, shader);

    DrawPrimitives();
}

void Batch::RenderIndirectLit(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = shader = mtrlPass->shader;

    if (shader && shader->GetIndirectLitVersion()) {
        shader = shader->GetIndirectLitVersion();
    } else {
        shader = ShaderManager::standardDefaultIndirectLitShader;
    }

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    if (mtrlPass->shader) {
        if (mtrlPass->shader->GetIndirectLitVersion()) {
            SetShaderProperties(shader, mtrlPass->shaderProperties);
        } else {
            const Texture *baseTexture = TextureFromShaderProperties(mtrlPass, "albedoMap");
            shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);
        }
    } else {
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], mtrlPass->texture);
    }

    shader->SetTexture("prefilteredDfgMap", backEnd.integrationLUTTexture);

    SetMatrixConstants(shader);

    SetProbeConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    SetMaterialConstants(mtrlPass, shader);

    shader->SetConstant3f(shader->builtInConstantIndices[Shader::ViewOriginConst], backEnd.camera->def->GetState().origin);

    DrawPrimitives();
}

static Shader *GetShadowShader(Shader *shader, RenderLight::Type lightType) {
    if (lightType == RenderLight::PointLight) {
        return shader->GetPointShadowVersion();
    }
    if (lightType == RenderLight::SpotLight) {
        return shader->GetSpotShadowVersion();
    }
    if (lightType == RenderLight::DirectionalLight) {
        return shader->GetParallelShadowVersion();
    }
    return shader;
}

void Batch::RenderAmbient_DirectLit(const Material::ShaderPass *mtrlPass, float ambientScale) const {
    Shader *shader = shader = mtrlPass->shader;
    
    if (shader && shader->GetDirectLitVersion()) {
        shader = shader->GetDirectLitVersion();
    } else {
        shader = ShaderManager::standardDefaultDirectLitShader;
    }

    bool useShadowMap = false;
    if (r_shadows.GetInteger()) {
        if ((surfLight->def->GetState().flags & RenderLight::CastShadowsFlag) && (surfSpace->def->GetState().flags & RenderObject::ReceiveShadowsFlag)) {
            shader = GetShadowShader(shader, surfLight->def->GetState().type);
            useShadowMap = true;
        }
    }

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();
    
    if (mtrlPass->shader) {
        if (mtrlPass->shader->GetDirectLitVersion()) {
            SetShaderProperties(shader, mtrlPass->shaderProperties);
        } else {
            const Texture *baseTexture = TextureFromShaderProperties(mtrlPass, "albedoMap");
            shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);
        }
    } else {
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], mtrlPass->texture);
    }

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    SetMaterialConstants(mtrlPass, shader);

    SetupLightingShader(mtrlPass, shader, useShadowMap);

    DrawPrimitives();
}

void Batch::RenderIndirectLit_DirectLit(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = shader = mtrlPass->shader;

    if (shader && shader->GetIndirectLitDirectLitVersion()) {
        shader = shader->GetIndirectLitDirectLitVersion();
    } else {
        shader = ShaderManager::standardDefaultIndirectLitDirectLitShader;
    }

    bool useShadowMap = false;
    if (r_shadows.GetInteger()) {
        if ((surfLight->def->GetState().flags & RenderLight::CastShadowsFlag) && (surfSpace->def->GetState().flags & RenderObject::ReceiveShadowsFlag)) {
            shader = GetShadowShader(shader, surfLight->def->GetState().type);
            useShadowMap = true;
        }
    }

    if (mtrlPass->renderingMode == Material::RenderingMode::AlphaCutoff) {
        if (shader->GetPerforatedVersion()) {
            shader = shader->GetPerforatedVersion();
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    shader->SetTexture("prefilteredDfgMap", backEnd.integrationLUTTexture);

    if (mtrlPass->shader) {
        if (mtrlPass->shader->GetIndirectLitDirectLitVersion()) {
            SetShaderProperties(shader, mtrlPass->shaderProperties);
        } else {
            const Texture *baseTexture = TextureFromShaderProperties(mtrlPass, "albedoMap");
            shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);
        }
    } else {
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], mtrlPass->texture);
    }

    SetMatrixConstants(shader);

    SetProbeConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    SetMaterialConstants(mtrlPass, shader);

    SetupLightingShader(mtrlPass, shader, useShadowMap);

    DrawPrimitives();
}

void Batch::RenderBase(const Material::ShaderPass *mtrlPass, float ambientScale) const {
    if (r_indirectLit.GetBool()) {
        if (surfLight) {
            RenderIndirectLit_DirectLit(mtrlPass);
        } else {
            RenderIndirectLit(mtrlPass);
        }
    } else {
        if (surfLight) {
            RenderAmbient_DirectLit(mtrlPass, ambientScale);
        } else {
            RenderAmbient(mtrlPass, ambientScale);
        }
    }
}

void Batch::SetupLightingShader(const Material::ShaderPass *mtrlPass, const Shader *shader, bool useShadowMap) const {
    Vec4 lightVec;

    if (surfLight->def->GetState().type == RenderLight::DirectionalLight) {
        lightVec = Vec4(-surfLight->def->GetState().axis[0], 0);
    } else {
        lightVec = Vec4(surfLight->def->GetState().origin, 1);
    }
    shader->SetConstant4f(shader->builtInConstantIndices[Shader::LightVecConst], lightVec);

    shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::LightTextureMatrixConst], true, surfLight->viewProjTexMatrix);
    shader->SetConstant4x3f(shader->builtInConstantIndices[Shader::LightFallOffMatrixConst], true, surfLight->def->GetFallOffMatrix());
    shader->SetConstant1f(shader->builtInConstantIndices[Shader::LightFallOffExponentConst], surfLight->def->GetState().fallOffExponent);

    shader->SetConstant3f(shader->builtInConstantIndices[Shader::ViewOriginConst], backEnd.camera->def->GetState().origin);

    if (useShadowMap) {
        if (surfLight->def->GetState().type == RenderLight::PointLight) {
            shader->SetConstant2f("shadowProjectionDepth", backEnd.shadowProjectionDepth);
            shader->SetConstant1f("vscmBiasedScale", backEnd.ctx->vscmBiasedScale);

            shader->SetTexture(shader->builtInSamplerUnits[Shader::CubicNormalCubeMapSampler], textureManager.cubicNormalCubeMapTexture);
            shader->SetTexture(shader->builtInSamplerUnits[Shader::IndirectionCubeMapSampler], backEnd.ctx->indirectionCubeMapTexture);
            shader->SetTexture(shader->builtInSamplerUnits[Shader::ShadowMapSampler], backEnd.ctx->vscmRT->DepthStencilTexture());
        } else if (surfLight->def->GetState().type == RenderLight::SpotLight) {
            shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::ShadowProjMatrixConst], true, backEnd.shadowViewProjectionScaleBiasMatrix[0]);
            shader->SetTexture(shader->builtInSamplerUnits[Shader::ShadowArrayMapSampler], backEnd.ctx->shadowMapRT->DepthStencilTexture());
        } else if (surfLight->def->GetState().type == RenderLight::DirectionalLight) {
            shader->SetConstantArray4x4f(shader->builtInConstantIndices[Shader::ShadowCascadeProjMatrixConst], true, r_CSM_count.GetInteger(), backEnd.shadowViewProjectionScaleBiasMatrix);

            if (r_CSM_selectionMethod.GetInteger() == 0) {
                // z-based selection shader needs shadowSplitFar value
                float sFar[4];
                for (int cascadeIndex = 0; cascadeIndex < r_CSM_count.GetInteger(); cascadeIndex++) {
                    float dFar = backEnd.csmDistances[cascadeIndex + 1];
                    sFar[cascadeIndex] = (backEnd.projMatrix[2][2] * -dFar + backEnd.projMatrix[2][3]) / dFar;
                    sFar[cascadeIndex] = sFar[cascadeIndex] * 0.5f + 0.5f;
                }
                shader->SetConstant4f(shader->builtInConstantIndices[Shader::ShadowSplitFarConst], sFar);
            }
            shader->SetConstant1f("cascadeBlendSize", r_CSM_blendSize.GetFloat());
            shader->SetConstantArray1f("shadowMapFilterSize", r_CSM_count.GetInteger(), backEnd.shadowMapFilterSize);
            shader->SetTexture(shader->builtInSamplerUnits[Shader::ShadowArrayMapSampler], backEnd.ctx->shadowMapRT->DepthStencilTexture());
        }

        if (r_shadowMapQuality.GetInteger() == 3) {
            shader->SetTexture("randomRotMatMap", textureManager.randomRotMatTexture);
        }

        Vec2 shadowMapTexelSize;

        if (surfLight->def->GetState().type == RenderLight::PointLight) {
            shadowMapTexelSize.x = 1.0f / backEnd.ctx->vscmRT->GetWidth();
            shadowMapTexelSize.y = 1.0f / backEnd.ctx->vscmRT->GetHeight();
        } else {
            shadowMapTexelSize.x = 1.0f / backEnd.ctx->shadowMapRT->GetWidth();
            shadowMapTexelSize.y = 1.0f / backEnd.ctx->shadowMapRT->GetHeight();
        }

        shader->SetConstant2f("shadowMapTexelSize", shadowMapTexelSize);
    } else {
        /*
        // WARNING: for the nvidia's stupid dynamic branching... 
        if (r_shadowMapQuality.GetInteger() == 3) {
            shader->SetTexture("randomRotMatMap", textureManager.randomRotMatTexture);
        }

        // WARNING: for the nvidia's stupid dynamic branching... 
        if (r_shadows.GetInteger() == 1) {
            shader->SetTexture("shadowArrayMap", backEnd.ctx->shadowMapRT->DepthStencilTexture());
        }*/
    }

    const Material *lightMaterial = surfLight->def->GetMaterial();

    shader->SetTexture(shader->builtInSamplerUnits[Shader::LightProjectionMapSampler], lightMaterial->GetPass()->texture);

    Color4 lightColor = surfLight->lightColor * surfLight->def->GetState().intensity * r_lightScale.GetFloat();
    shader->SetConstant4f(shader->builtInConstantIndices[Shader::LightColorConst], lightColor);

    /*bool useLightCube = lightStage->textureStage.texture->GetType() == TextureCubeMap ? true : false;
    shader->SetConstant1i("useLightCube", useLightCube);
    
    if (useLightCube) {
        shader->SetTexture("lightCubeMap", lightStage->textureStage.texture);
    } else {
        shader->SetTexture("lightCubeMap", textureManager.m_defaultCubeMapTexture);
    }*/
}

void Batch::RenderLightInteraction(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = mtrlPass->shader;

    if (shader && shader->GetDirectLitVersion()) {
        shader = shader->GetDirectLitVersion();
    } else {
        shader = ShaderManager::standardDefaultDirectLitShader;
    }

    bool useShadowMap = false;
    if (r_shadows.GetInteger()) {
        if ((surfLight->def->GetState().flags & RenderLight::CastShadowsFlag) && (surfSpace->def->GetState().flags & RenderObject::ReceiveShadowsFlag)) {
            shader = GetShadowShader(shader, surfLight->def->GetState().type);
            useShadowMap = true;
        }
    }

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    shader->SetConstant1f("ambientScale", 0);

    if (mtrlPass->shader) {
        if (mtrlPass->shader->GetDirectLitVersion()) {
            SetShaderProperties(shader, mtrlPass->shaderProperties);
        } else {
            const Texture *baseTexture = TextureFromShaderProperties(mtrlPass, "albedoMap");
            shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], baseTexture);
        }
    } else {
        shader->SetTexture(shader->builtInSamplerUnits[Shader::AlbedoMapSampler], mtrlPass->texture);
    }

    SetMatrixConstants(shader);

    SetEntityConstants(mtrlPass, shader);

    SetMaterialConstants(mtrlPass, shader);

    SetupLightingShader(mtrlPass, shader, useShadowMap);
   
    DrawPrimitives();
}

void Batch::RenderFogLightInteraction(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = ShaderManager::fogLightShader;

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    shader->Bind();

    // light texture transform matrix
    Mat4 viewProjScaleBiasMat = surfLight->def->GetViewProjScaleBiasMatrix() * surfSpace->def->GetObjectToWorldMatrix();
    shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::LightTextureMatrixConst], true, viewProjScaleBiasMat);
    shader->SetConstant3f("fogColor", &surfLight->def->GetState().materialParms[RenderObject::RedParm]);

    Vec3 vec = surfLight->def->GetState().origin - backEnd.camera->def->GetState().origin;
    bool fogEnter = vec.Dot(surfLight->def->GetState().axis[0]) < 0.0f ? true : false;

    if (fogEnter) {
        shader->SetTexture("fogMap", textureManager.fogTexture);
        shader->SetTexture("fogEnterMap", textureManager.whiteTexture);
    } else {
        shader->SetTexture("fogMap", textureManager.whiteTexture);
        shader->SetTexture("fogEnterMap", textureManager.fogEnterTexture);
    }

    const Material *lightMaterial = surfLight->def->GetState().material;
    shader->SetTexture("fogProjectionMap", lightMaterial->GetPass()->texture);

    DrawPrimitives();
}

void Batch::RenderBlendLightInteraction(const Material::ShaderPass *mtrlPass) const {
    Shader *shader = ShaderManager::blendLightShader;

    if (subMesh->useGpuSkinning) {
        Shader *skinningShader = shader->GetGPUSkinningVersion(subMesh->gpuSkinningVersionIndex);
        if (skinningShader) {
            shader = skinningShader;
        }
    }

    if (numInstances > 0) {
        if (shader->GetGPUInstancingVersion()) {
            shader = shader->GetGPUInstancingVersion();
        }
    }

    Color3 blendColor(&surfLight->def->GetState().materialParms[RenderObject::RedParm]);

    if (rhi.IsSRGBWriteEnabled()) {
        blendColor = blendColor.SRGBToLinear();
    }

    shader->Bind();

    // light texture transform matrix
    Mat4 viewProjScaleBiasMat = surfLight->def->GetViewProjScaleBiasMatrix() * surfSpace->def->GetObjectToWorldMatrix();
    shader->SetConstant4x4f(shader->builtInConstantIndices[Shader::LightTextureMatrixConst], true, viewProjScaleBiasMat);
    shader->SetConstant3f("blendColor", blendColor);

    const Material *lightMaterial = surfLight->def->GetState().material;
    shader->SetTexture("blendProjectionMap", lightMaterial->GetPass()->texture);	

    DrawPrimitives();
}

void Batch::RenderGui(const Material::ShaderPass *mtrlPass) const {
    const Shader *shader;

    if (mtrlPass->shader) {
        shader = mtrlPass->shader;
        shader->Bind();

        SetShaderProperties(shader, mtrlPass->shaderProperties);
    } else {
        shader = ShaderManager::unlitShader;
        shader->Bind();

        shader->SetTexture("albedoMap", mtrlPass->texture);
    }

    SetMatrixConstants(shader);

    Vec4 textureMatrixS = Vec4(mtrlPass->tcScale[0], 0.0f, 0.0f, mtrlPass->tcTranslation[0]);
    Vec4 textureMatrixT = Vec4(0.0f, mtrlPass->tcScale[1], 0.0f, mtrlPass->tcTranslation[1]);

    shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixSConst], textureMatrixS);
    shader->SetConstant4f(shader->builtInConstantIndices[Shader::TextureMatrixTConst], textureMatrixT);

    Color4 color;
    if (mtrlPass->useOwnerColor) {
        color = Color4(&surfSpace->def->GetState().materialParms[RenderObject::RedParm]);
    } else {
        color = mtrlPass->constantColor;
    }

    shader->SetConstant4f(shader->builtInConstantIndices[Shader::ConstantColorConst], color);

    SetVertexColorConstants(shader, Material::ModulateVertexColor);

    DrawPrimitives();
}

BE_NAMESPACE_END
