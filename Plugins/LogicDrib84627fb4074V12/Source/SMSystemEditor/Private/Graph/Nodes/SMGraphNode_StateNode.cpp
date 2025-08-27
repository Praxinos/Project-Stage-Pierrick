// Copyright Recursoft LLC. All Rights Reserved.

#include "SMGraphNode_StateNode.h"

#include "SMGraphNode_LinkStateNode.h"
#include "Blueprints/SMBlueprintEditor.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Graph/SMGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/Schema/SMStateGraphSchema.h"
#include "RootNodes/SMGraphK2Node_StateUpdateNode.h"
#include "RootNodes/SMGraphK2Node_StateEndNode.h"
#include "RootNodes/SMGraphK2Node_IntermediateNodes.h"
#include "RootNodes/SMGraphK2Node_TransitionInitializedNode.h"
#include "RootNodes/SMGraphK2Node_TransitionShutdownNode.h"

#include "SMGraphNode_TransitionEdge.h"
#include "SMUnrealTypeDefs.h"

#include "SMUtils.h"

#include "Kismet2/Kismet2NameValidators.h"
#include "UObject/UObjectThreadContext.h"

#define LOCTEXT_NAMESPACE "SMGraphStateNode"

class FSMStateNodeNameValidator : public FStringSetNameValidator
{
public:
	FSMStateNodeNameValidator(const USMGraphNode_StateNodeBase* InStateNode)
		: FStringSetNameValidator(FString())
	{
		TArray<USMGraphNode_StateNodeBase*> Nodes;
		USMGraph* StateMachine = CastChecked<USMGraph>(InStateNode->GetOuter());

		StateMachine->GetNodesOfClass<USMGraphNode_StateNodeBase>(Nodes);
		for (auto NodeIt = Nodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			USMGraphNode_StateNodeBase* Node = *NodeIt;
			if (Node != InStateNode)
			{
				Names.Add(Node->GetStateName());
			}
		}
	}

	// Begin FSMStateNodeNameValidator
	virtual EValidatorResult IsValid(const FString& Name, bool bOriginal) override
	{
		EValidatorResult Result = FStringSetNameValidator::IsValid(Name, bOriginal);

		if (Result == EValidatorResult::Ok)
		{
			if (Name.Len() > 100)
			{
				Result = EValidatorResult::TooLong;
			}
			else if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bRestrictInvalidCharacters)
			{
				FText Reason;
				if (!FName(Name).IsValidXName(Reason, LD_INVALID_STATENAME_CHARACTERS))
				{
					Result = EValidatorResult::ContainsInvalidCharacters;
				}
				else if (Name.Compare(TEXT("Sequencer"), ESearchCase::IgnoreCase) == 0)
				{
					// Can cause problems during copy & paste.
					Result = EValidatorResult::LocallyInUse;
				}
			}
		}

		return Result;
	}
	// End FSMStateNodeNameValidator
};

USMGraphNode_StateNodeBase::USMGraphNode_StateNodeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bAlwaysUpdate_DEPRECATED(false), bDisableTickTransitionEvaluation_DEPRECATED(false),
	  bEvalTransitionsOnStart_DEPRECATED(false), bExcludeFromAnyState_DEPRECATED(false),
	  bCanTransitionToSelf(false)
{
}

void USMGraphNode_StateNodeBase::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

FText USMGraphNode_StateNodeBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString StateName = GetStateName();
	return FText::FromString(StateName);
}

bool USMGraphNode_StateNodeBase::GetCanRenameNode() const
{
	return Super::GetCanRenameNode();
}

void USMGraphNode_StateNodeBase::OnRenameNode(const FString& NewName)
{
	Super::OnRenameNode(NewName);

	for (const TObjectPtr<USMGraphNode_LinkStateNode>& LinkState : LinkStates)
	{
		if (LinkState)
		{
			LinkState->OnLinkedStateRenamed();
		}
	}
}

void USMGraphNode_StateNodeBase::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		UEdGraphPin* InputPin = GetInputPin();

		if (InputPin && GetSchema()->TryCreateConnection(FromPin, InputPin))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}

void USMGraphNode_StateNodeBase::PostPlacedNewNode()
{
	SetToCurrentVersion();
	
	// Create a new state machine graph
	check(BoundGraph == nullptr);
	BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
		this,
		NAME_None,
		USMStateGraph::StaticClass(),
		USMStateGraphSchema::StaticClass());
	check(BoundGraph);

	// Find an interesting name
	TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, TEXT("State"));

	// Initialize the state machine graph
	const UEdGraphSchema* Schema = BoundGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*BoundGraph);

	// Add the new graph as a child of our parent graph
	UEdGraph* ParentGraph = GetGraph();

	// TODO: This search is slow on large state machines!
	if (ParentGraph->SubGraphs.Find(BoundGraph) == INDEX_NONE)
	{
		ParentGraph->Modify();
		ParentGraph->SubGraphs.Add(BoundGraph);
	}

	if (bGenerateTemplateOnNodePlacement)
	{
		InitTemplate();
	}
}

