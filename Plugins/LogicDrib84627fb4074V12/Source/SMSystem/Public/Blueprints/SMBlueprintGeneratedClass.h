// Copyright Recursoft LLC. All Rights Reserved.

#pragma once

#include "Engine/BlueprintGeneratedClass.h"

#include "UObject/Package.h" // Required for non-unity packaging in UE 5.2.

#include "SMBlueprintGeneratedClass.generated.h"

UCLASS()
class SMSYSTEM_API USMBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

	friend class FSMKismetCompilerContext;
	
public:
	// UClass
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	// ~UClass

	/** The root state machine Guid- set by the compiler. */
	void SetRootGuid(const FGuid& Guid);

	/** The root state machine Guid. */
	const FGuid& GetRootGuid() const { return RootGuid; }

	/** Retrieve stored sub object templates for this generated class. */
	const TMap<FName, TObjectPtr<UObject>>& GetSubObjectTemplates() const { return SubObjectTemplates; }
	
#if WITH_EDITORONLY_DATA
	/** Used for testing to validate determinism. */
	TArray<FString> GeneratedNames;

	/** This class may have stale parent graph data and needs to be regenerated. */
	UPROPERTY()
	uint8 bHasStaleParentData: 1;
#endif

protected:
	UPROPERTY(meta=(BlueprintCompilerGeneratedDefaults))
	FGuid RootGuid;

private:
	/*
	 * Archetype objects used for instantiating node instances and references.
	 * Originally stored on the instance CDO to support the now deprecated blueprint nativization, it has since
	 * been moved to the generated class.
	 */
	UPROPERTY(Instanced, meta=(BlueprintCompilerGeneratedDefaults))
	TMap<FName, TObjectPtr<UObject>> SubObjectTemplates;
};

UCLASS()
class SMSYSTEM_API USMNodeBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

};