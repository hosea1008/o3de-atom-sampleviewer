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

#include <AzFramework/API/ApplicationAPI_ios.h>
#import <UIKit/UIKit.h>

//[GFX TODO][ATOM-449] - Remove this file once we switch to unified launcher

////////////////////////////////////////////////////////////////////////////////////////////////////
@interface IosLumberyardApplicationDelegate : NSObject<UIApplicationDelegate>
{
}
@end    // IosLumberyardApplicationDelegate Interface

////////////////////////////////////////////////////////////////////////////////////////////////////
@implementation IosLumberyardApplicationDelegate

extern int ios_main();
////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)launchLumberyardApplication
{
    const int exitCode = ios_main();
    exit(exitCode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    [self performSelector:@selector(launchLumberyardApplication) withObject:nil afterDelay:0.0];
    return YES;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationWillResignActive:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnWillResignActive);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationDidEnterBackground:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnDidEnterBackground);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationWillEnterForeground:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnWillEnterForeground);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationDidBecomeActive:(UIApplication*)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnDidBecomeActive);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationWillTerminate:(UIApplication *)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnWillTerminate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
    EBUS_EVENT(AzFramework::IosLifecycleEvents::Bus, OnDidReceiveMemoryWarning);
}

@end 
