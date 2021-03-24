/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <ShadowedBistroExampleComponent.h>
#include <SampleComponentConfig.h>
#include <Automation/ScriptableImGui.h>
#include <Automation/ScriptRunnerBus.h>

#include <Atom/Component/DebugCamera/CameraControllerBus.h>
#include <Atom/Component/DebugCamera/NoClipControllerComponent.h>
#include <imgui/imgui.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Transform.h>
#include <AzFramework/Components/CameraBus.h>

namespace AtomSampleViewer
{
    const AZ::Color ShadowedBistroExampleComponent::DirectionalLightColor = AZ::Color::CreateOne();
    const AZ::Render::ShadowmapSize ShadowedBistroExampleComponent::s_shadowmapSizes[] =
    {
        AZ::Render::ShadowmapSize::Size256,
        AZ::Render::ShadowmapSize::Size512,
        AZ::Render::ShadowmapSize::Size1024,
        AZ::Render::ShadowmapSize::Size2048
    };
    const char* ShadowedBistroExampleComponent::s_directionalLightShadowmapSizeLabels[] =
    {
        "256",
        "512",
        "1024",
        "2048"
    };
    const AZ::Render::ShadowFilterMethod ShadowedBistroExampleComponent::s_shadowFilterMethods[] =
    {
        AZ::Render::ShadowFilterMethod::None,
        AZ::Render::ShadowFilterMethod::Pcf,
        AZ::Render::ShadowFilterMethod::Esm,
        AZ::Render::ShadowFilterMethod::EsmPcf
    };
    const char* ShadowedBistroExampleComponent::s_shadowFilterMethodLabels[] =
    {
        "None",
        "PCF",
        "ESM",
        "ESM+PCF"
    };

