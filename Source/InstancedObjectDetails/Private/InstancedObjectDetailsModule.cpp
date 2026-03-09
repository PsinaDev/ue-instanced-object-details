// Copyright PsinaDev. All Rights Reserved.

#include "InstancedObjectDetailsModule.h"
#include "InstancedObjectPropertyCustomization.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "InstancedObjectBPFilter"

void FInstancedObjectDetailsModule::StartupModule()
{
	Identifier = MakeShared<FInstancedObjectPropertyIdentifier>();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		UObject::StaticClass()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInstancedObjectPropertyCustomization::MakeInstance),
		Identifier
	);
}

void FInstancedObjectDetailsModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(UObject::StaticClass()->GetFName(), Identifier);
	}

	Identifier.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInstancedObjectDetailsModule, InstancedObjectDetails)
