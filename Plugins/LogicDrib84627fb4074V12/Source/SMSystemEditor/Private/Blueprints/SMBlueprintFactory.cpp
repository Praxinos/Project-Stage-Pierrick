// Copyright Recursoft LLC. All Rights Reserved.

#include "SMBlueprintFactory.h"

#include "SMAssetClassFilter.h"
#include "Graph/SMGraphK2.h"
#include "Graph/Schema/SMGraphK2Schema.h"
#include "UI/SMNewAssetDialogueOption.h"
#include "UI/SSMAssetPickerList.h"
#include "UI/SSMNewAssetDialog.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMVersionUtils.h"

#include "SMConduitInstance.h"
#include "SMInstance.h"
#include "SMStateMachineInstance.h"
#include "SMTransitionInstance.h"
#include "Blueprints/SMBlueprint.h"

#include "BlueprintEditorSettings.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMBlueprintFactory"

USMBlueprintFactory::FOnGetNewAssetDialogOptions USMBlueprintFactory::OnGetNewAssetDialogOptionsEvent;

USMBlueprintFactory::USMBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), BlueprintType(), SelectedBlueprintToCopy(nullptr), SelectedClassForParent(nullptr)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USMBlueprint::StaticClass();
	ParentClass = USMInstance::StaticClass();
}

bool USMBlueprintFactory::ConfigureProperties()
{
	SelectedBlueprintToCopy = nullptr;
	SelectedClassForParent = nullptr;

	if (!bParentClassOverridden)
	{
		UClass* DefaultParentClass = FSMBlueprintEditorUtils::GetProjectEditorSettings()->DefaultStateMachineBlueprintParentClass.LoadSynchronous();
		ParentClass = DefaultParentClass ? DefaultParentClass : USMInstance::StaticClass();
	}
	
	if (!bDisplayDialog)
	{
		return true;
	}

	const IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	const TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	const TSharedPtr<SSMAssetPickerList> ClassPicker = SNew(SSMAssetPickerList)
	.AssetPickerMode(SSMAssetPickerList::EAssetPickerMode::ClassPicker);

	const TSharedPtr<SSMAssetPickerList> AssetPicker = SNew(SSMAssetPickerList)
	.AssetPickerMode(SSMAssetPickerList::EAssetPickerMode::AssetPicker)
	.OnItemDoubleClicked(SSMAssetPickerList::FOnItemDoubleClicked::CreateLambda([this]()
	{
		if (NewAssetDialog.IsValid())
		{
			NewAssetDialog->TryConfirmSelection();
		}
	}));

	TArray<FSMNewAssetDialogOption> DialogOptions
	{
		FSMNewAssetDialogOption(
			LOCTEXT("CreateEmptyLabel", "Create New State Machine"),
			LOCTEXT("CreateEmptyDescription", "Create an empty state machine blueprint."),
			LOCTEXT("EmptyLabel", "New State Machine"),
			FSMNewAssetDialogOption::FOnCanContinue(),
			FSMNewAssetDialogOption::FOnCanContinue(),
			FSMNewAssetDialogOption::FOnCanContinue(),
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoOptionsLabel", "No Options"))
			]),

		FSMNewAssetDialogOption(
			LOCTEXT("CreateChildLabel", "Create Child State Machine"),
			LOCTEXT("CreateChildDescription", "Select a parent state machine blueprint to inherit from."),
			LOCTEXT("ParentAssetSelectLabel", "Select a Parent State Machine"),
			FSMNewAssetDialogOption::FOnCanContinue::CreateUObject(this,
				&USMBlueprintFactory::OnCanSelectStateMachineAsset, ENewAssetType::Parent, ClassPicker),
				FSMNewAssetDialogOption::FOnCanContinue(),
				FSMNewAssetDialogOption::FOnCanContinue::CreateUObject(this,
				&USMBlueprintFactory::OnStateMachineAssetSelectionConfirmed, ENewAssetType::Parent, ClassPicker),
			ClassPicker.ToSharedRef()),

		FSMNewAssetDialogOption(
			LOCTEXT("CreateFromExistingLabel", "Copy Existing State Machine"),
			LOCTEXT("CreateFromExistingDescription", "Duplicate an existing state machine blueprint to a new asset. Does not deep copy references."),
			LOCTEXT("ExistingAssetSelectLabel", "Select a State Machine"),
			FSMNewAssetDialogOption::FOnCanContinue::CreateUObject(this,
				&USMBlueprintFactory::OnCanSelectStateMachineAsset, ENewAssetType::Duplicate, AssetPicker),
				FSMNewAssetDialogOption::FOnCanContinue(),
				FSMNewAssetDialogOption::FOnCanContinue::CreateUObject(this,
					&USMBlueprintFactory::OnStateMachineAssetSelectionConfirmed, ENewAssetType::Duplicate, AssetPicker),
			AssetPicker.ToSharedRef())
	};

	// Allow external callers to add their own options.
	{
		TArray<FSMNewAssetDialogOption> ExternalOptions;
		OnGetNewAssetDialogOptionsEvent.Broadcast(ExternalOptions);

		DialogOptions.Append(MoveTemp(ExternalOptions));
	}

	SAssignNew(NewAssetDialog, SSMNewAssetDialog, LOCTEXT("AssetTypeName", "State Machine"), MoveTemp(DialogOptions));
	FSlateApplication::Get().AddModalWindow(NewAssetDialog.ToSharedRef(), ParentWindow);

	if (NewAssetDialog.IsValid() && NewAssetDialog->GetUserConfirmedSelection() == false)
	{
		// User cancelled or closed the dialog so abort asset creation.
		return false;
	}

	return true;
}

