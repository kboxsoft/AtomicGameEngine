#pragma once

#include <Atomic/Scene/Serializable.h>

/// Define an attribute that points to a member of the object.
#define ATOMIC_MEMBER_ATTRIBUTE(name, typeName, variable, defaultValue, mode) ATOMIC_ACCESSOR_ATTRIBUTE_FREE(name, [](const ClassName* classPtr) -> typename AttributeTrait<typeName >::ReturnType { return classPtr->variable; }, [](ClassName* classPtr, typename AttributeTrait<typeName >::ParameterType value) { classPtr->variable = value; }, typeName, defaultValue, mode)
/// Define an attribute that points to a member of the object, and uses zero-based enum values, which are mapped to names through an array of C string pointers.
#define ATOMIC_MEMBER_ENUM_ATTRIBUTE(name, typeName, variable, enumNames, defaultValue, mode) ATOMIC_ENUM_ACCESSOR_ATTRIBUTE_FREE(name, [](const ClassName* classPtr) -> unsigned { return static_cast<unsigned>(classPtr->variable); }, [](ClassName* classPtr, unsigned value) { classPtr->variable = static_cast<typeName>(value); }, unsigned, enumNames, defaultValue, mode)
/// Define an attribute that points to a member of the object and uses some setter and getter of this object.
#define ATOMIC_MEMBER_ATTRIBUTE_ACCESSOR(name, typeName, variable, getFunction, setFunction, defaultValue, mode) ATOMIC_ACCESSOR_ATTRIBUTE_FREE(name, [](const ClassName* classPtr) -> typename AttributeTrait<typeName >::ReturnType { return classPtr->variable.getFunction(); }, [](ClassName* classPtr, typename AttributeTrait<typeName >::ParameterType value) { classPtr->variable.setFunction(value); }, typeName, defaultValue, mode)
