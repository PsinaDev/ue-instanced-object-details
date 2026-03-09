// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class IPropertyTypeIdentifier;

class FInstancedObjectDetailsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IPropertyTypeIdentifier> Identifier;
};
