//
//  SerializeSourcemap.cc
//  drafter
//
//  Created by Pavan Kumar Sunkara on 18/01/15.
//  Copyright (c) 2015 Apiary Inc. All rights reserved.
//

#include "SerializeSourcemap.h"

using namespace drafter;

using snowcrash::SourceMapBase;
using snowcrash::SourceMap;
using snowcrash::Collection;
using snowcrash::DataStructure;
using snowcrash::Asset;
using snowcrash::Payload;
using snowcrash::Header;
using snowcrash::Parameters;
using snowcrash::Parameter;
using snowcrash::Value;
using snowcrash::TransactionExample;
using snowcrash::Request;
using snowcrash::Response;
using snowcrash::Action;
using snowcrash::Resource;
using snowcrash::Element;
using snowcrash::Description;
using snowcrash::Blueprint;
using snowcrash::Metadata;

/**
 * \brief functor pattern to translate Collection<> into sos::Array on serialization 
 *
 * usage:
 *
 *  sos::Array elements = WrapCollection<mson::Elements>()(getSomeListOfElements(), WrapMSONElement));
 *
 *  operator()(const T& collection, Functor &wrapper)
 *  \param collection - it come typicaly from snowcrash
 *  \param wrapper - adaptee element before push to collection
 *         you have to write your own, for example \see SeriallizeAST.cc
 *
 */

template<typename T, typename R = sos::Array>
struct WrapSourceMapCollection {
    typedef Collection<SourceMap<T> > collection_type;
    typedef typename collection_type::const_iterator iterator_type;

    template<typename Arg1, typename Functor>
    R operator()(const Arg1& collection, Functor &wrapper) const {
        R array;
        for( iterator_type it = collection.begin() ; it != collection.end() ; ++it ) {
            array.push(wrapper(*it));
        }
        return array;
    }

};

sos::Array WrapSourcemap(const SourceMapBase& value)
{
    sos::Array sourceMap;

    for (mdp::RangeSet<mdp::BytesRange>::const_iterator it = value.sourceMap.begin();
         it != value.sourceMap.end();
         ++it) {

        sos::Array sourceMapRow;

        sourceMapRow.push(sos::Number(it->location));
        sourceMapRow.push(sos::Number(it->length));

        sourceMap.push(sourceMapRow);
    }

    return sourceMap;
}

// Forward declarations
sos::Array WrapTypeSectionsSourcemap(const SourceMap<mson::TypeSections>& typeSections);
sos::Array WrapElementsSourcemap(const SourceMap<mson::Elements>& elements);

sos::Object WrapPropertyMemberSourcemap(const SourceMap<mson::PropertyMember>& propertyMember)
{
    sos::Object propertyMemberObject;

    // Name
    propertyMemberObject.set(SerializeKey::Name, WrapSourcemap(propertyMember.name));

    // Description
    propertyMemberObject.set(SerializeKey::Description, WrapSourcemap(propertyMember.description));

    // Value Definition
    propertyMemberObject.set(SerializeKey::ValueDefinition, WrapSourcemap(propertyMember.valueDefinition));

    // Type Sections
    propertyMemberObject.set(SerializeKey::Sections, WrapTypeSectionsSourcemap(propertyMember.sections));

    return propertyMemberObject;
}

sos::Object WrapValueMemberSourcemap(const SourceMap<mson::ValueMember>& valueMember)
{
    sos::Object valueMemberObject;

    // Description
    valueMemberObject.set(SerializeKey::Description, WrapSourcemap(valueMember.description));

    // Value Definition
    valueMemberObject.set(SerializeKey::ValueDefinition, WrapSourcemap(valueMember.valueDefinition));

    // Type Sections
    valueMemberObject.set(SerializeKey::Sections, WrapTypeSectionsSourcemap(valueMember.sections));

    return valueMemberObject;
}

sos::Array WrapMixinSourcemap(const SourceMap<mson::Mixin>& mixin)
{
    return WrapSourcemap(mixin);
}

sos::Array WrapOneOf(const SourceMap<mson::OneOf>& oneOf)
{
    return WrapElementsSourcemap(oneOf);
}

