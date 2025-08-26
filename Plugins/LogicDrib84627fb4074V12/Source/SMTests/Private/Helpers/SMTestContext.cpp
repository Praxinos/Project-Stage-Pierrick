// Copyright Recursoft LLC. All Rights Reserved.

#include "SMTestContext.h"

float USMTestContext::GreaterThanTest = 5;

void USMTestContext::IncreaseUpdateInt(float Value)
{
	TestUpdateFromDeltaSecondsInt += FMath::RoundToInt(Value);
	TimesUpdateHit.Increase();
}

void USMTestContext::IncreaseTransitionInit()
{
	TestTransitionInit.Increase();
}

void USMTestContext::IncreaseTransitionShutdown()
{
	TestTransitionShutdown.Increase();
}

USMStateMachineTestComponent::USMStateMachineTestComponent(class FObjectInitializer const& ObjectInitializer) : Super(ObjectInitializer)
{
}

void USMStateMachineTestComponent::SetStateMachineClass(UClass* NewClass)
{
	StateMachineClass = NewClass;
}

void USMStateMachineTestComponent::ClearTemplateInstance()
{
	InstanceTemplate = nullptr;
}

void USMStateMachineTestComponent::SetAllowTick(bool bAllowOverride, bool bCanEverTick)
{
	bOverrideTick_DEPRECATED = bAllowOverride;
	bCanEverTick_DEPRECATED = bCanEverTick;
}

void USMStateMachineTestComponent::SetTickInterval(bool bAllowOverride, float TickInterval)
{
	bOverrideTickInterval_DEPRECATED = bAllowOverride;
	TickInterval_DEPRECATED = TickInterval;
}

void USMStateMachineTestComponent::ImportDeprecatedProperties_Public()
{
	ImportDeprecatedProperties();
}

void USMTestNotifyInstance::OnStateMachineStart_Implementation()
{
	Super::OnStateMachineStart_Implementation();
	LastToStateGuid.Invalidate();
	LastFromStateGuid.Invalidate();
	LastStateStartedGuid.Invalidate();
	LastTransitionGuid.Invalidate();
	TimesStateChangedHit = 0;
	TimesStateStartedHit = 0;
	TimesTransitionTakenHit = 0;
	TimesStateMachineStarted++;
}

void USMTestNotifyInstance::OnStateMachineStop_Implementation()
{
	Super::OnStateMachineStop_Implementation();
	TimesStateMachineStopped++;
}

void USMTestNotifyInstance::OnStateMachineStateChanged_Implementation(const FSMStateInfo& ToState,
                                                                      const FSMStateInfo& FromState)
{
	Super::OnStateMachineStateChanged_Implementation(ToState, FromState);
	
	ensureAlways(!(LastToStateGuid == ToState.Guid && LastFromStateGuid == FromState.Guid));
	LastToStateGuid = ToState.Guid;
	LastFromStateGuid = FromState.Guid;

	TimesStateChangedHit++;
}

void USMTestNotifyInstance::OnStateMachineStateStarted_Implementation(const FSMStateInfo& State)
{
	Super::OnStateMachineStateStarted_Implementation(State);

	if (bTestLastStateStarted)
	{
		ensureAlways(LastStateStartedGuid != State.Guid);
		LastStateStartedGuid = State.Guid;
	}
	
	TimesStateStartedHit++;
}

void USMTestNotifyInstance::OnStateMachineTransitionTaken_Implementation(const FSMTransitionInfo& Transition)
{
	Super::OnStateMachineTransitionTaken_Implementation(Transition);

	ensureAlways(LastTransitionGuid != Transition.Guid);
	LastTransitionGuid = Transition.Guid;

	TimesTransitionTakenHit++;
}