    void ShadowedBistroExampleComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ShadowedBistroExampleComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void ShadowedBistroExampleComponent::Activate()
    {
        using namespace AZ;
        m_directionalLightShadowmapSizeIndex = s_shadowmapSizeIndexDefault;
        m_spotLightShadowmapSize = Render::ShadowmapSize::None; // random
        m_cascadeCount = s_cascadesCountDefault;
        m_ratioLogarithmUniform = s_ratioLogarithmUniformDefault;
        m_spotLightCount = 0;

        // heuristic spot light default position configuration
        m_spotLightsBasePosition[0] = 0.04f;
        m_spotLightsBasePosition[1] = 0.04f;;
        m_spotLightsBasePosition[2] = -0.03f;
        m_spotLightsPositionScatteringRatio = 0.27f;

        RPI::Scene* scene = RPI::RPISystemInterface::Get()->GetDefaultScene().get();
        m_directionalLightFeatureProcessor = scene->GetFeatureProcessor<Render::DirectionalLightFeatureProcessorInterface>();
        m_spotLightFeatureProcessor = scene->GetFeatureProcessor<Render::SpotLightFeatureProcessorInterface>();

        SetupScene();

        // enable camera control
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::CameraControllerRequestBus::Events::Enable,
            azrtti_typeid<Debug::NoClipControllerComponent>());
        SaveCameraConfiguration();

        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            75.f);

        m_cameraTransformInitialized = false;

        m_imguiSidebar.Activate();

        TickBus::Handler::BusConnect();

        // Don't continue the script until after the models have loaded.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::PauseScriptWithTimeout, 120.0f);
    }

    void ShadowedBistroExampleComponent::Deactivate()
    {
        using namespace AZ;

        TickBus::Handler::BusDisconnect();

        m_imguiSidebar.Deactivate();

        // disable camera control
        RestoreCameraConfiguration();
        Debug::CameraControllerRequestBus::Event(
            GetCameraEntityId(),
            &Debug::CameraControllerRequestBus::Events::Disable);

        GetMeshFeatureProcessor()->ReleaseMesh(m_meshHandle);

        m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        UpdateSpotLightCount(0);
    }

    void ShadowedBistroExampleComponent::OnTick(float deltaTime, AZ::ScriptTimePoint timePoint)
    {
        AZ_UNUSED(deltaTime);
        AZ_UNUSED(timePoint);

        using namespace AZ;

        SetInitialCameraTransform();

        const auto lightTrans = Transform::CreateRotationZ(m_directionalLightYaw) * Transform::CreateRotationX(m_directionalLightPitch);
        m_directionalLightFeatureProcessor->SetDirection(
            m_directionalLightHandle,
            lightTrans.GetBasis(1));

        DrawSidebar();

        // Camera Configuration
        {
            Camera::Configuration config;
            Camera::CameraRequestBus::EventResult(
                config,
                GetCameraEntityId(),
                &Camera::CameraRequestBus::Events::GetCameraConfiguration);
            m_directionalLightFeatureProcessor->SetCameraConfiguration(
                m_directionalLightHandle,
                config);
        }

        // Camera Transform
        {
            Transform transform = Transform::CreateIdentity();
            TransformBus::EventResult(
                transform,
                GetCameraEntityId(),
                &TransformBus::Events::GetWorldTM);
            m_directionalLightFeatureProcessor->SetCameraTransform(
                m_directionalLightHandle, transform);
        }
    }

    void ShadowedBistroExampleComponent::OnModelReady(AZ::Data::Instance<AZ::RPI::Model> model)
    {
        m_bistroExteriorAssetLoaded = true;
        m_worldAabb = model->GetAabb();
        UpdateSpotLightCount(SpotLightCountDefault);

        // Now that the models are initialized, we can allow the script to continue.
        ScriptRunnerRequestBus::Broadcast(&ScriptRunnerRequests::ResumeScript);
    }

    void ShadowedBistroExampleComponent::SaveCameraConfiguration()
    {
        Camera::CameraRequestBus::EventResult(
            m_originalFarClipDistance,
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::GetFarClipDistance);
    }

    void ShadowedBistroExampleComponent::RestoreCameraConfiguration()
    {
        Camera::CameraRequestBus::Event(
            GetCameraEntityId(),
            &Camera::CameraRequestBus::Events::SetFarClipDistance,
            m_originalFarClipDistance);
    }

    void ShadowedBistroExampleComponent::SetInitialCameraTransform()
    {
        using namespace AZ;

        if (!m_cameraTransformInitialized)
        {
            // heuristic relatively scenic camera location
            const float cameraTransValues[12] =
            {
                sqrtf(0.5f), -sqrtf(0.4f), -sqrtf(0.1f), -2.1f,
                sqrtf(0.5f),  sqrtf(0.4f),  sqrtf(0.1f), -2.9f,
                sqrtf(0.0f), -sqrtf(0.1f),  sqrtf(0.8f),  1.9f
            };
            const auto cameraTrans = Transform::CreateFromMatrix3x4(Matrix3x4::CreateFromRowMajorFloat12(cameraTransValues));
            TransformBus::Event(
                GetCameraEntityId(),
                &TransformBus::Events::SetWorldTM,
                cameraTrans);
            m_cameraTransformInitialized = true;
        }
    }

    void ShadowedBistroExampleComponent::SetupScene()
    {
        using namespace AZ;

        const char* bistroPath = "Objects/Bistro/Bistro_Research_Exterior.azmodel";
        Data::Asset<RPI::ModelAsset> modelAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::ModelAsset>(bistroPath, RPI::AssetUtils::TraceLevel::Assert);
        Data::Asset<RPI::MaterialAsset> materialAsset = RPI::AssetUtils::GetAssetByProductPath<RPI::MaterialAsset>(DefaultPbrMaterialPath, RPI::AssetUtils::TraceLevel::Assert);
        m_meshHandle = GetMeshFeatureProcessor()->AcquireMesh(modelAsset, RPI::Material::FindOrCreate(materialAsset));

        // rotate the entity 180 degrees about Z (the vertical axis)
        // This makes it consistent with how it was positioned in the world when the world was Y-up.
        GetMeshFeatureProcessor()->SetTransform(m_meshHandle, Transform::CreateRotationZ(AZ::Constants::Pi));

        Data::Instance<RPI::Model> model = GetMeshFeatureProcessor()->GetModel(m_meshHandle);
        if (model)
        {
            OnModelReady(model);
        }
        else
        {
            GetMeshFeatureProcessor()->ConnectModelChangeEventHandler(m_meshHandle, m_meshChangedHandler);
        }

        // directional light
        {
            Render::DirectionalLightFeatureProcessorInterface* featureProcessor = m_directionalLightFeatureProcessor;
            const DirectionalLightHandle handle = featureProcessor->AcquireLight();

            const auto lightTransform = Transform::CreateLookAt(
                Vector3(100, 100, 100),
                Vector3::CreateZero());
            featureProcessor->SetDirection(
                handle,
                lightTransform.GetBasis(1));

            featureProcessor->SetRgbIntensity(handle, AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux>(DirectionalLightColor * m_directionalLightIntensity));
            featureProcessor->SetCascadeCount(handle, s_cascadesCountDefault);
            featureProcessor->SetShadowmapSize(handle, s_shadowmapSizes[s_shadowmapSizeIndexDefault]);
            featureProcessor->SetViewFrustumCorrectionEnabled(handle, m_isCascadeCorrectionEnabled);
            featureProcessor->SetShadowFilterMethod(handle, s_shadowFilterMethods[m_shadowFilterMethodIndexDirectional]);
            featureProcessor->SetShadowBoundaryWidth(handle, m_boundaryWidthDirectional);
            featureProcessor->SetPredictionSampleCount(handle, m_predictionSampleCountDirectional);
            featureProcessor->SetFilteringSampleCount(handle, m_filteringSampleCountDirectional);
            featureProcessor->SetGroundHeight(handle, 0.f);

            m_directionalLightHandle = handle;
            SetupDebugFlags();
        }

        // spot lights are initialized after loading models.
        BuildSpotLightParameters();
    }

    void ShadowedBistroExampleComponent::BuildSpotLightParameters()
    {
        m_random.SetSeed(0);
        m_spotLights.clear();
        m_spotLights.reserve(SpotLightCountMax);
        for (int index = 0; index < SpotLightCountMax; ++index)
        {
            m_spotLights.emplace_back(
                GetRandomColor(),
                GetRandomPosition(),
                GetRandomShadowmapSize());
        }
    }

    void ShadowedBistroExampleComponent::UpdateSpotLightCount(uint16_t count)
    {
        // We suppose m_spotLights has been initialized except m_entity.
        using namespace AZ;

        // Don't assert here if count == 0, since the count is set to 0 during Deactivate
        if ((!m_worldAabb.IsValid() || !m_worldAabb.IsFinite()) && count != 0)
        {
            AZ_Assert(false, "World AABB is not initialized correctly.");
            return;
        }

        for (int index = count; index < m_spotLightCount; ++index)
        {
            SpotLightHandle& handle = m_spotLights[index].m_handle;
            m_spotLightFeatureProcessor->ReleaseLight(handle);
        }

        const int previousSpotLightCount = m_spotLightCount;

        for (int index = previousSpotLightCount; index < count; ++index)
        {
            Render::SpotLightFeatureProcessorInterface* const featureProcessor = m_spotLightFeatureProcessor;
            const SpotLightHandle handle = featureProcessor->AcquireLight();

            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(m_spotLights[index].m_color * m_spotLightIntensity);
            featureProcessor->SetRgbIntensity(handle, lightColor);
            featureProcessor->SetAttenuationRadius(
                handle,
                sqrtf(m_spotLightIntensity / CutoffIntensity));
            featureProcessor->SetShadowmapSize(
                handle,
                m_spotLightShadowEnabled ?
                m_spotLights[index].m_shadowmapSize :
                Render::ShadowmapSize::None);
            featureProcessor->SetConeAngles(handle, 45.f, 55.f);
            featureProcessor->SetShadowFilterMethod(handle, aznumeric_cast<Render::ShadowFilterMethod>(m_shadowFilterMethodIndexSpot));
            featureProcessor->SetPredictionSampleCount(handle, aznumeric_cast<uint16_t>(m_predictionSampleCountSpot));
            featureProcessor->SetFilteringSampleCount(handle, aznumeric_cast<uint16_t>(m_filteringSampleCountSpot));

            m_spotLights[index].m_handle = handle;

            UpdateSpotLightPosition(index);
        }

        m_spotLightCount = count;
    }

    const AZ::Color& ShadowedBistroExampleComponent::GetRandomColor()
    {
        static const AZStd::vector<AZ::Color> colors =
        {
            AZ::Colors::Red,
            AZ::Colors::Green,
            AZ::Colors::Blue,
            AZ::Colors::Cyan,
            AZ::Colors::Fuchsia,
            AZ::Colors::Yellow,
            AZ::Colors::SpringGreen
        };

        return colors[m_random.GetRandom() % colors.size()];
    }

    AZ::Vector3 ShadowedBistroExampleComponent::GetRandomPosition()
    {
        // returns a position in the range [-0.5, +0.5]^3.
        return AZ::Vector3(
            m_random.GetRandomFloat() - 0.5f,
            m_random.GetRandomFloat() - 0.5f,
            m_random.GetRandomFloat() - 0.5f);
    }

    AZ::Render::ShadowmapSize ShadowedBistroExampleComponent::GetRandomShadowmapSize() 
    {
        static const AZStd::vector<AZ::Render::ShadowmapSize> sizes =
        {
            AZ::Render::ShadowmapSize::Size256,
            AZ::Render::ShadowmapSize::Size512,
            AZ::Render::ShadowmapSize::Size1024,
            AZ::Render::ShadowmapSize::Size2048
        };

        return sizes[m_random.GetRandom() % sizes.size()];
    }

    void ShadowedBistroExampleComponent::DrawSidebar()
    {
        using namespace AZ;
        using namespace AZ::Render;

        if (!m_imguiSidebar.Begin())
        {
            return;
        }

        if (!m_bistroExteriorAssetLoaded)
        {
            const ImGuiWindowFlags windowFlags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("Asset", nullptr, windowFlags))
            {
                ImGui::Text("Bistro Exterior Model: %s", m_bistroExteriorAssetLoaded ? "Loaded" : "Loading...");
                ImGui::End();
            }
            m_imguiSidebar.End();
            return;
        }

        ImGui::Spacing();

        ImGui::Text("Directional Light");
        ImGui::Indent();
        {
            ScriptableImGui::SliderAngle("Pitch", &m_directionalLightPitch, -90.0f, 0.f);
            ScriptableImGui::SliderAngle("Yaw", &m_directionalLightYaw, 0.f, 360.f);

            if (ScriptableImGui::SliderFloat("Intensity##directional", &m_directionalLightIntensity, 0.f, 20.f, "%.1f", 2.f))
            {
                AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux> lightColor(DirectionalLightColor * m_directionalLightIntensity);
                m_directionalLightFeatureProcessor->SetRgbIntensity(m_directionalLightHandle, lightColor);
            }

            ImGui::Separator();

            ImGui::Text("Shadowmap Size");
            if (ScriptableImGui::Combo(
                "Size",
                &m_directionalLightShadowmapSizeIndex,
                s_directionalLightShadowmapSizeLabels,
                AZ_ARRAY_SIZE(s_directionalLightShadowmapSizeLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowmapSize(
                    m_directionalLightHandle,
                    s_shadowmapSizes[m_directionalLightShadowmapSizeIndex]);
            }

            ImGui::Text("Number of cascades");
            bool cascadesChanged = false;
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("1", &m_cascadeCount, 1);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("2", &m_cascadeCount, 2);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("3", &m_cascadeCount, 3);
            ImGui::SameLine();
            cascadesChanged = cascadesChanged ||
                ScriptableImGui::RadioButton("4", &m_cascadeCount, 4);
            if (cascadesChanged)
            {
                m_directionalLightFeatureProcessor->SetCascadeCount(
                    m_directionalLightHandle,
                    m_cascadeCount);
            }

            ImGui::Spacing();

            ImGui::Text("Cascade partition scheme");
            ImGui::Text("  (uniform <--> logarithm)");
            if (ScriptableImGui::SliderFloat("Ratio", &m_ratioLogarithmUniform, 0.f, 1.f, "%0.3f"))
            {
                m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(
                    m_directionalLightHandle,
                    m_ratioLogarithmUniform);
            }

            ImGui::Spacing();

            if (ScriptableImGui::Checkbox("Cascade Position Correction", &m_isCascadeCorrectionEnabled))
            {
                m_directionalLightFeatureProcessor->SetViewFrustumCorrectionEnabled(
                    m_directionalLightHandle,
                    m_isCascadeCorrectionEnabled);
            }

            bool DebugFlagsChanged = false;
            DebugFlagsChanged = DebugFlagsChanged || ScriptableImGui::Checkbox("Debug Coloring", &m_isDebugColoringEnabled);
            DebugFlagsChanged = DebugFlagsChanged || ScriptableImGui::Checkbox("Debug Bounding Box", &m_isDebugBoundingBoxEnabled);

            if (DebugFlagsChanged)
            {
                SetupDebugFlags();
            }

            ImGui::Spacing();
            ImGui::Text("Filtering");
            if (ScriptableImGui::Combo(
                "Filter Method##Directional",
                &m_shadowFilterMethodIndexDirectional,
                s_shadowFilterMethodLabels,
                AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                m_directionalLightFeatureProcessor->SetShadowFilterMethod(
                    m_directionalLightHandle,
                    s_shadowFilterMethods[m_shadowFilterMethodIndexDirectional]);
            }

            if (m_shadowFilterMethodIndexDirectional != aznumeric_cast<int>(ShadowFilterMethod::None))
            {
                ImGui::Text("Boundary Width in meter");
                if (ScriptableImGui::SliderFloat("Width##Directional", &m_boundaryWidthDirectional, 0.f, 0.1f, "%.3f"))
                {
                    m_directionalLightFeatureProcessor->SetShadowBoundaryWidth(
                        m_directionalLightHandle,
                        m_boundaryWidthDirectional);
                }
            }

            if (m_shadowFilterMethodIndexDirectional == aznumeric_cast<int>(ShadowFilterMethod::Pcf) ||
                m_shadowFilterMethodIndexDirectional == aznumeric_cast<int>(ShadowFilterMethod::EsmPcf))
            {
                ImGui::Spacing();
                ImGui::Text("Filtering (PCF specific)");
                if (ScriptableImGui::SliderInt("Prediction # ##Directional", &m_predictionSampleCountDirectional, 4, 16))
                {
                    m_directionalLightFeatureProcessor->SetPredictionSampleCount(
                        m_directionalLightHandle,
                        m_predictionSampleCountDirectional);
                }
                if (ScriptableImGui::SliderInt("Filtering # ##Directional", &m_filteringSampleCountDirectional, 4, 64))
                {
                    m_directionalLightFeatureProcessor->SetFilteringSampleCount(
                        m_directionalLightHandle,
                        m_filteringSampleCountDirectional);
                }
            }
        }
        ImGui::Unindent();

        ImGui::Separator();

        ImGui::Text("Spot Lights");
        ImGui::Indent();
        {
            int spotLightCount = m_spotLightCount;
            if (ScriptableImGui::SliderInt("Number", &spotLightCount, 0, SpotLightCountMax))
            {
                UpdateSpotLightCount(spotLightCount);
            }

            if (ScriptableImGui::SliderFloat("Intensity##spot", &m_spotLightIntensity, 0.f, 100000.f, "%.1f", 4.f))
            {
                for (const SpotLight& light : m_spotLights)
                {
                    if (light.m_handle.IsValid())
                    {
                        AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Candela> lightColor(light.m_color * m_spotLightIntensity);
                        m_spotLightFeatureProcessor->SetRgbIntensity(light.m_handle, lightColor);
                        m_spotLightFeatureProcessor->SetAttenuationRadius(
                            light.m_handle,
                            sqrtf(m_spotLightIntensity / CutoffIntensity));
                    }
                }
            }

            // avoiding SliderFloat3 since its sliders are too narrow.
            if (ScriptableImGui::SliderFloat("Center X", &m_spotLightsBasePosition[0], -0.5f, 0.5f) ||
                ScriptableImGui::SliderFloat("Center Y", &m_spotLightsBasePosition[1], -0.5f, 0.5f) ||
                ScriptableImGui::SliderFloat("Center Z", &m_spotLightsBasePosition[2], -0.5f, 0.5f) ||
                ScriptableImGui::SliderFloat("Pos. Scatt. Ratio", &m_spotLightsPositionScatteringRatio, 0.f, 1.f))
            {
                UpdateSpotLightPositions();
            }

            bool spotLightShadowmapChanged = ScriptableImGui::Checkbox("Enable Shadow", &m_spotLightShadowEnabled);

            ImGui::Text("Shadowmap Size");
            int newSize = static_cast<int>(m_spotLightShadowmapSize);
            // To avoid GPU memory consumption, we avoid bigger shadowmap sizes here.
            spotLightShadowmapChanged = spotLightShadowmapChanged ||
                ScriptableImGui::RadioButton("256", &newSize, static_cast<int>(Render::ShadowmapSize::Size256)) ||
                ScriptableImGui::RadioButton("512", &newSize, static_cast<int>(Render::ShadowmapSize::Size512)) ||
                ScriptableImGui::RadioButton("1024", &newSize, static_cast<int>(Render::ShadowmapSize::Size1024)) ||
                ScriptableImGui::RadioButton("Random", &newSize, static_cast<int>(Render::ShadowmapSize::None));

            if (spotLightShadowmapChanged)
            {
                m_spotLightShadowmapSize = static_cast<Render::ShadowmapSize>(newSize);
                UpdateSpotLightShadowmapSize();
            }

            ImGui::Spacing();
            ImGui::Text("Filtering");
            if (ScriptableImGui::Combo(
                "Filter Method##Spot",
                &m_shadowFilterMethodIndexSpot,
                s_shadowFilterMethodLabels,
                AZ_ARRAY_SIZE(s_shadowFilterMethodLabels)))
            {
                for (int index = 0; index < m_spotLightCount; ++index)
                {
                    m_spotLightFeatureProcessor->SetShadowFilterMethod(m_spotLights[index].m_handle, aznumeric_cast<ShadowFilterMethod>(m_shadowFilterMethodIndexSpot));
                }
            }

            if (m_shadowFilterMethodIndexSpot != aznumeric_cast<int>(ShadowFilterMethod::None))
            {
                ImGui::Text("Boundary Width in degrees");
                if (ScriptableImGui::SliderFloat("Width##Spot", &m_boundaryWidthSpot, 0.f, 1.f, "%.3f"))
                {
                    for (int index = 0; index < m_spotLightCount; ++index)
                    {
                        m_spotLightFeatureProcessor->SetShadowBoundaryWidthAngle(m_spotLights[index].m_handle, m_boundaryWidthSpot);
                    }
                }
            }

            if (m_shadowFilterMethodIndexSpot == aznumeric_cast<int>(ShadowFilterMethod::Pcf) ||
                m_shadowFilterMethodIndexSpot == aznumeric_cast<int>(ShadowFilterMethod::EsmPcf))
            {
                ImGui::Spacing();
                ImGui::Text("Filtering (PCF specific)");
                if (ScriptableImGui::SliderInt("Predictiona # ##Spot", &m_predictionSampleCountSpot, 4, 16))
                {
                    for (int index = 0; index < m_spotLightCount; ++index)
                    {
                        m_spotLightFeatureProcessor->SetPredictionSampleCount(m_spotLights[index].m_handle, m_predictionSampleCountSpot);
                    }
                }
                if (ScriptableImGui::SliderInt("Filtering # ##Spot", &m_filteringSampleCountSpot, 4, 64))
                {
                    for (int index = 0; index < m_spotLightCount; ++index)
                    {
                        m_spotLightFeatureProcessor->SetFilteringSampleCount(m_spotLights[index].m_handle, m_filteringSampleCountSpot);
                    }
                }
            }
        }
        ImGui::Unindent();

        m_imguiSidebar.End();
    }

    void ShadowedBistroExampleComponent::UpdateSpotLightShadowmapSize()
    {
        using namespace AZ::Render;
        SpotLightFeatureProcessorInterface* const featureProcessor = m_spotLightFeatureProcessor;

        if (!m_spotLightShadowEnabled)
        {
            // disabled shadows
            for (const SpotLight& light : m_spotLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowmapSize(
                        light.m_handle,
                        ShadowmapSize::None);
                }
            }
        }
        else if (m_spotLightShadowmapSize != ShadowmapSize::None)
        {
            // uniform size
            for (const SpotLight& light : m_spotLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowmapSize(
                        light.m_handle,
                        m_spotLightShadowmapSize);
                }
            }
        }
        else
        {
            // random sizes
            for (const SpotLight& light : m_spotLights)
            {
                if (light.m_handle.IsValid())
                {
                    featureProcessor->SetShadowmapSize(
                        light.m_handle,
                        light.m_shadowmapSize);
                }
            }
        }
    }

    void ShadowedBistroExampleComponent::UpdateSpotLightPositions()
    {
        for (int index = 0; index < m_spotLightCount; ++index)
        {
            UpdateSpotLightPosition(index);
        }
    }

    void ShadowedBistroExampleComponent::UpdateSpotLightPosition(int index)
    {
        using namespace AZ;
        Render::SpotLightFeatureProcessorInterface* const featureProcessor = m_spotLightFeatureProcessor;


        if (!m_worldAabb.IsValid() || !m_worldAabb.IsFinite())
        {
            AZ_Assert(false, "World AABB is not initialized correctly.");
            return;
        }

        const Vector3 basePosition(
            m_spotLightsBasePosition[0],
            m_spotLightsBasePosition[1],
            m_spotLightsBasePosition[2]);
        const Vector3 relativePosition = basePosition + m_spotLights[index].m_relativePosition * m_spotLightsPositionScatteringRatio;
        const Vector3 position = m_worldAabb.GetCenter() +
            m_worldAabb.GetExtents() * relativePosition;
        const auto transform =
            Transform::CreateTranslation(position) * Transform::CreateRotationX(-Constants::HalfPi);
        featureProcessor->SetPosition(
            m_spotLights[index].m_handle,
            position);
        featureProcessor->SetDirection(
            m_spotLights[index].m_handle,
            transform.GetBasis(1));
    }

    void ShadowedBistroExampleComponent::SetupDebugFlags()
    {
        int flags = AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawNone;
        if (m_isDebugColoringEnabled)
        {
            flags |= AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawColoring;
        }
        if (m_isDebugBoundingBoxEnabled)
        {
            flags |= AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags::DebugDrawBoundingBoxes;
        }
        m_directionalLightFeatureProcessor->SetDebugFlags(m_directionalLightHandle,
            static_cast<AZ::Render::DirectionalLightFeatureProcessorInterface::DebugDrawFlags>(flags));
    }
} // namespace AtomSampleViewer