sos::Base WrapElementSourcemap(const SourceMap<mson::Element>& element)
{
    if (!element.elements().collection.empty()) {
        // Same for oneOf
        return WrapElementsSourcemap(element.elements());
    }
    else if (!element.mixin.sourceMap.empty()) {
        return WrapMixinSourcemap(element.mixin);
    }
    else if (!element.value.empty()) {
        return WrapValueMemberSourcemap(element.value);
    }
    else if (!element.property.empty()) {
        return WrapPropertyMemberSourcemap(element.property);
    }

    return sos::Null();
}

sos::Array WrapElementsSourcemap(const SourceMap<mson::Elements>& elements)
{
    return WrapSourceMapCollection<mson::Element>()(elements.collection, WrapElementSourcemap);
}

sos::Array WrapTypeSectionsSourcemap(const SourceMap<mson::TypeSections>& sections)
{
    sos::Array sectionsArray;

    for (Collection<SourceMap<mson::TypeSection> >::const_iterator it = sections.collection.begin();
         it != sections.collection.end();
         ++it) {

        if (!it->description.sourceMap.empty()) {
            sectionsArray.push(WrapSourcemap(it->description));
        }
        else if (!it->value.sourceMap.empty()) {
            sectionsArray.push(WrapSourcemap(it->value));
        }
        else if (!it->elements().collection.empty()) {
            sectionsArray.push(WrapElementsSourcemap(it->elements()));
        }
    }

    return sectionsArray;
}

sos::Object WrapNamedTypeSourcemap(const SourceMap<mson::NamedType>& namedType)
{
    sos::Object namedTypeObject;

    // Name
    namedTypeObject.set(SerializeKey::Name, WrapSourcemap(namedType.name));

    // Type Definition
    namedTypeObject.set(SerializeKey::TypeDefinition, WrapSourcemap(namedType.typeDefinition));

    // Type Sections
    namedTypeObject.set(SerializeKey::Sections, WrapTypeSectionsSourcemap(namedType.sections));

    return namedTypeObject;
}

sos::Object WrapDataStructureSourcemap(const SourceMap<DataStructure>& dataStructure)
{
    sos::Object dataStructureObject;

    // Name
    dataStructureObject.set(SerializeKey::Name, WrapSourcemap(dataStructure.name));

    // Type Definition
    dataStructureObject.set(SerializeKey::TypeDefinition, WrapSourcemap(dataStructure.typeDefinition));

    // Type Sections
    dataStructureObject.set(SerializeKey::Sections, WrapTypeSectionsSourcemap(dataStructure.sections));

    return dataStructureObject;
}

sos::Object WrapAssetSourcemap(const SourceMap<Asset>& asset)
{
    sos::Object assetObject;

    // Content
    assetObject.set(SerializeKey::Content, WrapSourcemap(asset));

    return assetObject;
}

sos::Object WrapPayloadSourcemap(const SourceMap<Payload>& payload)
{
    sos::Object payloadObject;

    // Reference
    if (!payload.reference.sourceMap.empty()) {
        payloadObject.set(SerializeKey::Reference, WrapSourcemap(payload.reference));
    }

    // Name
    payloadObject.set(SerializeKey::Name, WrapSourcemap(payload.name));

    // Description
    payloadObject.set(SerializeKey::Description, WrapSourcemap(payload.description));

    // Headers
    payloadObject.set(SerializeKey::Headers, 
                      WrapSourceMapCollection<Header>()(payload.headers.collection, WrapSourcemap));

    // Body
    payloadObject.set(SerializeKey::Body, WrapSourcemap(payload.body));

    // Schema
    payloadObject.set(SerializeKey::Schema, WrapSourcemap(payload.schema));

    // Content
    sos::Array content;

    /// Attributes
    if (!payload.attributes.empty()) {
        content.push(WrapDataStructureSourcemap(payload.attributes));
    }

    /// Asset 'bodyExample'
    if (!payload.body.sourceMap.empty()) {
        content.push(WrapAssetSourcemap(payload.body));
    }

    /// Asset 'bodySchema'
    if (!payload.schema.sourceMap.empty()) {
        content.push(WrapAssetSourcemap(payload.schema));
    }

    payloadObject.set(SerializeKey::Content, content);

    return payloadObject;
}

