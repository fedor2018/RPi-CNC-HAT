// generichid, DIY HID device 
// Copyright (C) 2009, Frank Tkalcevic, www.franksworkshop.com

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "listpropertymanager.h"


ListPropertyManager::ListPropertyManager(QObject *parent)
: QtAbstractPropertyManager(parent)
{
}

ListPropertyManager::~ListPropertyManager()
{
    clear();
}


int ListPropertyManager::value(const QtProperty *property) const
{
    PropertyValueMap::const_iterator it = m_values.constFind( property );
    if ( it == m_values.constEnd() )
	return 0;
    else
	return it->val;
}

ListEnumList ListPropertyManager::enums(const QtProperty *property) const
{
    PropertyValueMap::const_iterator it = m_values.constFind( property );
    if ( it == m_values.constEnd() )
	return ListEnumList();
    else
	return it->enums;
}

QString ListPropertyManager::valueText(const QtProperty *property) const
{
    PropertyValueMap::const_iterator it = m_values.constFind( property );
    if ( it == m_values.constEnd() )
	return QString();

    // if the index is in the enum list, return it, otherwise, stringise the number
    ListEnumsMap::const_iterator itEnum = it->enumsMap.constFind( it->val );
    if ( itEnum != it->enumsMap.constEnd() )
	return itEnum->s;
    else
	return QString::number(it->val);
}

void ListPropertyManager::setValue(QtProperty *property, int val)
{
    const PropertyValueMap::iterator it = m_values.find(property);
    if (it == m_values.end())
        return;

    Data &data = it.value();
    if ( data.val == val )
	return;

    data.val = val;

    emit propertyChanged(property);
    emit valueChanged(property, data.val);
}

void ListPropertyManager::setEnums(QtProperty *property, const ListEnumList &enumNames)
{
    const PropertyValueMap::iterator it = m_values.find(property);
    if (it == m_values.end())
        return;

    Data &data = it.value();
    data.enums.clear();
    data.enumsMap.clear();

    data.enums = enumNames;
    foreach ( const EnumItem &item, data.enums )
	data.enumsMap.insert( item.i, item );

    emit enumsChanged(property);
    emit propertyChanged(property);
    emit valueChanged(property, data.val);
}

void ListPropertyManager::initializeProperty(QtProperty *property)
{
    m_values[property] = Data();
}

void ListPropertyManager::uninitializeProperty(QtProperty *property)
{
    m_values.remove(property);
}

