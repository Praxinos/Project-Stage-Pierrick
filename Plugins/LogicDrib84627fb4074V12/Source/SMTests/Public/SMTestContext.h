// Copyright Recursoft LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SMNode_Info.h"
#include "SMStateInstance.h"
#include "SMConduitInstance.h"
#include "SMStateMachineComponent.h"
#include "SMTestContext.generated.h"

class USMInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTestDelegateSignature);

USTRUCT()
struct FSMTestData
{
	GENERATED_BODY()

	int32 Count = 0;
	double TimeStamp = 0.f;

	void Increase(bool bTakeTimeStamp = true)
	{
		Count++;
		if (bTakeTimeStamp)
		{
			TakeTimeStamp();
		}
	}

	void TakeTimeStamp()
	{
		TimeStamp = FPlatformTime::Seconds();
	}
};

UCLASS(NotBlueprintable, NotBlueprintable, Transient)
class ULambdaWrapper : public UObject
{
	GENERATED_BODY()

public:
	FSMTestData OnInitializeHit;
	FSMTestData OnStartHit;
	FSMTestData OnStoppedHit;
	FSMTestData OnShutdownHit;

	UFUNCTION()
	void OnInitialize(USMInstance* Instance)
	{
		OnInitializeHit.Increase();
	}

	UFUNCTION()
	void OnStart(USMInstance* Instance)
	{
		OnStartHit.Increase();
	}

	UFUNCTION()
	void OnStop(USMInstance* Instance)
	{
		OnStoppedHit.Increase();
	}

	UFUNCTION()
	void OnShutdown(USMInstance* Instance)
	{
		OnShutdownHit.Increase();
	}
};

UCLASS(Blueprintable)
class USMTestContext : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseEntryInt() { TestEntryInt++;}

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseUpdateInt(float Value);

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseEndInt() { TestEndInt++;}

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	int32 GetEntryInt() const { return TestEntryInt; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	int32 GetUpdateFromDeltaSecondsInt() const { return TestUpdateFromDeltaSecondsInt; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	int32 GetEndInt() const { return TestEndInt; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	bool CanTransition() const { return bCanTransition; }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionInit();

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionShutdown();

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionPreEval() { TestTransitionPreEval.Increase(); }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionPostEval() { TestTransitionPostEval.Increase(); }

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void IncreaseTransitionTaken() { TestTransitionEntered.Increase(); }

	/** Quick test for feeding in float data. */
	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	bool FloatGreaterThan(float Input) const
	{
		return Input > GreaterThanTest;
	}

	UFUNCTION(BlueprintCallable, Category = "State Machine Tests")
	void SetTestReference(USMInstance* Instance)
	{
		TestReference = Instance;
	}
	
	void Reset()
	{
		TestEntryInt = 0;
		TestTransitionsHit = 0;
		TestStatesHit = 0;
		bCanTransition = false;
	}

	UFUNCTION()
	void OnTransitionTaken(USMInstance* Instance, FSMTransitionInfo Transition)
	{
		TestTransitionsHit++;
	}

	UFUNCTION()
	void OnStateChanged(USMInstance* Instance, FSMStateInfo To, FSMStateInfo From)
	{
		TestStatesHit++;
	}

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestEntryInt = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestUpdateFromDeltaSecondsInt = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestEndInt = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestTransitionsHit = 0;

	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	int32 TestStatesHit = 0;
	
	UPROPERTY(BlueprintReadWrite, Category = "State Machine Tests")
	bool bCanTransition = true;

	UPROPERTY()
	FSMTestData TestTransitionInit;

	UPROPERTY()
	FSMTestData TestTransitionShutdown;

	UPROPERTY()
	FSMTestData TestTransitionPreEval;

	UPROPERTY()
	FSMTestData TestTransitionPostEval;

	UPROPERTY()
	FSMTestData TestTransitionEntered;

	FSMTestData TimesUpdateHit;

	UPROPERTY()
	TObjectPtr<USMInstance> TestReference;

	UPROPERTY(BlueprintAssignable, Category = "State Machine Tests")
	FTestDelegateSignature TransitionEvent;
	
	static float GreaterThanTest;
};

UCLASS(Blueprintable)
class USMStateTestInstance : public USMStateInstance
{
public:
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "State Machine Tests")
	FTestDelegateSignature StateEvent;
};

UCLASS(Blueprintable)
class USMStateTestInstance2 : public USMStateTestInstance
{
public:
	GENERATED_BODY()
};

UCLASS(Blueprintable)
class USMStateManualTransitionTestInstance : public USMStateTestInstance
{
public:
	GENERATED_BODY()

};

UCLASS(Blueprintable)
class USMStateMachineTestInstance : public USMStateMachineInstance
{
public:
	GENERATED_BODY()

};

UCLASS(Blueprintable)
class USMConduitTestInstance : public USMConduitInstance
{
public:
	GENERATED_BODY()
	
	UPROPERTY()
	int32 IntValue;
};

UCLASS(Blueprintable)
class USMTransitionTestInstance : public USMTransitionInstance
{
public:
	GENERATED_BODY()

	bool bCanTransition = false;
};

UCLASS()
class USMTestObject : public UObject
{
public:
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Test")
	FText CustomToText() const { return FText::FromString("Test serializer"); }

};

UCLASS()
class USMStateMachineTestComponent : public USMStateMachineComponent
{
	GENERATED_UCLASS_BODY()

public:
	void SetStateMachineClass(UClass* NewClass);
	void ClearTemplateInstance();
	
	void SetAllowTick(bool bAllowOverride, bool bCanEverTick);
	void SetTickInterval(bool bAllowOverride, float TickInterval);

	void ImportDeprecatedProperties_Public();
};

static void RecordTime(double& InOutVariable)
{
#if !PLATFORM_WINDOWS
	FPlatformProcess::Sleep(0.001f);
#endif
	InOutVariable = FPlatformTime::Cycles64();
}

UCLASS()
class USMTestNotifyInstance : public USMInstance
{
	GENERATED_BODY()

public:

	virtual void OnStateMachineStart_Implementation() override;
	virtual void OnStateMachineStop_Implementation() override;
	virtual void OnStateMachineStateChanged_Implementation(const FSMStateInfo& ToState, const FSMStateInfo& FromState) override;
	virtual void OnStateMachineStateStarted_Implementation(const FSMStateInfo& State) override;
	virtual void OnStateMachineTransitionTaken_Implementation(const FSMTransitionInfo& Transition) override;
	
	bool bTestLastStateStarted =  true;
	
	int32 TimesStateChangedHit = 0;
	int32 TimesStateStartedHit = 0;
	int32 TimesTransitionTakenHit = 0;
	int32 TimesStateMachineStarted = 0;
	int32 TimesStateMachineStopped = 0;
	
	FGuid LastFromStateGuid;
	FGuid LastToStateGuid;

	FGuid LastStateStartedGuid;

	FGuid LastTransitionGuid;
};