sos::Array WrapParametersSourcemap(const SourceMap<Parameters>& parameters)
{
    sos::Array parametersArray;

    for (Collection<SourceMap<Parameter> >::const_iterator it = parameters.collection.begin();
         it != parameters.collection.end();
         ++it) {

        sos::Object parameter;

        // Name
        parameter.set(SerializeKey::Name, WrapSourcemap(it->name));

        // Description
        parameter.set(SerializeKey::Description, WrapSourcemap(it->description));

        // Type
        parameter.set(SerializeKey::Type, WrapSourcemap(it->type));

        // Use
        parameter.set(SerializeKey::Required, WrapSourcemap(it->use));

        // Example Value
        parameter.set(SerializeKey::Example, WrapSourcemap(it->exampleValue));

        // Default Value
        parameter.set(SerializeKey::Default, WrapSourcemap(it->defaultValue));

        // Values
        sos::Array values;

        for (Collection<SourceMap<Value> >::const_iterator valIt = it->values.collection.begin();
             valIt != it->values.collection.end();
             ++valIt) {

            sos::Object value;

            value.set(SerializeKey::Value, WrapSourcemap(*valIt));

            values.push(value);
        }

        parameter.set(SerializeKey::Values, values);
    }

    return parametersArray;
}

sos::Object WrapTransactionExampleSourcemap(const SourceMap<TransactionExample>& example)
{
    sos::Object exampleObject;

    // Name
    exampleObject.set(SerializeKey::Name, WrapSourcemap(example.name));

    // Description
    exampleObject.set(SerializeKey::Description, WrapSourcemap(example.description));

    // Requests
    exampleObject.set(SerializeKey::Requests, 
                      WrapSourceMapCollection<Request>()(example.requests.collection, WrapPayloadSourcemap));

    // Responses
    exampleObject.set(SerializeKey::Responses, 
                      WrapSourceMapCollection<Response>()(example.responses.collection, WrapPayloadSourcemap));

    return exampleObject;
}

sos::Object WrapActionSourcemap(const SourceMap<Action>& action)
{
    sos::Object actionObject;

    // Name
    actionObject.set(SerializeKey::Name, WrapSourcemap(action.name));

    // Description
    actionObject.set(SerializeKey::Description, WrapSourcemap(action.description));

    // HTTP Method
    actionObject.set(SerializeKey::Method, WrapSourcemap(action.method));

    // Parameters
    actionObject.set(SerializeKey::Parameters, WrapParametersSourcemap(action.parameters));

    // Transaction Examples
    actionObject.set(SerializeKey::Examples, 
                     WrapSourceMapCollection<TransactionExample>()(action.examples.collection, WrapTransactionExampleSourcemap));

    // Content
    sos::Array content;

    /// Attributes
    if (!action.attributes.empty()) {
        content.push(WrapDataStructureSourcemap(action.attributes));
    }

    actionObject.set(SerializeKey::Content, content);

    return actionObject;
}

sos::Object WrapResourceSourcemap(const SourceMap<Resource>& resource)
{
    sos::Object resourceObject;

    // Name
    resourceObject.set(SerializeKey::Name, WrapSourcemap(resource.name));

    // Description
    resourceObject.set(SerializeKey::Description, WrapSourcemap(resource.description));

    // URI Template
    resourceObject.set(SerializeKey::URITemplate, WrapSourcemap(resource.uriTemplate));

    // Model
    sos::Object model = (resource.model.name.sourceMap.empty() ? sos::Object() : WrapPayloadSourcemap(resource.model));
    resourceObject.set(SerializeKey::Model, model);

    // Parameters
    resourceObject.set(SerializeKey::Parameters, WrapParametersSourcemap(resource.parameters));

    // Actions
    resourceObject.set(SerializeKey::Actions, 
                       WrapSourceMapCollection<Action>()(resource.actions.collection, WrapActionSourcemap));

    // Content
    sos::Array content;

    /// Attributes
    if (!resource.attributes.empty()) {
        content.push(WrapDataStructureSourcemap(resource.attributes));
    }

    resourceObject.set(SerializeKey::Content, content);

    return resourceObject;
}

