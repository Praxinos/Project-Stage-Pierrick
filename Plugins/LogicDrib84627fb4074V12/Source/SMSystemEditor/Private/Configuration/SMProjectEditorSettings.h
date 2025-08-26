// Copyright Recursoft LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMProjectEditorSettings.generated.h"

class USMInstance;

UCLASS(config = Editor, defaultconfig)
class SMSYSTEMEDITOR_API USMProjectEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USMProjectEditorSettings();

	UPROPERTY(config, VisibleAnywhere, Category = "Version Updates (Lite)")
	FString InstalledVersion;
	
	/**
	 * Automatically update assets saved by older versions to the most current version. It is strongly recommended to leave this on.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates (Lite)")
	bool bUpdateAssetsOnStartup;

	/**
	 * Display a progress bar when updating assets to a new version.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates (Lite)", meta = (EditCondition = "bUpdateAssetsOnStartup"))
	bool bDisplayAssetUpdateProgress;

	/**
	 * Display a popup with a link to the patch notes when a new version is detected.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Version Updates (Lite)")
	bool bDisplayUpdateNotification;

	/**
	 * Warn if approaching Blueprint memory limits on a compile.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory")
	bool bDisplayMemoryLimitsOnCompile;

	/**
	 * Display the used struct memory as an info message on compile. 
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory", meta = (EditCondition = "bDisplayMemoryLimitsOnCompile"))
	bool bAlwaysDisplayStructMemoryUsage;
	
	/**
	 * The percent of used struct memory that must be reached before a warning is triggered.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation|Memory", meta = (EditCondition = "bDisplayMemoryLimitsOnCompile", UIMin = 0.0, UIMax = 1.0))
	float StructMemoryLimitWarningThreshold;

	/**
	 * Restrict invalid characters in state names. When false any character is allowed, but certain operations can cause Unreal to crash, such
	 * as copying and pasting states. Only set to false if you know what you are doing.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation")
	bool bRestrictInvalidCharacters;

	/**
	 * Children which reference a parent state machine graph risk being out of date if a package the parent references is modified.
	 * Compiling the package will signal that affected state machine children need to be compiled, however if you start a PIE session
	 * instead of pressing the compile button, the children may not be updated. In this case the state machine compiler will attempt to warn you.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Compile")
	bool bWarnIfChildrenAreOutOfDate;

	/**
	 * Calculate path guids during compile when possible reducing run-time initialization time. This requires the state machine to be partially
	 * instantiated during compile.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Compile")
	bool bCalculateGuidsOnCompile;

	/**
	* Perform special compile handling when linker load is detected to avoid possible crashes and improve sub-object packaging.
	* This should remain on.
	*
	* This setting exists primarily for troubleshooting and will likely be removed in a future update.
	*/
	UPROPERTY(config, EditAnywhere, Category = "Compile", AdvancedDisplay)
	bool bLinkerLoadHandling;
	
	/**
	 * Newly placed transitions will default to true if they do not have a node class assigned.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Transitions")
	bool bDefaultNewTransitionsToTrue;

	/**
	 * Newly placed conduits will default to true if they do not have a node class assigned.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Conduits")
	bool bDefaultNewConduitsToTrue;
	
	/**
	 * Newly placed conduits will automatically be configured as transitions.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Conduits")
	bool bConfigureNewConduitsAsTransitions;

	/**
	 * Default parent class to be assigned when creating new state machine blueprints (SMInstance).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Blueprints")
	TSoftClassPtr<USMInstance> DefaultStateMachineBlueprintParentClass;

	/**
	 * The default prefix to use when creating a new state machine blueprint.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Blueprints")
	FString DefaultStateMachineBlueprintNamePrefix;
	
	/**
	 * Newly placed state machine references will have their templates enabled by default.
	 * This allows custom node classes to be supported with references.
	 *
	 * State machine blueprints that have a custom root state machine node class assigned in their class defaults
	 * will always default to using a template, regardless of this setting.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Node Instances")
	bool bEnableReferenceTemplatesByDefault;

	// UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~UObject
};