UObject* USMBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a SM Blueprint, then create and init one
	check(Class->IsChildOf(USMBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}
	else if (SelectedClassForParent)
	{
		ParentClass = SelectedClassForParent;
	}

	if (!ParentClass || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(USMInstance::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != nullptr) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateStateMachineBlueprint", "Cannot create a State Machine Blueprint based on the class '{ClassName}'."), Args));
		return nullptr;
	}

	USMBlueprint* NewStateMachineBP = SelectedBlueprintToCopy ?
		NewStateMachineBP = CastChecked<USMBlueprint>(StaticDuplicateObject(SelectedBlueprintToCopy, InParent, Name)) :
	CastChecked<USMBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType,
		USMBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

	return NewStateMachineBP;
}

UObject* USMBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

bool USMBlueprintFactory::DoesSupportClass(UClass* Class)
{
	return Class == USMBlueprint::StaticClass();
}

FString USMBlueprintFactory::GetDefaultNewAssetName() const
{
	if (SelectedBlueprintToCopy)
	{
		const FString DesiredName = SelectedBlueprintToCopy->GetName() + TEXT("_Copy");
		const bool bExists = StaticFindObjectFast(SelectedBlueprintToCopy->GetClass(), SelectedBlueprintToCopy->GetPackage(), *DesiredName) != nullptr;
		const FName UniqueName = bExists ? MakeUniqueObjectName(SelectedBlueprintToCopy->GetPackage(), SelectedBlueprintToCopy->GetClass(), *DesiredName) : *DesiredName;
		return UniqueName.ToString();
	}
	if (SelectedClassForParent)
	{
		FString DesiredName = SelectedClassForParent->GetName();
		DesiredName.RemoveFromEnd(TEXT("_C"));
		DesiredName += TEXT("_Child");
		const bool bExists = StaticFindObjectFast(SelectedClassForParent->GetClass(), SelectedClassForParent->GetPackage(), *DesiredName) != nullptr;
		const FName UniqueName = bExists ? MakeUniqueObjectName(SelectedClassForParent->GetPackage(), SelectedClassForParent->GetClass(), *DesiredName) : *DesiredName;
		return UniqueName.ToString();
	}
	
	return FString::Printf(TEXT("%sStateMachine"), *FSMBlueprintEditorUtils::GetProjectEditorSettings()->DefaultStateMachineBlueprintNamePrefix);
}

void USMBlueprintFactory::CreateGraphsForBlueprintIfMissing(USMBlueprint* Blueprint)
{
	if (FSMBlueprintEditorUtils::GetTopLevelStateMachineGraph(Blueprint) == nullptr)
	{
		CreateGraphsForNewBlueprint(Blueprint);
	}
}

