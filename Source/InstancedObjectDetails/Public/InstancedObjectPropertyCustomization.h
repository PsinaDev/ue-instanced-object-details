// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"

class IPropertyHandle;
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
 * Children: re-groups the sub-object's leaves into nested categories.
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
	/**
	 * Returns true if the property should be visible.
	 * Requires CPF_Edit; EditInstanceOnly is dropped only in template context.
	 */
	static bool ShouldShowProperty(const FProperty* Property, bool bTemplateContext);

	/**
	 * Recursively walks property handle tree, skipping intermediate nodes
	 * (object nodes, category nodes), collecting leaf property handles.
	 */
	static void GatherLeafProperties(
		TSharedRef<IPropertyHandle> ParentHandle,
		TArray<TSharedRef<IPropertyHandle>>& OutProperties,
		bool bTemplateContext);

	/**
	 * Returns true when the edited instanced sub-object is a CDO/archetype or lives inside one.
	 * Assets are not templates: a UDataAsset such as UInputAction is a plain object instance.
	 */
	static bool ResolveTemplateContext(const IPropertyHandle& PropertyHandle);
};
