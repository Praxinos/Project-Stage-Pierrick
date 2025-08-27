// Copyright Recursoft LLC. All Rights Reserved.

#include "SMNode_Base.h"
#include "SMUtils.h"
#include "SMLogging.h"
#include "SMNodeInstance.h"
#include "SMRuntimeSettings.h"
#include "ExposedFunctions/SMExposedFunctionDefines.h"

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
#include "Misc/App.h"
#endif

#define LOGICDRIVER_FUNCTION_HANDLER_TYPE FSMNode_FunctionHandlers

#if WITH_EDITORONLY_DATA
bool FSMNode_Base::bValidateGuids = false;
#endif

FSMNode_Base::FSMNode_Base() : FunctionHandlers(nullptr), TimeInState(0), bIsInEndState(false),
                               bHasUpdated(false), DuplicateId(0),
                               OwnerNode(nullptr),
                               OwningInstance(nullptr),
                               NodeInstance(nullptr), NodeInstanceClass(nullptr),
                               ServerTimeInState(SM_ACTIVE_TIME_NOT_SET), bHaveGraphFunctionsInitialized(false),
                               bIsInitializedForRun(0),
                               bIsActive(false)
{
	/*
	 * Originally the Guid was initialized here. This caused warnings to show up during packaging because
	 * Unreal does safety checks on struct native constructors by comparing multiple initializations with different
	 * addresses and verifying each property matches. That doesn't work with a Guid because it is guaranteed to
	 * be unique each time.
	 */
}

void FSMNode_Base::Initialize(UObject* Instance)
{
	OwningInstance = Cast<USMInstance>(Instance);
	CreateNodeInstance();
}

void FSMNode_Base::InitializeFunctionHandlers()
{
	// InitializeFunctionHandlers must be implemented for each FSMNode type.
	unimplemented();

	// Define `FSM[NodeType]_FunctionHandlers` under FSMExposedNodeFunctions that gets set by the BP compiler.
	// Each implementation needs `#define LOGICDRIVER_FUNCTION_HANDLER_TYPE FSM[NodeType]_FunctionHandlers` set in the CPP.
	// Then each overload of InitializeFunctionHandlers just needs to call `INITIALIZE_NODE_FUNCTION_HANDLER();`
}

void FSMNode_Base::InitializeGraphFunctions()
{
	check(IsInGameThread());

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMNode_Base::InitializeFunctionHandlers"), STAT_SMNode_InitializeFunctionHandlers, STATGROUP_LogicDriver);
		check(OwningInstance);
		InitializeFunctionHandlers();
		checkf(FunctionHandlers != nullptr || &OwningInstance->GetRootStateMachine() == this,
			TEXT("Exposed functions not set for node `%s` in state machine `%s`. If this is a cooked build make sure you have cooked your assets since your last change."),
			*GetNodeName(), *OwningInstance->GetName());
	}

	INITIALIZE_EXPOSED_FUNCTIONS(OnRootStateMachineStartedGraphEvaluator);
	INITIALIZE_EXPOSED_FUNCTIONS(OnRootStateMachineStoppedGraphEvaluator);

	INITIALIZE_EXPOSED_FUNCTIONS(NodeInitializedGraphEvaluators);
	INITIALIZE_EXPOSED_FUNCTIONS(NodeShutdownGraphEvaluators);
	
	bHaveGraphFunctionsInitialized = true;
}

void FSMNode_Base::Reset()
{
	FunctionHandlers = nullptr;
}

void FSMNode_Base::OnStartedByInstance(USMInstance* Instance)
{
	if (Instance == GetOwningInstance())
	{
		EXECUTE_EXPOSED_FUNCTIONS(OnRootStateMachineStartedGraphEvaluator);
	}
}

void FSMNode_Base::OnStoppedByInstance(USMInstance* Instance)
{
	if (Instance == GetOwningInstance())
	{
		EXECUTE_EXPOSED_FUNCTIONS(OnRootStateMachineStoppedGraphEvaluator);
	}
}

const FGuid& FSMNode_Base::GetNodeGuid() const
{
	return Guid;
}

void FSMNode_Base::GenerateNewNodeGuid()
{
	SetNodeGuid(FGuid::NewGuid());
}

const FGuid& FSMNode_Base::GetGuid() const
{
	return PathGuid;
}

