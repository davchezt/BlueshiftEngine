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
#include "Asset/GuidMapper.h"

BE_NAMESPACE_BEGIN

EnvProbe::EnvProbe(int index) {
    this->index = index;
}

EnvProbe::~EnvProbe() {
    if (diffuseProbeTexture) {
        textureManager.ReleaseTexture(diffuseProbeTexture);
    }
    if (specularProbeTexture) {
        textureManager.ReleaseTexture(specularProbeTexture);
    }
    if (diffuseProbeRT) {
        RenderTarget::Delete(diffuseProbeRT);
    }
    if (specularProbeRT) {
        RenderTarget::Delete(specularProbeRT);
    }
}

void EnvProbe::Update(const EnvProbe::State *stateDef) {
    if (!needToRefresh) {
        bool resolutionMatch = state.resolution == stateDef->resolution;
        bool useHDRMatch = state.useHDR == stateDef->useHDR;
        bool clearMethodMatch = state.clearMethod == stateDef->clearMethod;
        bool clearColorMatch = state.clearColor == stateDef->clearColor;
        bool clippingNearMatch = state.clippingNear == stateDef->clippingNear;
        bool clippingFarMatch = state.clippingFar == stateDef->clippingFar;
        bool originMatch = state.origin == stateDef->origin;

        if (!resolutionMatch || !useHDRMatch || !clearMethodMatch || (stateDef->clearMethod == ClearMethod::ColorClear && !clearColorMatch) || 
            !clippingNearMatch || !clippingFarMatch || !originMatch) {
            needToRefresh = true;
        }
    }

    state = *stateDef;

    worldAABB = AABB(-state.boxExtent, state.boxExtent);
    worldAABB += state.origin + state.boxOffset;

    if (state.bakedDiffuseProbeTexture && state.bakedDiffuseProbeTexture != diffuseProbeTexture) {
        if (diffuseProbeTexture) {
            textureManager.ReleaseTexture(diffuseProbeTexture);
        }

        // Use baked diffuse convolution cubemap
        diffuseProbeTexture = state.bakedDiffuseProbeTexture;
        diffuseProbeTexture->AddRefCount();
    } else {
        if (!diffuseProbeTexture) {
            // Create default diffuse convolution cubemap 
            // TODO: Create using skybox
            diffuseProbeTexture = textureManager.AllocTexture(va("diffuseProbe-%i", index));
            diffuseProbeTexture->CreateEmpty(RHI::TextureCubeMap, 16, 16, 1, 1, 1, Image::RGB_8_8_8,
                Texture::Clamp | Texture::NoCompression | Texture::NoMipmaps | Texture::HighQuality);

            resourceGuidMapper.Set(Guid::CreateGuid(), diffuseProbeTexture->GetHashName());
        }
    }

    if (state.bakedSpecularProbeTexture && state.bakedSpecularProbeTexture != specularProbeTexture) {
        if (specularProbeTexture) {
            textureManager.ReleaseTexture(specularProbeTexture);
        }

        // Use baked specular convolution cubemap 
        specularProbeTexture = state.bakedSpecularProbeTexture;
        specularProbeTexture->AddRefCount();
    } else {
        if (!specularProbeTexture) {
            // Create default specular convolution cubemap
            // TODO: Create using skybox
            int size = 16;
            int numMipLevels = Math::Log(2, size) + 1;

            specularProbeTexture = textureManager.AllocTexture(va("specularProbe-%i", index));
            specularProbeTexture->CreateEmpty(RHI::TextureCubeMap, size, size, 1, 1, numMipLevels, Image::RGB_8_8_8,
                Texture::Clamp | Texture::NoCompression | Texture::HighQuality);

            resourceGuidMapper.Set(Guid::CreateGuid(), specularProbeTexture->GetHashName());
        }
    }
}

int EnvProbe::ToActualResolution(Resolution resolution) {
    // resolution value same order with EnvProbe::Resolution
    static const int size[] = {
        16, 32, 64, 128, 256, 512, 1024, 2048
    };
    return size[(int)resolution];
}

