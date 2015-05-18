#include "RefractAST.h"
#include "refract/Element.h"

namespace drafter
{

    static refract::ArrayElement* MsAttributesToRefract(const mson::TypeAttributes& ta)
    {
        refract::ArrayElement* attr = new refract::ArrayElement;

        if (ta & mson::RequiredTypeAttribute) {
            attr->push_back(refract::IElement::Create("required"));
        }
        if (ta & mson::OptionalTypeAttribute) {
            attr->push_back(refract::IElement::Create("optional"));
        }
        if (ta & mson::DefaultTypeAttribute) {
            attr->push_back(refract::IElement::Create("default"));
        }
        if (ta & mson::SampleTypeAttribute) {
            attr->push_back(refract::IElement::Create("sample"));
        }
        if (ta & mson::FixedTypeAttribute) {
            attr->push_back(refract::IElement::Create("fixed"));
        }

        if (attr->value.empty()) {
            delete attr;
            attr = NULL;
        }

        return attr;
    }

    template <typename T>
    T LiteralTo(const mson::Literal& literal);

    template <>
    bool LiteralTo<bool>(const mson::Literal& literal)
    {
        return literal == "true";
    }

    template <>
    double LiteralTo<double>(const mson::Literal& literal)
    {
        return atof(literal.c_str());
    }

    template <>
    std::string LiteralTo<std::string>(const mson::Literal& literal)
    {
        return literal;
    }

    static refract::IElement* SimplifyRefractContainer(const std::vector<refract::IElement*>& container)
    {
        if (container.empty()) {
            return NULL;
        }

        if (container.size() == 1) {
            return container[0];
        }

        refract::ArrayElement* array = new refract::ArrayElement;
        for (std::vector<refract::IElement*>::const_iterator it = container.begin(); it != container.end(); ++it) {
            array->push_back(*it);
        }
        return array;
    }

    struct RefractElementFactory
    {
        virtual ~RefractElementFactory()
        {
        }
        virtual refract::IElement* Create(const std::string& literal) = 0;
    };

    template <typename E>
    struct RefractElementFactoryImpl : RefractElementFactory
    {
        virtual refract::IElement* Create(const std::string& literal)
        {
            E* element = new E;
            element->set(LiteralTo<typename E::ValueType>(literal));
            return element;
        }
    };

    RefractElementFactory& FactoryFromType(const mson::BaseTypeName typeName)
    {
        static RefractElementFactoryImpl<refract::BooleanElement> bef;
        static RefractElementFactoryImpl<refract::NumberElement> nef;
        static RefractElementFactoryImpl<refract::StringElement> sef;

        switch (typeName) {
            case mson::BooleanTypeName:
                return bef;
            case mson::NumberTypeName:
                return nef;
            case mson::StringTypeName:
                return sef;
            default:
                ; // do nothing
        }

        throw std::logic_error("Out of scope - this should never happen");
    }

    template <typename T, typename V = typename T::ValueType>
    struct ExtractValues
    { // This will handle primitive elements
        const mson::ValueDefinition& vd;

        ExtractValues(const mson::ValueDefinition& v) : vd(v)
        {
        }

        operator V()
        {
            // printf("EVp\n");
            if (vd.values.empty()) {
                throw std::logic_error("Cannot extract values from empty container");
            }
            if (vd.values.size() > 1) {
                throw std::logic_error("For primitive types is just one value supported");
            }
            return LiteralTo<V>(vd.values.begin()->literal);
        }
    };

    template <typename T>
    struct ExtractValues<T, std::vector<refract::IElement*> >
    { // this will handle Array && Object because of underlaying type
        const mson::ValueDefinition& vd;
        typedef std::vector<refract::IElement*> V;

        ExtractValues(const mson::ValueDefinition& v) : vd(v)
        {
        }