void FSMNode_Base::CalculatePathGuid(TMap<FString, int32>& InOutMappedPaths, bool bUseGuidCache)
{
	const USMInstance* PrimaryInstance = OwningInstance ? OwningInstance->GetPrimaryReferenceOwnerConst() : nullptr;

	static TMap<FGuid, FSMGuidMap> EmptyGuidCache;
	const TMap<FGuid, FSMGuidMap>& GuidCache = PrimaryInstance ? PrimaryInstance->GetRootPathGuidCache() : EmptyGuidCache;
	
	if (bUseGuidCache && PrimaryInstance && &OwningInstance->GetRootStateMachine() != this &&
		GuidCache.Num() > 0 /* Will be empty if caching is disabled. */)
	{
		if (const FSMGuidMap* NodeMap = GuidCache.Find(
			OwningInstance->GetRootStateMachine().GetGuid()))
		{
			if (const FGuid* CachedPathGuid = NodeMap->NodeToPathGuids.Find(GetNodeGuid()))
			{
				PathGuid = *CachedPathGuid;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
				// Only verify in debug builds as this is a very slow check
				if (
					FApp::GetBuildConfiguration() == EBuildConfiguration::DebugGame ||
					FApp::GetBuildConfiguration() == EBuildConfiguration::Debug
#if WITH_EDITORONLY_DATA
					|| bValidateGuids
#endif
					)
				{
					checkCode(
						FGuid ConfirmGuid;
						auto ConfirmMappedPaths = InOutMappedPaths;
						USMUtils::PathToGuid(GetGuidPath(ConfirmMappedPaths), &ConfirmGuid);
						check(ConfirmGuid == PathGuid);
					);
				}
#endif
			}
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (!PathGuid.IsValid())
		{
			if (OwningInstance == PrimaryInstance)
			{
				LD_LOG_WARNING(TEXT("Guid cache specified but none found for node '%s' in SMInstance '%s'. Try recompiling applicable blueprints."),
					*GetNodeName(), *OwningInstance->GetName())
			}
			else
			{
				LD_LOG_WARNING(TEXT("Guid cache specified but none found for node '%s' in SMInstance '%s' which has a primary reference owner of '%s'. Try recompiling applicable blueprints."),
					*GetNodeName(), *OwningInstance->GetName(), *PrimaryInstance->GetName())
			}

#if WITH_EDITORONLY_DATA
			if (bValidateGuids)
			{
				checkNoEntry();
			}
#endif
		}
#endif
	}
	else
	{
		PathGuid.Invalidate();
	}

	if (!PathGuid.IsValid())
	{
		USMUtils::PathToGuid(GetGuidPath(InOutMappedPaths), &PathGuid);
	}
}

FString FSMNode_Base::GetGuidPath(TMap<FString, int32>& InOutMappedPaths) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SMNode_Base::GetGuidPath"), STAT_SMNode_Base_GetGuidPath, STATGROUP_LogicDriver);
	TArray<const FSMNode_Base*> Owners;
	USMUtils::TryGetAllOwners(this, Owners);
	return USMUtils::BuildGuidPathFromNodes(Owners, &InOutMappedPaths);
}

FGuid FSMNode_Base::CalculatePathGuidConst() const
{
	TMap<FString, int32> PathToStateMachine;
	const FString Path = GetGuidPath(PathToStateMachine);
	return USMUtils::PathToGuid(Path);
}

void FSMNode_Base::GenerateNewNodeGuidIfNotSet()
{
	if (Guid.IsValid())
	{
		return;
	}

	GenerateNewNodeGuid();
}

void FSMNode_Base::SetNodeGuid(const FGuid& NewGuid)
{
	Guid = NewGuid;
}

void FSMNode_Base::SetOwnerNodeGuid(const FGuid& NewGuid)
{
	OwnerGuid = NewGuid;
}

void FSMNode_Base::SetOwnerNode(FSMNode_Base* Owner)
{
	OwnerNode = Owner;
}

