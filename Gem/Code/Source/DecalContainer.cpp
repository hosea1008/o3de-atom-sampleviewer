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

#include "DecalContainer.h"
#include <AzCore/Math/Vector3.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>


namespace AtomSampleViewer
{
    namespace
    {
        static const char* const DecalMaterialNames[] =
        {
            "materials/Decal/scorch_01_decal.azmaterial",
            "materials/Decal/brushstoke_01_decal.azmaterial",
            "materials/Decal/am_road_dust_decal.azmaterial",
            "materials/Decal/am_mud_decal.azmaterial",
            "materials/Decal/airship_nose_number_decal.azmaterial",
            "materials/Decal/airship_tail_01_decal.azmaterial",
            "materials/Decal/airship_tail_02_decal.azmaterial",
            "materials/Decal/airship_symbol_decal.azmaterial",
        };
    }

    DecalContainer::DecalContainer(AZ::Render::DecalFeatureProcessorInterface* fp)
        : m_decalFeatureProcessor(fp)
    {
        SetupDecals();
    }

    void DecalContainer::SetupDecals()
    {
        const float HalfLength = 0.25f;
        const float HalfProjectionDepth = 10.0f;
        const AZ::Vector3 halfSize(HalfLength, HalfLength, HalfProjectionDepth);
        SetupNewDecal(AZ::Vector3(-0.75f, -0.25f, 1), halfSize, DecalMaterialNames[0]);
        SetupNewDecal(AZ::Vector3(-0.25f, -0.25f, 1), halfSize, DecalMaterialNames[1]);
        SetupNewDecal(AZ::Vector3(0.25f, -0.25f, 1), halfSize, DecalMaterialNames[2]);
        SetupNewDecal(AZ::Vector3(0.75f, -0.25f, 1), halfSize, DecalMaterialNames[3]);
        SetupNewDecal(AZ::Vector3(-0.75f, 0.25f, 1), halfSize, DecalMaterialNames[4]);
        SetupNewDecal(AZ::Vector3(-0.25f, 0.25f, 1), halfSize, DecalMaterialNames[5]);
        SetupNewDecal(AZ::Vector3(0.25f, 0.25f, 1), halfSize, DecalMaterialNames[6]);
        SetupNewDecal(AZ::Vector3(0.75f, 0.25f, 1), halfSize, DecalMaterialNames[7]);
    }

    DecalContainer::~DecalContainer()
    {
        SetNumDecalsActive(0);
    }

    void DecalContainer::SetNumDecalsActive(int numDecals)
    {
        for (int i = 0; i < aznumeric_cast<int>(m_decals.size()); ++i)
        {
            if (i < numDecals)
            {
                AcquireDecal(i);
            }
            else
            {
                ReleaseDecal(i);
            }
        }
        m_numDecalsActive = numDecals;
    }

    void DecalContainer::SetupNewDecal(const AZ::Vector3 position, const AZ::Vector3 halfSize, const char* const decalMaterialName)
    {
        Decal newDecal;
        newDecal.m_position = position;
        newDecal.m_halfSize = halfSize;
        newDecal.m_materialName = decalMaterialName;

        m_decals.push_back(newDecal);
    }

    void DecalContainer::AcquireDecal(int i)
    {
        Decal& decal = m_decals[i];
        if (decal.m_decalHandle.IsValid())
        {
            return;
        }

        decal.m_decalHandle = m_decalFeatureProcessor->AcquireDecal();
        m_decalFeatureProcessor->SetDecalHalfSize(decal.m_decalHandle, decal.m_halfSize);
        m_decalFeatureProcessor->SetDecalPosition(decal.m_decalHandle, decal.m_position);

        const AZ::Data::AssetId assetId = AZ::RPI::AssetUtils::GetAssetIdForProductPath(decal.m_materialName);
        m_decalFeatureProcessor->SetDecalMaterial(decal.m_decalHandle, assetId);
    }

    void DecalContainer::ReleaseDecal(int i)
    {
        Decal& decal = m_decals[i];
        if (decal.m_decalHandle.IsNull())
        {
            return;
        }

        m_decalFeatureProcessor->ReleaseDecal(decal.m_decalHandle);
        decal.m_decalHandle = AZ::Render::DecalFeatureProcessorInterface::DecalHandle::Null;
    }
}