        operator V()
        {
            // printf("EVc\n");
            if (vd.values.empty()) {
                throw std::logic_error("Cannot extract values from empty container");
            }

            mson::BaseTypeName type = mson::StringTypeName;
            // Found if type of element is specified.
            // if more types is used - fallback to "StringType"
            if (vd.typeDefinition.typeSpecification.nestedTypes.size() == 1) {
                type = vd.typeDefinition.typeSpecification.nestedTypes.begin()->base;
            }

            RefractElementFactory& elementFactory = FactoryFromType(type);

            V result;
            for (mson::Values::const_iterator it = vd.values.begin(); it != vd.values.end(); ++it) {
                result.push_back(elementFactory.Create(it->literal));
            }

            return result;
        }
    };

    static refract::IElement* MsonElementToRefract(const mson::Element& mse);

    // FIXME: check against original behavioration
    template <typename T, typename V = typename T::ValueType>
    struct ExtractTypeSection
    { // This will handle primitive elements
        const mson::TypeSection& ts;

        ExtractTypeSection(const mson::TypeSection& v) : ts(v)
        {
        }

        operator V()
        {
            // printf("ETs\n");
            return LiteralTo<V>(ts.content.value);
        }
    };

    template <typename T>
    struct ExtractTypeSection<T, std::vector<refract::IElement*> >
    { // this will handle Array && Object because of underlaying type
        const mson::TypeSection& ts;
        typedef std::vector<refract::IElement*> V;

        ExtractTypeSection(const mson::TypeSection& v) : ts(v)
        {
        }

        operator V()
        {
            // printf("ETc\n");
            const mson::Elements& e = ts.content.elements();
            if (e.empty()) {
                throw std::logic_error("Cannot extract values from empty container");
            }

            V result;
            for (mson::Elements::const_iterator it = e.begin(); it != e.end(); ++it) {
                result.push_back(MsonElementToRefract(*it));
            }

            return result;
        }
    };

    template <typename T>
    refract::IElement* RefractElementFromValue(const mson::ValueMember&);

    template <typename T>
    refract::IElement* RefractElementFromProperty(const mson::PropertyMember& property)
    {

        refract::IElement* element = RefractElementFromValue<T>(property);
        element->meta["name"] = refract::IElement::Create(property.name.literal);

        return element;
    }

    static bool hasMembers(const mson::ValueMember& value)
    {
        for (mson::TypeSections::const_iterator it = value.sections.begin(); it != value.sections.end(); ++it) {
            if (it->klass == mson::TypeSection::MemberTypeClass) {
                return true;
            }
        }
        return false;
    }

    static bool hasChildren(const mson::ValueMember& value)
    {
        return (value.valueDefinition.values.size() > 1) || hasMembers(value);
    }

    static refract::IElement* ArrayToEnum(refract::IElement* array)
    {
        if (!array)
            return array;
        array->element("enum");
        refract::ObjectElement* obj = new refract::ObjectElement;
        obj->push_back(array);
        return obj;
    }

