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

#include <Automation/AssetStatusTracker.h>
#include <AzFramework/StringFunc/StringFunc.h>

namespace AtomSampleViewer
{
    AssetStatusTracker::~AssetStatusTracker()
    {
        AzFramework::AssetSystemInfoBus::Handler::BusDisconnect();
    }

    void AssetStatusTracker::StartTracking()
    {
        if (!m_isTracking)
        {
            AzFramework::AssetSystemInfoBus::Handler::BusConnect();
        }

        m_isTracking = true;

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        m_allAssetStatusData.clear();
    }

    void AssetStatusTracker::StopTracking()
    {
        if (m_isTracking)
        {
            m_isTracking = false;

            AzFramework::AssetSystemInfoBus::Handler::BusDisconnect();

            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            m_allAssetStatusData.clear();
        }
    }

    void AssetStatusTracker::ExpectAsset(AZStd::string sourceAssetPath, uint32_t expectedCount)
    {
        AzFramework::StringFunc::Path::Normalize(sourceAssetPath);
        AZStd::to_lower(sourceAssetPath.begin(), sourceAssetPath.end());

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        m_allAssetStatusData[sourceAssetPath].m_expecteCount += expectedCount;
    }

    bool AssetStatusTracker::DidExpectedAssetsFinish() const
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);

        for (auto& assetData : m_allAssetStatusData)
        {
            const AZStd::string& assetPath = assetData.first;
            const AssetStatusEvents& status = assetData.second;

            if (status.m_expecteCount > (status.m_succeeded + status.m_failed))
            {
                return false;
            }
        }

        return true;
    }

    void AssetStatusTracker::AssetCompilationStarted(const AZStd::string& assetPath)
    {
        AZ_TracePrintf("Automation", "AssetCompilationStarted(%s)\n", assetPath.c_str());

        AZStd::string normalizedAssetPath = assetPath;
        AzFramework::StringFunc::Path::Normalize(normalizedAssetPath);
        AZStd::to_lower(normalizedAssetPath.begin(), normalizedAssetPath.end());

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        m_allAssetStatusData[normalizedAssetPath].m_started++;
    }

    void AssetStatusTracker::AssetCompilationSuccess(const AZStd::string& assetPath)
    {
        AZ_TracePrintf("Automation", "AssetCompilationSuccess(%s)\n", assetPath.c_str());

        AZStd::string normalizedAssetPath = assetPath;
        AzFramework::StringFunc::Path::Normalize(normalizedAssetPath);
        AZStd::to_lower(normalizedAssetPath.begin(), normalizedAssetPath.end());

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        m_allAssetStatusData[normalizedAssetPath].m_succeeded++;
    }

    void AssetStatusTracker::AssetCompilationFailed(const AZStd::string& assetPath)
    {
        AZ_TracePrintf("Automation", "AssetCompilationFailed(%s)\n", assetPath.c_str());

        AZStd::string normalizedAssetPath = assetPath;
        AzFramework::StringFunc::Path::Normalize(normalizedAssetPath);
        AZStd::to_lower(normalizedAssetPath.begin(), normalizedAssetPath.end());

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        m_allAssetStatusData[normalizedAssetPath].m_failed++;
    }


} // namespace AtomSampleViewer
