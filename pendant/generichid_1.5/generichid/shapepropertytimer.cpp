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
#include "shapepropertytimer.h"

ShapePropertyTimer::ShapePropertyTimer( QtStringPropertyManager *manager, const QString &sName, const QString &sDescription, bool bEnabled )
: ShapePropertyString( sName, sDescription, bEnabled )
, m_pManager( manager )
{
}

ShapePropertyTimer::~ShapePropertyTimer(void)
{
}

QtProperty *ShapePropertyTimer::getQtProperty()
{
    QtProperty *pProp = m_pManager->addProperty( m_sName );
    pProp->setToolTip( m_sDescription );
    pProp->setWhatsThis( m_sDescription );
    pProp->setEnabled( m_bEnabled );
    return pProp;
}


void ShapePropertyTimer::setValue(QtProperty *qtProp, PropertyValue *propVal) const
{
    PropertyValueString *val = dynamic_cast<PropertyValueString *>( propVal );
    if ( val != NULL )
	m_pManager->setValue( qtProp, val->Value );
}

void ShapePropertyTimer::getValue(QtProperty *qtProp, PropertyValue *propVal) const
{
    PropertyValueString *val = dynamic_cast<PropertyValueString *>( propVal );
    if ( val != NULL )
	val->Value = m_pManager->value( qtProp );
}