    static refract::IElement* MsonPropertyToRefract(const mson::PropertyMember& property)
    {
        refract::IElement* element = NULL;
        mson::BaseTypeName nameType = property.valueDefinition.typeDefinition.typeSpecification.name.base;
        // printf("P[%d]", nameType);
        switch (nameType) {
            case mson::BooleanTypeName:
                // printf("=B\n");
                element = RefractElementFromProperty<refract::BooleanElement>(property);
                break;
            case mson::NumberTypeName:
                // printf("=N\n");
                element = RefractElementFromProperty<refract::NumberElement>(property);
                break;
            case mson::StringTypeName:
                // printf("=S\n");
                element = RefractElementFromProperty<refract::StringElement>(property);
                break;
            case mson::EnumTypeName:
                // printf("=E\n");
                element = ArrayToEnum(RefractElementFromProperty<refract::ArrayElement>(property));
                break;
            case mson::ArrayTypeName:
                // printf("=A\n");
                element = RefractElementFromProperty<refract::ArrayElement>(property);
                break;
            case mson::ObjectTypeName:
                // printf("=O\n");
                element = RefractElementFromProperty<refract::ObjectElement>(property);
                break;
            case mson::UndefinedTypeName:
                // printf("=U");
                element = hasChildren(property) ? RefractElementFromProperty<refract::ObjectElement>(property)
                                                : RefractElementFromProperty<refract::StringElement>(property);
                break;

            default:
                throw std::runtime_error("Unhandled type of PropertyMember");
        }
        return element;
    }
    template <typename T>
    refract::IElement* RefractElementFromValue(const mson::ValueMember& value)
    {
        using namespace refract;
        typedef T ElementType;

        ElementType* element = new ElementType;

        if (!value.valueDefinition.values.empty()) {
            element->set(ExtractValues<T>(value.valueDefinition));
        }

        if (IElement* attrs = MsAttributesToRefract(value.valueDefinition.typeDefinition.attributes)) {
            element->attributes["typeAttributes"] = attrs;
        }

        if (!value.description.empty()) {
            element->meta["description"] = IElement::Create(value.description);
        }

        if (!value.sections.empty()) {
            typedef std::vector<refract::IElement*> Elements;

            // FIXME: for Array - extract type of element from
            // value.valueDefinition.typeDefinition.typeSpecification.nestedTypes[];
            // and inject into ExtractTypeSection
            // reason - value defined as list eg.
            // - value: 1,2,3,4,5 (array[number, string])
            // does not hold type resp. is defaultly set as mson::UndefinedTypeName
            //
            // fallback for now - present as refract::StringElement

            std::vector<refract::IElement*> defaults;
            std::vector<refract::IElement*> samples;

            for (mson::TypeSections::const_iterator it = value.sections.begin(); it != value.sections.end(); ++it) {
                ElementType* e = new ElementType;

                if (it->klass == mson::TypeSection::MemberTypeClass) {
                    delete e;
                    if (!element->empty()) {
                        throw std::logic_error("Element content was already set, you cannot fill it from 'memberType'");
                    }
                    element->set(ExtractTypeSection<T>(*it));
                    continue;
                }

                e->set(ExtractTypeSection<T>(*it));

                if (it->klass == mson::TypeSection::SampleClass) {
                    samples.push_back(e);
                } else if (it->klass == mson::TypeSection::DefaultClass) {
                    defaults.push_back(e);
                } else { // FIXME: description is not handled
                    // printf("S[%d]\n",it->klass);
                    throw std::logic_error("Unexpected section for property");
                }
            }

            if (IElement* e = SimplifyRefractContainer(samples)) {
                element->attributes["sample"] = e;
            }

            if (IElement* e = SimplifyRefractContainer(defaults)) {
                element->attributes["default"] = e;
            }
        }

        return element;
    }

    refract::IElement* MsonValueToRefract(const mson::ValueMember& value)
    {
        refract::IElement* element = NULL;
        mson::BaseTypeName typeName = value.valueDefinition.typeDefinition.typeSpecification.name.base;
        // printf("V[%d]",typeName);
        switch (typeName) {
            case mson::BooleanTypeName:
                // printf("=B\n");
                element = RefractElementFromValue<refract::BooleanElement>(value);
                break;
            case mson::NumberTypeName:
                // printf("=N\n");
                element = RefractElementFromValue<refract::NumberElement>(value);
                break;
            case mson::StringTypeName:
                // printf("=S\n");
                element = RefractElementFromValue<refract::StringElement>(value);
                break;
            case mson::UndefinedTypeName:
                // printf("=U\n");
                element = RefractElementFromValue<refract::StringElement>(value);
                break;
            // case mson::EnumTypeName :
            //    //printf("=E\n");
            // case mson::ArrayTypeName :
            //    //printf("=A\n");
            //    // FIXME: switch to ArrayElement
            //    element = RefractElementFromValue<refract::StringElement>(value);
            //    break;
            // case mson::ObjectTypeName :
            //    //printf("=O\n");
            //    // FIXME: switch to ObjectElement
            //    element = RefractElementFromValue<refract::StringElement>(value);
            //    break;

            default:
                throw std::runtime_error("Unhandled type of ValueMember");
        }
        return element;
    }