sos::Object WrapResourceGroupSourcemap(const SourceMap<Element>& resourceGroup)
{
    sos::Object resourceGroupObject;

    // Name
    resourceGroupObject.set(SerializeKey::Name, WrapSourcemap(resourceGroup.attributes.name));

    // Description & Resources
    SourceMap<Description> description;
    sos::Array resources;

    for (Collection<SourceMap<Element> >::const_iterator it = resourceGroup.content.elements().collection.begin();
         it != resourceGroup.content.elements().collection.end();
         ++it) {

        if (it->element == Element::ResourceElement) {
            resources.push(WrapResourceSourcemap(it->content.resource));
        }
        else if (it->element == Element::CopyElement) {
            description.sourceMap.append(it->content.copy.sourceMap);
        }
    }

    resourceGroupObject.set(SerializeKey::Description, WrapSourcemap(description));
    resourceGroupObject.set(SerializeKey::Resources, resources);

    return resourceGroupObject;
}


sos::Object WrapDataStructureContent(const SourceMap<DataStructure>& dataStructure)
{
    sos::Object dataStructureObject;

    // Source
    dataStructureObject.set(SerializeKey::Source, WrapNamedTypeSourcemap(dataStructure));

    return dataStructureObject;
}

sos::Object WrapElementSourcemap(const SourceMap<Element>& element)
{
    sos::Object elementObject;

    if (!element.attributes.name.sourceMap.empty()) {

        sos::Object attributes;

        attributes.set(SerializeKey::Name, WrapSourcemap(element.attributes.name));
        elementObject.set(SerializeKey::Attributes, attributes);
    }

    switch (element.element) {
        case Element::CopyElement:
        {
            elementObject.set(SerializeKey::Content, WrapSourcemap(element.content.copy));
            break;
        }

        case Element::DataStructureElement:
        {
            return WrapDataStructureSourcemap(element.content.dataStructure);
        }

        case Element::ResourceElement:
        {
            return WrapResourceSourcemap(element.content.resource);
        }

        case Element::CategoryElement:
        {
            sos::Array content;

            for (Collection<SourceMap<Element> >::const_iterator it = element.content.elements().collection.begin();
                 it != element.content.elements().collection.end();
                 ++it) {

                content.push(WrapElementSourcemap(*it));
            }

            elementObject.set(SerializeKey::Content, content);
            /*
            elementObject.set(SerializeKey::Content, 
                              WrapSourceMapCollection<Element>()(element.content.elements().collection, WrapElementSourcemap));
            */
            break;
        }

        default:
            break;
    }

    return elementObject;
}

sos::Object drafter::WrapBlueprintSourcemap(const SourceMap<Blueprint>& blueprint)
{
    sos::Object blueprintObject;

    // Metadata
    blueprintObject.set(SerializeKey::Metadata, 
                        WrapSourceMapCollection<Metadata>()(blueprint.metadata.collection, WrapSourcemap));

    // Name
    blueprintObject.set(SerializeKey::Name, WrapSourcemap(blueprint.name));

    // Description
    blueprintObject.set(SerializeKey::Description, WrapSourcemap(blueprint.description));

    // Resource Groups
    sos::Array resourceGroups;

    for (Collection<SourceMap<Element> >::const_iterator it = blueprint.content.elements().collection.begin();
         it != blueprint.content.elements().collection.end();
         ++it) {

        if (it->element == Element::CategoryElement &&
            it->category == Element::ResourceGroupCategory) {

            resourceGroups.push(WrapResourceGroupSourcemap(*it));
        }
    }

    blueprintObject.set(SerializeKey::ResourceGroups, resourceGroups);

    // Content
    sos::Array content;

    for (Collection<SourceMap<Element> >::const_iterator it = blueprint.content.elements().collection.begin();
         it != blueprint.content.elements().collection.end();
         ++it) {

        content.push(WrapElementSourcemap(*it));
    }

    blueprintObject.set(SerializeKey::Content, content);
    /*
    blueprintObject.set(SerializeKey::Content, 
                        WrapSourceMapCollection<Element>()(blueprint.content.elements().collection, WrapElementSourcemap));
    */
    return blueprintObject;
}