void USMGraphNode_StateNodeBase::PostPasteNode()
{
	RemoveInvalidLinkStates();
	
	// Look for a unique name. We have to perform manual handling for this before passing it off to Unreal.
	// Every copy resets the unique name count, so State_1 pasted might be State_1_1. This makes it so it is State_2.
	// t-155
	const FString UniqueName = FSMBlueprintEditorUtils::FindUniqueName(GetStateName(), GetOwningStateMachineGraph());
	
	const TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(this);
	FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, UniqueName);
	
	TArray<UEdGraphNode*> ContainedNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<UEdGraphNode>(BoundGraph, ContainedNodes);

	for (UEdGraphNode* GraphNode : ContainedNodes)
	{
		GraphNode->CreateNewGuid();
		GraphNode->PostPasteNode();
		// Required to correct context display issues.
		GraphNode->ReconstructNode();
	}

	Super::PostPasteNode();

	bRequestInitialAnimation = true;
}

void USMGraphNode_StateNodeBase::DestroyNode()
{
	Modify();
	if (BoundGraph)
	{
		BoundGraph->Modify();
	}

	for (auto It = LinkStates.CreateIterator(); It; ++It)
	{
		if ((*It)->GetLinkedState() == this)
		{
			(*It)->LinkToState(FString());
		}
	}
	
	UEdGraph* GraphToRemove = BoundGraph;

	BoundGraph = nullptr;
	Super::DestroyNode();

	if (GraphToRemove)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(this);
		FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphToRemove, EGraphRemoveFlags::Recompile);
	}
}

TSharedPtr<INameValidatorInterface> USMGraphNode_StateNodeBase::MakeNameValidator() const
{
	return MakeShareable(new FSMStateNodeNameValidator(this));
}

void USMGraphNode_StateNodeBase::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void USMGraphNode_StateNodeBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
}

void USMGraphNode_StateNodeBase::ImportDeprecatedProperties()
{
	Super::ImportDeprecatedProperties();

	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		StateInstance->SetAlwaysUpdate(bAlwaysUpdate_DEPRECATED);
		StateInstance->SetDisableTickTransitionEvaluation(bDisableTickTransitionEvaluation_DEPRECATED);
		StateInstance->SetEvalTransitionsOnStart(bEvalTransitionsOnStart_DEPRECATED);
		StateInstance->SetExcludeFromAnyState(bExcludeFromAnyState_DEPRECATED);
	}
}

void USMGraphNode_StateNodeBase::PreCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::PreCompile(CompilerContext);

	// Verify linked states are correct and cleanup if necessary.
	RemoveInvalidLinkStates();
}

void USMGraphNode_StateNodeBase::SetRuntimeDefaults(FSMState_Base& State) const
{
	State.SetNodeName(GetStateName());

	if (const USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		State.bAlwaysUpdate = StateInstance->GetAlwaysUpdate();
		State.bDisableTickTransitionEvaluation = StateInstance->GetDisableTickTransitionEvaluation();
		State.bEvalTransitionsOnStart = StateInstance->GetEvalTransitionsOnStart();
		State.bCanBeEndState = StateInstance->GetCanBeEndState();
	}
}

bool USMGraphNode_StateNodeBase::IsEndState(bool bCheckAnyState) const
{
	if (const USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		if (!StateInstance->GetCanBeEndState())
		{
			return false;
		}
	}
	
	// Must have entry.
	if (!HasInputConnections())
	{
		return false;
	}

	// Check Any States since they add transitions to this node on compile.
	if (bCheckAnyState && FSMBlueprintEditorUtils::IsNodeImpactedFromAnyStateNode(this))
	{
		return false;
	}

	// If no output definitely end state.
	if (const UEdGraphPin* OutputPin = GetOutputPin())
	{
		if (OutputPin->LinkedTo.Num() == 0)
		{
			return true;
		}

		for (const UEdGraphPin* Pin : OutputPin->LinkedTo)
		{
			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode()))
			{
				// Transitioning to self doesn't count.
				if (Transition->GetFromState() == Transition->GetToState())
				{
					continue;
				}

				// There has to be some way out of here...
				if (Transition->PossibleToTransition())
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool USMGraphNode_StateNodeBase::HasInputConnections() const
{
	if (UEdGraphPin* Pin = GetInputPin())
	{
		if (Pin->LinkedTo.Num() == 0)
		{
			for (const TObjectPtr<USMGraphNode_LinkStateNode>& LinkState : LinkStates)
			{
				if (LinkState && LinkState->HasInputConnections())
				{
					return true;
				}
			}

			return false;
		}

		for (const UEdGraphPin* InputPin : Pin->LinkedTo)
		{
			if (InputPin->GetOwningNode()->IsA<USMGraphNode_StateMachineEntryNode>())
			{
				return true;
			}

			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InputPin->GetOwningNode()))
			{
				// Ignore self and input connections which can't transition.
				if (Transition->GetFromState() == Transition->GetToState() || !Transition->PossibleToTransition())
				{
					continue;
				}

				return true;
			}
		}
	}

	return false;
}