    static refract::IElement* MsonOneofToRefract(const mson::OneOf& oneOf)
    {
        refract::ArrayElement* select = new refract::ArrayElement;
        select->element("select");
        for (mson::Elements::const_iterator it = oneOf.begin(); it != oneOf.end(); ++it) {
            refract::ObjectElement* option = new refract::ObjectElement;
            option->element("option");
            option->push_back(MsonElementToRefract(*it));
            select->push_back(option);
        }
        return select;
    }

    static refract::IElement* MsonMixinToRefract(const mson::Mixin& mixin)
    {
        refract::ObjectElement* ref = new refract::ObjectElement;
        ref->element("ref");
        ref->renderCompactContent(true);

        refract::StringElement* path = new refract::StringElement;
        path->set("content");
        path->meta["name"] = refract::IElement::Create("path");
        ref->push_back(path);

        refract::StringElement* href = new refract::StringElement;
        href->set(mixin.typeSpecification.name.symbol.literal);
        href->meta["name"] = refract::IElement::Create("href");
        ref->push_back(href);

        return ref;
    }

    static refract::IElement* MsonElementToRefract(const mson::Element& mse)
    {
        using namespace refract;

        std::string klass;

        switch (mse.klass) {
            case mson::Element::PropertyClass: {
                // elementObject.set(SerializeKey::Content, WrapPropertyMember(element.content.property));
                return MsonPropertyToRefract(mse.content.property);
                break;
            }

            case mson::Element::ValueClass: {
                klass = "value";
                // elementObject.set(SerializeKey::Content, WrapValueMember(element.content.value));
                return MsonValueToRefract(mse.content.value);
                break;
            }

            case mson::Element::MixinClass: {
                klass = "mixin";
                // elementObject.set(SerializeKey::Content, WrapMixin(element.content.mixin));
                return MsonMixinToRefract(mse.content.mixin);
                break;
            }

            case mson::Element::OneOfClass: {
                klass = "oneOf";
                return MsonOneofToRefract(mse.content.oneOf());
                // elementObject.set(SerializeKey::Content,
                //                  WrapCollection<mson::Element>()(element.content.oneOf(), WrapMSONElement));
                break;
            }

            case mson::Element::GroupClass: {
                klass = "group";
                // elementObject.set(SerializeKey::Content,
                //                  WrapCollection<mson::Element>()(element.content.elements(), WrapMSONElement));
                break;
            }

            default:
                break;
        }

        throw std::runtime_error("NI unhandled element");
    }

    refract::IElement* ToRefract(const snowcrash::DataStructure& ds)
    {
        using namespace refract;

        ObjectElement* e = new ObjectElement;

        if (!ds.typeDefinition.typeSpecification.name.symbol.literal.empty()) {
            e->element(ds.typeDefinition.typeSpecification.name.symbol.literal);
        }

        e->meta["id"] = IElement::Create(ds.name.symbol.literal);
        e->meta["title"] = IElement::Create(ds.name.symbol.literal);

        for (mson::TypeSections::const_iterator it = ds.sections.begin(); it != ds.sections.end(); ++it) {

            if (it->klass == mson::TypeSection::BlockDescriptionClass) {
                e->meta["description"] = IElement::Create(it->content.description);
                continue;
            }

            for (mson::Elements::const_iterator eit = (*it).content.elements().begin();
                 eit != (*it).content.elements().end(); ++eit) {
                e->push_back(MsonElementToRefract(*eit));
            }
        }

        return e;
    }

    sos::Object DataStructureToRefract(const snowcrash::DataStructure& dataStructure)
    {
        using namespace refract;
        IElement* element = ToRefract(dataStructure);
        SerializeVisitor serializer;
        serializer.visit(*element);
        delete element;

        return serializer.get();
    }
}
