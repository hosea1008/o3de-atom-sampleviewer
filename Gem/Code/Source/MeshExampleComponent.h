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

#pragma once

#include <CommonSampleComponentBase.h>


#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TickBus.h>

#include <Utils/Utils.h>
#include <Utils/ImGuiSidebar.h>
#include <Utils/ImGuiMaterialDetails.h>
#include <Utils/ImGuiAssetBrowser.h>

#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>

namespace AtomSampleViewer
{
    class MeshExampleComponent final
        : public CommonSampleComponentBase
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT(MeshExampleComponent, "{A2402165-5DF1-4981-BF7F-665209640BBD}", CommonSampleComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        MeshExampleComponent();
        ~MeshExampleComponent() override = default;

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // AZ::EntityBus::MultiHandler
        void OnEntityDestruction(const AZ::EntityId& entityId) override;

        void ModelChange();

        void UseArcBallCameraController();
        void UseNoClipCameraController();
        void RemoveController();

        void SetArcBallControllerParams();
        void ResetCameraController();

        enum class CameraControllerType : int32_t 
        {
            ArcBall = 0,
            NoClip,
            Count
        };
        static const uint32_t CameraControllerCount = static_cast<uint32_t>(CameraControllerType::Count);
        static const char* CameraControllerNameTable[CameraControllerCount];
        CameraControllerType m_currentCameraControllerType = CameraControllerType::ArcBall;

        // Not owned by this sample, we look this up
        AZ::Component* m_cameraControlComponent = nullptr;

        AZ::Render::MeshFeatureProcessorInterface::ModelChangedEvent::Handler m_changedHandler;

        static constexpr float ArcballRadiusMinModifier = 0.01f;
        static constexpr float ArcballRadiusMaxModifier = 4.0f;
        
        AZ::RPI::Cullable::LodOverride m_lodOverride = AZ::RPI::Cullable::NoLodOverride;

        bool m_enableMaterialOverride = true;

        // If false, only azmaterials generated from ".material" files will be listed.
        // Otherwise, all azmaterials, regardless of its source (e.g ".fbx"), will
        // be shown in the material list.
        bool m_showModelMaterials = false;

        bool m_cameraControllerDisabled = false;

        AZ::Data::Instance<AZ::RPI::Material> m_materialOverrideInstance; //< Holds a copy of the material instance being used when m_enableMaterialOverride is true.
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;

        ImGuiSidebar m_imguiSidebar;
        ImGuiMaterialDetails m_imguiMaterialDetails;
        ImGuiAssetBrowser m_materialBrowser;
        ImGuiAssetBrowser m_modelBrowser;
    };
} // namespace AtomSampleViewer
