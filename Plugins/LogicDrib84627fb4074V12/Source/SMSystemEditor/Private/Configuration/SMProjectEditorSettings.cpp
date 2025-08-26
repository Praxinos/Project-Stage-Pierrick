// Copyright Recursoft LLC. All Rights Reserved.

#include "SMProjectEditorSettings.h"

#include "SMInstance.h"

USMProjectEditorSettings::USMProjectEditorSettings()
{
	InstalledVersion = "";
	bUpdateAssetsOnStartup = true;
	bDisplayAssetUpdateProgress = true;
	bDisplayUpdateNotification = true;
	
	bDisplayMemoryLimitsOnCompile = true;
	bAlwaysDisplayStructMemoryUsage = false;
	StructMemoryLimitWarningThreshold = 0.9f;
	
	bRestrictInvalidCharacters = true;
	bCalculateGuidsOnCompile = true;
	bWarnIfChildrenAreOutOfDate = true;
	bLinkerLoadHandling = true;
	
	bDefaultNewTransitionsToTrue = false;
	bDefaultNewConduitsToTrue = false;
	bConfigureNewConduitsAsTransitions = true;
	
	DefaultStateMachineBlueprintParentClass = USMInstance::StaticClass();
	DefaultStateMachineBlueprintNamePrefix = TEXT("BP_");

	bEnableReferenceTemplatesByDefault = false;
}

void USMProjectEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (DefaultStateMachineBlueprintParentClass.IsNull())
	{
		DefaultStateMachineBlueprintParentClass = USMInstance::StaticClass();
	}
	
	SaveConfig();
}
