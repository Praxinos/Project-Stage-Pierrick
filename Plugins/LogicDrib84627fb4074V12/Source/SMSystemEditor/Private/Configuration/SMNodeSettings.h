// Copyright Recursoft LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMNodeSettings.generated.h"

UENUM()
enum class ESMEditorConstructionScriptProjectSetting : uint8
{
	/** Run when the blueprint is modified or compiled. */
	SM_Standard	UMETA(DisplayName = "Standard"),
	/** Run only on blueprint compile. */
	SM_Compile UMETA(DisplayName = "Compile"),
	/** Run only on initial instantiation. This option limits overall functionality and support is being phased out. */
	SM_Legacy UMETA(DisplayName = "Legacy")
};