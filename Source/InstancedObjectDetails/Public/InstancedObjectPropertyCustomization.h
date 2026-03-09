// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"

class IPropertyHandle;
class IPropertyUtilities;
class IDetailChildrenBuilder;
class FDetailWidgetRow;
class IPropertyTypeCustomizationUtils;

/**
 * Identifier that matches only FObjectPropertyBase with CPF_InstancedReference.
 */
class FInstancedObjectPropertyIdentifier : public IPropertyTypeIdentifier
{
public:
	virtual bool IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const override;
};

/**
 * Customization for instanced UObject properties.
 *
 * Header: default class picker + "Show In Content Browser" button
 *         (visible only when the assigned class is a Blueprint).
 * Children: filters by CPF_Edit / CPF_DisableEditOnInstance, groups by category.
 */
class FInstancedObjectPropertyCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	/** Checks if the currently assigned object is a Blueprint-generated class. */
	bool IsCurrentValueBlueprintClass() const;

	/** Navigates Content Browser to the Blueprint asset of the current value. */
	void BrowseToBlueprint() const;

	/**
	 * Returns true if the property should be visible on an instanced object.
	 * Requires CPF_Edit and no CPF_DisableEditOnInstance.
	 */
	static bool ShouldShowProperty(const FProperty* Property);

	/**
	 * Recursively walks property handle tree, skipping intermediate nodes
	 * (object nodes, category nodes), collecting leaf property handles.
	 */
	static void GatherLeafProperties(
		TSharedRef<IPropertyHandle> ParentHandle,
		TArray<TSharedRef<IPropertyHandle>>& OutProperties);

	TSharedPtr<IPropertyHandle> CachedPropertyHandle;
};