void USMBlueprintFactory::CreateGraphsForNewBlueprint(USMBlueprint* Blueprint)
{
	// New blueprints should always be on the latest version.
	FSMVersionUtils::SetToLatestVersion(Blueprint);
	
	// Locate the blueprint's event graph or create a new one.
	UEdGraph* EventGraph = FindObject<UEdGraph>(Blueprint, *(UEdGraphSchema_K2::GN_EventGraph.ToString()));

	if (!EventGraph)
	{
#if WITH_EDITORONLY_DATA
		if (Blueprint->UbergraphPages.Num())
		{
			FBlueprintEditorUtils::RemoveGraphs(Blueprint, Blueprint->UbergraphPages);
		}
#endif
		EventGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, UEdGraphSchema_K2::GN_EventGraph, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

		FBlueprintEditorUtils::AddUbergraphPage(Blueprint, EventGraph);
		EventGraph->bAllowDeletion = false;

		const UEdGraphSchema* EventGraphSchema = EventGraph->GetSchema();
		EventGraphSchema->CreateDefaultNodesForGraph(*EventGraph);
	}

	UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
	if (Settings && Settings->bSpawnDefaultBlueprintNodes)
	{
		// Create default events.
		const int32 NodePositionX = 255;
		int32 NodePositionY = 0;
		
		// OnStateMachineStart
		UK2Node_Event* OnStateMachineStartedNode = FKismetEditorUtilities::AddDefaultEventNode(Blueprint, EventGraph, GET_FUNCTION_NAME_CHECKED(USMInstance, OnStateMachineStart), USMInstance::StaticClass(), NodePositionY);
		if (USMGraphK2Schema::GetThenPin(OnStateMachineStartedNode)->LinkedTo.Num() == 0)
		{
			FSMBlueprintEditorUtils::CreateParentFunctionCall(EventGraph, USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, OnStateMachineStart)), OnStateMachineStartedNode, NodePositionX);
		}
		
		// Tick
		UK2Node_Event* TickFunctionNode = FKismetEditorUtilities::AddDefaultEventNode(Blueprint, EventGraph, GET_FUNCTION_NAME_CHECKED(USMInstance, Tick), USMInstance::StaticClass(), NodePositionY);
		if (USMGraphK2Schema::GetThenPin(TickFunctionNode)->LinkedTo.Num() == 0)
		{
			FSMBlueprintEditorUtils::CreateParentFunctionCall(EventGraph, USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, Tick)), TickFunctionNode, NodePositionX);
		}

		int32 SafeXPosition = 0;
		int32 SafeYPosition = 0;

		if (EventGraph->Nodes.Num() != 0)
		{
			// Place right under OnStateMachineStart node.
			SafeXPosition = EventGraph->Nodes[0]->NodePosX;
			SafeYPosition = EventGraph->Nodes[0]->NodePosY + EventGraph->Nodes[0]->NodeHeight + 70;
		}

		// Add a getter for the context for the state machine.
		UK2Node_CallFunction* GetOwnerNode = NewObject<UK2Node_CallFunction>(EventGraph);
		UFunction* MakeNodeFunction = USMInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USMInstance, GetContext));
		GetOwnerNode->CreateNewGuid();
		GetOwnerNode->PostPlacedNewNode();
		GetOwnerNode->SetFromFunction(MakeNodeFunction);
		GetOwnerNode->SetFlags(RF_Transactional);
		GetOwnerNode->AllocateDefaultPins();
		GetOwnerNode->NodePosX = SafeXPosition;
		GetOwnerNode->NodePosY = SafeYPosition;
		UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);
		GetOwnerNode->MakeAutomaticallyPlacedGhostNode();

		EventGraph->AddNode(GetOwnerNode);
	}

	// Default top level state machine graph
	USMGraphK2* NewTopLevelGraph = CastChecked<USMGraphK2>(FBlueprintEditorUtils::CreateNewGraph(Blueprint, USMGraphK2Schema::GN_StateMachineDefinitionGraph, USMGraphK2::StaticClass(), USMGraphK2Schema::StaticClass()));
	NewTopLevelGraph->bAllowDeletion = false;
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewTopLevelGraph);

	const UEdGraphSchema* StateMachineGraphSchema = NewTopLevelGraph->GetSchema();
	StateMachineGraphSchema->CreateDefaultNodesForGraph(*NewTopLevelGraph);

	// Set the first graph to the new state machine.
	TArray<USMGraphK2Node_StateMachineNode*> StateMachineNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<USMGraphK2Node_StateMachineNode>(Blueprint, StateMachineNodes);
	check(StateMachineNodes.Num() == 1);

	USMGraph* StateMachineGraph = StateMachineNodes[0]->GetStateMachineGraph();
	Blueprint->LastEditedDocuments.Reset();
	Blueprint->LastEditedDocuments.Add(StateMachineGraph);
}

void USMBlueprintFactory::SetParentClass(TSubclassOf<USMInstance> InNewParent)
{
	ParentClass = InNewParent.Get() ? InNewParent.Get() : USMInstance::StaticClass();
	bParentClassOverridden = ParentClass != nullptr;
}

bool USMBlueprintFactory::OnCanSelectStateMachineAsset(ENewAssetType InNewAssetType, const TSharedPtr<SSMAssetPickerList> InAssetPicker) const
{
	if (InNewAssetType == ENewAssetType::Duplicate)
	{
		return InAssetPicker->GetSelectedAssets().Num() > 0;
	}

	if (InNewAssetType == ENewAssetType::Parent)
	{
		return InAssetPicker->GetSelectedClasses().Num() > 0;
	}

	return false;
}

bool USMBlueprintFactory::OnStateMachineAssetSelectionConfirmed(ENewAssetType InNewAssetType,
	const TSharedPtr<SSMAssetPickerList> InAssetPicker)
{
	switch(InNewAssetType)
	{
	case ENewAssetType::Parent:
		{
			const TArray<UClass*> SelectedClasses = InAssetPicker->GetSelectedClasses();
			if (SelectedClasses.Num() > 0)
			{
				SelectedClassForParent = SelectedClasses[0];
				SelectedBlueprintToCopy = nullptr;
			}
			break;
		}
	case ENewAssetType::Duplicate:
		{
			const TArray<FAssetData>& SelectedAssets = InAssetPicker->GetSelectedAssets();

			if (SelectedAssets.Num() > 0)
			{
				const FAssetData& SelectedAsset = SelectedAssets[0];

				SelectedBlueprintToCopy = Cast<USMBlueprint>(SelectedAsset.GetAsset());
				SelectedClassForParent = nullptr;
			}
			break;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
