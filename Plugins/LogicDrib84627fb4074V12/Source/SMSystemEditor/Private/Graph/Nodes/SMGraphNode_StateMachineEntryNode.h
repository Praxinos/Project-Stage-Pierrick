// Copyright Recursoft LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SMStateMachine.h"
#include "SMGraphNode_StateNode.h"
#include "RootNodes/SMGraphK2Node_RuntimeNodeContainer.h"
#include "SMGraphNode_StateMachineEntryNode.generated.h"

/** Created for normal state machine UEdGraphs. */
UCLASS(MinimalAPI, HideCategories = (Class, Display))
class USMGraphNode_StateMachineEntryNode : public USMGraphNode_StateNodeBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	FSMStateMachine StateMachineNode;
	
	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PostPlacedNewNode() override;
	virtual void PostPasteNode() override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~UEdGraphNode
	
	// USMGraphNode_Base
	virtual bool CanExistAtRuntime() const override { return false; }
	// ~USMGraphNode_Base
};

/** Created by compiler for nested state machine entry points. */
UCLASS(MinimalAPI)
class USMGraphK2Node_StateMachineEntryNode : public USMGraphK2Node_RuntimeNodeContainer
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "State Machines")
	FSMStateMachine StateMachineNode;

	virtual FSMNode_Base* GetRunTimeNode()  override { return &StateMachineNode; }

	// UEdGraphNode
	virtual void AllocateDefaultPins() override;
	// ~UEdGraphNode
};