bool USMGraphNode_StateNodeBase::HasOutputConnections() const
{
	if (UEdGraphPin* Pin = GetOutputPin())
	{
		return Pin->LinkedTo.Num() > 0;
	}

	return false;
}

bool USMGraphNode_StateNodeBase::ShouldExcludeFromAnyState() const
{
	if (USMStateInstance_Base* StateInstance = Cast<USMStateInstance_Base>(GetNodeTemplate()))
	{
		return StateInstance->GetExcludeFromAnyState();
	}

	return false;
}

bool USMGraphNode_StateNodeBase::HasTransitionToNode(const UEdGraphNode* Node) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		for (const UEdGraphPin* Pin : OutputPin->LinkedTo)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode()))
			{
				if (Transition->GetToState() == Node)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool USMGraphNode_StateNodeBase::HasTransitionFromNode(const UEdGraphNode* Node) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		for (const UEdGraphPin* Pin : InputPin->LinkedTo)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (const USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(Pin->GetOwningNode()))
			{
				if (Transition->GetFromState() == Node)
				{
					return true;
				}
			}
		}
	}

	return false;
}

USMGraphNode_Base* USMGraphNode_StateNodeBase::GetPreviousNode(int32 Index) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		if (InputPin->LinkedTo.Num() <= Index || InputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		return Cast<USMGraphNode_Base>(InputPin->LinkedTo[Index]->GetOwningNodeUnchecked());
	}

	return nullptr;
}

USMGraphNode_Base* USMGraphNode_StateNodeBase::GetNextNode(int32 Index) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		if (OutputPin->LinkedTo.Num() <= Index || OutputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		return Cast<USMGraphNode_Base>(OutputPin->LinkedTo[Index]->GetOwningNodeUnchecked());
	}

	return nullptr;
}

USMGraphNode_StateNodeBase* USMGraphNode_StateNodeBase::GetPreviousState(int32 Index /*= 0*/, bool bIncludeReroute /*= false*/, bool bIncludeEntryState /*= false*/) const
{
	if (USMGraphNode_TransitionEdge* Transition = GetPreviousTransition(Index))
	{
		return Transition->GetFromState();
	}

	if (bIncludeEntryState)
	{
		return Cast<USMGraphNode_StateMachineEntryNode>(GetPreviousNode(Index));
	}

	return nullptr;
}

USMGraphNode_StateNodeBase* USMGraphNode_StateNodeBase::GetNextState(int32 Index /*= 0*/, bool bIncludeReroute /*= false*/) const
{
	if (USMGraphNode_TransitionEdge* Transition = GetNextTransition(Index))
	{
		return Transition->GetToState();
	}

	return nullptr;
}

USMGraphNode_TransitionEdge* USMGraphNode_StateNodeBase::GetPreviousTransition(int32 Index) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		if (InputPin->LinkedTo.Num() <= Index || InputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InputPin->LinkedTo[Index]->GetOwningNode()))
		{
			return Transition;
		}
	}
	return nullptr;
}

USMGraphNode_TransitionEdge* USMGraphNode_StateNodeBase::GetNextTransition(int32 Index) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		if (OutputPin->LinkedTo.Num() <= Index || OutputPin->LinkedTo[Index] == nullptr)
		{
			return nullptr;
		}

		if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(OutputPin->LinkedTo[Index]->GetOwningNode()))
		{
			return Transition;
		}
	}

	return nullptr;
}

void USMGraphNode_StateNodeBase::GetInputTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions) const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		for (int32 Idx = 0; Idx < InputPin->LinkedTo.Num(); ++Idx)
		{
			if (InputPin->LinkedTo[Idx] == nullptr)
			{
				// Could be null during a paste.
				continue;
			}
			if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(InputPin->LinkedTo[Idx]->GetOwningNode()))
			{
				OutTransitions.AddUnique(Transition);
			}
		}
	}
}