void EnvProbeJob::RevalidateDiffuseProbeRT() {
    // Recreate diffuse convolution texture if its format have changed.
    if ((envProbe->state.useHDR ^ Image::IsFloatFormat(envProbe->diffuseProbeTexture->GetFormat()))) {
        Image::Format format = envProbe->state.useHDR ? Image::RGB_11F_11F_10F : Image::RGB_8_8_8;

        envProbe->diffuseProbeTexture->CreateEmpty(RHI::TextureCubeMap, 64, 64, 1, 1, 1, format, // fixed size (64) for irradiance cubemap
            Texture::Clamp | Texture::NoMipmaps | Texture::HighQuality);

        if (envProbe->diffuseProbeRT) {
            RenderTarget::Delete(envProbe->diffuseProbeRT);
            envProbe->diffuseProbeRT = nullptr;
        }
    }

    // Create diffuse convolution render target if it is not created yet.
    if (!envProbe->diffuseProbeRT) {
        envProbe->diffuseProbeRT = RenderTarget::Create(envProbe->diffuseProbeTexture, nullptr, 0);
    }
}

void EnvProbeJob::RevalidateSpecularProbeRT() {
    int size = envProbe->GetSize();

    // Recreate diffuse convolution texture if its format or size have changed.
    if (size != envProbe->specularProbeTexture->GetWidth() ||
        (envProbe->state.useHDR ^ Image::IsFloatFormat(envProbe->specularProbeTexture->GetFormat()))) {
        Image::Format format = envProbe->state.useHDR ? Image::RGB_11F_11F_10F : Image::RGB_8_8_8;

        int numMipLevels = Math::Log(2, size) + 1;

        envProbe->specularProbeTexture->CreateEmpty(RHI::TextureCubeMap, size, size, 1, 1, numMipLevels, format,
            Texture::Clamp | Texture::HighQuality);

        if (envProbe->specularProbeRT) {
            RenderTarget::Delete(envProbe->specularProbeRT);
            envProbe->specularProbeRT = nullptr;
        }
    }

    // Create specular convolution render target if it is not created yet.
    if (!envProbe->specularProbeRT) {
        envProbe->specularProbeRT = RenderTarget::Create(envProbe->specularProbeTexture, nullptr, RHI::HasDepthBuffer);
    }
}

bool EnvProbeJob::IsFinished() const {
    if (!diffuseProbeCubemapComputed || specularProbeCubemapComputedLevel < specularProbeCubemapMaxLevel) {
        return false;
    }
    return true;
}

bool EnvProbeJob::Refresh() {
    if (specularProbeCubemapComputedLevel == -1) {
        if (specularProbeCubemapComputedLevel0Face == -1) {
            RevalidateSpecularProbeRT();
        }

        if (specularProbeCubemapComputedLevel0Face < 5) {
            // FIXME: use EnvProbeStatic instead of -1
            int staticMask = envProbe->state.type == EnvProbe::Type::Baked ? -1 : 0;

            // We can skip complex calculation of specular convolution cubemap for mipLevel 0.
            // It is same as perfect specular mirror. so we just render environment cubmap.
            renderSystem.CaptureEnvCubeRTFace(renderWorld,
                envProbe->state.clearMethod == EnvProbe::ClearMethod::ColorClear,
                envProbe->state.clearColor,
                envProbe->state.layerMask, staticMask, envProbe->state.origin,
                envProbe->state.clippingNear, envProbe->state.clippingFar,
                envProbe->specularProbeRT, specularProbeCubemapComputedLevel0Face + 1);

            specularProbeCubemapComputedLevel0Face++;
            return false;
        } else {
            specularProbeCubemapComputedLevel = 0;
        }
    }

    if (specularProbeCubemapComputedLevel < specularProbeCubemapMaxLevel) {
        // Generate specular convolution cube map from mipLevel 1 to specularProbeCubemapMaxLevel using environment cubemap.
        renderSystem.GenerateGGXLDSumRTLevel(envProbe->specularProbeTexture, envProbe->specularProbeRT, specularProbeCubemapMaxLevel, specularProbeCubemapComputedLevel + 1);

        specularProbeCubemapComputedLevel++;
        return false;
    }

    if (!diffuseProbeCubemapComputed) {
        RevalidateDiffuseProbeRT();

        // Generate diffuse convolution cube map using environment cubemap.
        renderSystem.GenerateIrradianceEnvCubeRT(envProbe->specularProbeTexture, envProbe->diffuseProbeRT);

        diffuseProbeCubemapComputed = true;
    }

    envProbe->needToRefresh = false;
    return true;
}

BE_NAMESPACE_END