void FSMNode_Base::CreateNodeInstance()
{
	if (!NodeInstanceClass)
	{
		SetNodeInstanceClass(GetDefaultNodeInstanceClass());
		check(NodeInstanceClass);
	}

	UObject* TemplateInstance = nullptr;
	if (TemplateName != NAME_None && OwningInstance)
	{
		TemplateInstance = USMUtils::FindTemplateFromInstance(OwningInstance, TemplateName);
		if (TemplateInstance == nullptr)
		{
			LD_LOG_ERROR(TEXT("Could not find node template %s for use on node %s from package %s. Loading defaults."), *TemplateName.ToString(), *GetNodeName(), *OwningInstance->GetName());
		}
	}

#if WITH_EDITORONLY_DATA
	if (TemplateInstance && OwningInstance && TemplateInstance->GetClass() != NodeInstanceClass && TemplateInstance->GetClass()->GetName().StartsWith("REINST_"))
	{
		LD_LOG_ERROR(TEXT("Node class mismatch. Node %s has template class %s but is expecting %s. Try recompiling the blueprint %s."),
			*GetNodeName(), *TemplateInstance->GetClass()->GetName(), *NodeInstanceClass->GetName(), *OwningInstance->GetName());
		return;
	}
#endif

	if (!CanEverCreateNodeInstance() ||
		(IsUsingDefaultNodeClass() && !GetDefault<USMRuntimeSettings>()->bPreloadDefaultNodes))
	{
		// Default node instances are created on demand.
		return;
	}
	
	NodeInstance = NewObject<USMNodeInstance>(OwningInstance, NodeInstanceClass, NAME_None, RF_NoFlags, TemplateInstance);
	NodeInstance->SetOwningNode(this);
}

void FSMNode_Base::SetNodeInstanceClass(UClass* NewNodeInstanceClass)
{
	if (NewNodeInstanceClass && !IsNodeInstanceClassCompatible(NewNodeInstanceClass))
	{
		LD_LOG_ERROR(TEXT("Could not set node instance class %s on node %s. The types are not compatible."), *NewNodeInstanceClass->GetName(), *GetNodeName());
		return;
	}

	NodeInstanceClass = NewNodeInstanceClass;
}

bool FSMNode_Base::IsNodeInstanceClassCompatible(UClass* NewNodeInstanceClass) const
{
	ensureMsgf(false, TEXT("FSMNode_Base IsNodeInstanceClassCompatible hit for node %s and instance class %s. This should always be overidden in child classes."),
		*GetNodeName(), NewNodeInstanceClass ? *NewNodeInstanceClass->GetName() : TEXT("None"));
	return false;
}

USMNodeInstance* FSMNode_Base::GetOrCreateNodeInstance()
{
	if (!NodeInstance && CanEverCreateNodeInstance())
	{
		if (!HaveGraphFunctionsInitialized())
		{
			LD_LOG_ERROR(TEXT("GetOrCreateNodeInstance called on node %s before it has initialized."), *GetNodeName());
			return nullptr;
		}

		if (!NodeInstanceClass)
		{
			LD_LOG_ERROR(TEXT("GetOrCreateNodeInstance called on node %s with null NodeInstanceClass."), *GetNodeName());
			return nullptr;
		}
	
		NodeInstance = NewObject<USMNodeInstance>(OwningInstance, NodeInstanceClass, NAME_None, RF_NoFlags);
		NodeInstance->SetOwningNode(this);
	}
	
	return NodeInstance;
}

void FSMNode_Base::SetNodeName(const FString& Name)
{
	NodeName = Name;
}

void FSMNode_Base::SetTemplateName(const FName& Name)
{
	TemplateName = Name;
}

FName FSMNode_Base::GetTemplateName(USMNodeInstance* InNodeInstance) const
{
	if (InNodeInstance == nullptr || InNodeInstance == NodeInstance)
	{
		return TemplateName;
	}

	return NAME_None;
}

void FSMNode_Base::ExecuteInitializeNodes()
{
	if (IsInitializedForRun())
	{
		return;
	}
	
	EXECUTE_EXPOSED_FUNCTIONS(NodeInitializedGraphEvaluators);
	bIsInitializedForRun = true;
}

void FSMNode_Base::ExecuteShutdownNodes()
{
	EXECUTE_EXPOSED_FUNCTIONS(NodeShutdownGraphEvaluators);
	bIsInitializedForRun = false;
}

void FSMNode_Base::SetServerTimeInState(float InTime)
{
	ServerTimeInState = InTime;
}

#if WITH_EDITOR

void FSMNode_Base::ResetGeneratedValues()
{
	PathGuid.Invalidate();
}

#endif

void FSMNode_Base::PrepareGraphExecution()
{
	if (!HaveGraphFunctionsInitialized())
	{
		return;
	}

	UpdateReadStates();
}

void FSMNode_Base::SetActive(bool bValue)
{
#if WITH_EDITORONLY_DATA
	bWasActive = bIsActive;
#endif
	bIsActive = bValue;
}

#undef LOGICDRIVER_FUNCTION_HANDLER_TYPE