void USMGraphNode_StateNodeBase::GetOutputTransitions(TArray<USMGraphNode_TransitionEdge*>& OutTransitions) const
{
	if (UEdGraphPin* OutputPin = GetOutputPin())
	{
		for (int32 Idx = 0; Idx < OutputPin->LinkedTo.Num(); ++Idx)
		{
			if (OutputPin->LinkedTo[Idx] == nullptr)
			{
				// Could be null during a paste.
				continue;
			}
			if (USMGraphNode_TransitionEdge* Transition = Cast<USMGraphNode_TransitionEdge>(OutputPin->LinkedTo[Idx]->GetOwningNode()))
			{
				OutTransitions.AddUnique(Transition);
			}
		}
	}
}

int32 USMGraphNode_StateNodeBase::GetNumInputConnections() const
{
	if (const UEdGraphPin* InputPin = GetInputPin())
	{
		return InputPin->LinkedTo.Num();
	}

	return 0;
}

int32 USMGraphNode_StateNodeBase::GetNumOutputConnections() const
{
	if (const UEdGraphPin* OutputPin = GetOutputPin())
	{
		return OutputPin->LinkedTo.Num();
	}

	return 0;
}

UEdGraphPin* USMGraphNode_StateNodeBase::GetConnectedEntryPin() const
{
	if (UEdGraphPin* InputPin = GetInputPin())
	{
		for (int32 Idx = 0; Idx < InputPin->LinkedTo.Num(); ++Idx)
		{
			if (InputPin->LinkedTo[Idx]->GetOwningNode()->IsA<USMGraphNode_StateMachineEntryNode>())
			{
				return InputPin->LinkedTo[Idx];
			}
		}
	}

	return nullptr;
}

FLinearColor USMGraphNode_StateNodeBase::GetBackgroundColorForNodeInstance(const USMNodeInstance* NodeInstance) const
{
	const USMEditorSettings* Settings = FSMBlueprintEditorUtils::GetEditorSettings();
	const FLinearColor* CustomColor = GetCustomBackgroundColor(NodeInstance);
	const FLinearColor ColorModifier = !CustomColor ? FLinearColor(0.6f, 0.6f, 0.6f, 0.5f) : *CustomColor;
	const FLinearColor EndStateColor = !CustomColor ? Settings->EndStateColor * ColorModifier : CastChecked<USMStateInstance_Base>(NodeInstance)->GetEndStateColor();

	if (IsEndState())
	{
		return EndStateColor;
	}

	const FLinearColor DefaultColor = Settings->StateDefaultColor;

	// No input -- node unreachable.
	if (!HasInputConnections())
	{
		return DefaultColor * ColorModifier;
	}

	// State is active
	if (FSMBlueprintEditorUtils::GraphHasAnyLogicConnections(BoundGraph))
	{
		return CustomColor ? *CustomColor * FLinearColor(1.f, 1.f, 1.f, 1.2f) : Settings->StateWithLogicColor * ColorModifier;
	}

	return DefaultColor * ColorModifier;
}

void USMGraphNode_StateNodeBase::RemoveInvalidLinkStates()
{
	for (auto It = LinkStates.CreateIterator(); It; ++It)
	{
		if (!It->Get()
			|| (It->Get()->GetLinkedState() != this)
			|| (GetGraph() && !GetGraph()->Nodes.Contains(*It)))
		{
			It.RemoveCurrent();
		}
	}
}

FLinearColor USMGraphNode_StateNodeBase::Internal_GetBackgroundColor() const
{
	return GetBackgroundColorForNodeInstance(NodeInstanceTemplate);
}

USMGraphNode_StateNode::USMGraphNode_StateNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USMGraphNode_StateNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Enable templates
	bool bStateChange = false;
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USMGraphNode_StateNode, StateClass))
	{
		InitTemplate();
		bStateChange = true;
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (bStateChange && IsSafeToConditionallyCompile(PropertyChangedEvent.ChangeType))
	{
		FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(this), false);
	}
}

UObject* USMGraphNode_StateNode::GetJumpTargetForDoubleClick() const
{
	return Super::GetJumpTargetForDoubleClick();
}

void USMGraphNode_StateNode::PlaceDefaultInstanceNodes()
{
	Super::PlaceDefaultInstanceNodes();
}

void USMGraphNode_StateNode::SetNodeClass(UClass* Class)
{
	StateClass = Class;
	Super::SetNodeClass(Class);
}

const FSlateBrush* USMGraphNode_StateNode::GetNodeIcon() const
{
	if (const FSlateBrush* Icon = Super::GetNodeIcon())
	{
		return Icon;
	}

	return FSMUnrealAppStyle::Get().GetBrush(TEXT("Graph.StateNode.Icon"));
}

void USMGraphNode_StateNode::InitTemplate()
{
	Super::InitTemplate();
}

void USMGraphNode_StateNode::OnCompile(FSMKismetCompilerContext& CompilerContext)
{
	Super::OnCompile(CompilerContext);
}

#undef LOCTEXT_NAMESPACE
