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
#include "shapekeymatrix.h"

const int MAX_ROWS = 8;
const int MAX_COLUMNS = 8;

ShapeKeyMatrix::ShapeKeyMatrix(QDomElement &node, const QString &sShapeName, ShapeType::ShapeType eShapeType, const QString &sShapeId, bool bSource, const QString &sImageFile, const QString &sIconFile, int nMaxInstances, const QString &sDescription)
    : Shape(node, sShapeName, eShapeType, sShapeId, bSource, sImageFile, sIconFile, nMaxInstances, sDescription)
{
}

ShapeKeyMatrix::~ShapeKeyMatrix(void)
{
}

bool ShapeKeyMatrix::Verify( QString &sErrors, const QList<class PinItem *> &pins, const QList<class PropertyValue *> &values ) const
{
    bool bSuccess = true;

    QSet<QString> pinsToCheck, notPinsToCheck;

    int nRows = GetPropertyValueInt( "Rows", values, 0 );
    int nColumns = GetPropertyValueInt( "Columns", values, 0 );

    for ( int i = 0; i < MAX_ROWS; i++ )
    {
        QString s = QString("R%1").arg(i);
        if ( i < nRows )
            pinsToCheck << s;
        else
            notPinsToCheck << s;
    }

    for ( int i = 0; i < MAX_COLUMNS; i++ )
    {
        QString s = QString("C%1").arg(i);
        if ( i < nColumns )
            pinsToCheck << s;
        else
            notPinsToCheck << s;
    }


    bSuccess = CheckPins( sErrors, pins, pinsToCheck );
    if ( !CheckNotPins( sErrors, pins, notPinsToCheck ) )
        bSuccess = false;

    return bSuccess;
}


void ShapeKeyMatrix::MakeControlsXML( QDomElement &elem, const QList<class PinItem *> &pins, const QList<PropertyValue *> &values  ) const
{
    int nRows = GetPropertyValueInt("Rows",values,0);
    int nCols = GetPropertyValueInt("Columns",values,0);

    QStringList rows, cols;
    for ( int i = 0; i < nRows; i++ )
        rows << GetPort(pins, QString("R%1").arg(i) );

    for ( int i = 0; i < nCols; i++ )
        cols << GetPort(pins, QString("C%1").arg(i) );

    MakeKeyMatrixControl( elem, 
                          GetPropertyValueString("Key Names",values,""),
                          GetPropertyValueUsagePage("Usage",values,1),
                          GetPropertyValueUsage("Usage",values,1),
                          GetPropertyValueInt("DebounceMs",values,10),
                          rows,
                          cols );
}


void ShapeKeyMatrix::PropertyChanged( QtBrowserItem *item, QList<PropertyValue *> & ) const
{
    // if rows or columns change, update the key names editor
    QtProperty *prop = item->property();
    assert( prop != NULL );
    if ( prop != NULL && prop->propertyName().compare( "Rows", Qt::CaseInsensitive ) == 0 )
    {
        int nRows = ShapeProperty::m_intManager->value( prop );
        ShapeProperty::m_keyMatrixNameFactory->setRows( nRows );
    }
    else if ( prop != NULL && prop->propertyName().compare( "Columns", Qt::CaseInsensitive ) == 0 )
    {
        int nColumns = ShapeProperty::m_intManager->value( prop );
        ShapeProperty::m_keyMatrixNameFactory->setColumns( nColumns );
    }
}

void ShapeKeyMatrix::populateProperties( QList<PropertyValue *> &values ) const
{
    Shape::populateProperties( values );
    // need to read the row/column values and set the row/column properties of the key names editor
    int nRows = GetPropertyValueInt( "Rows", values, 0 );
    int nColumns = GetPropertyValueInt( "Columns", values, 0 );

    ShapeProperty::m_keyMatrixNameFactory->setRows( nRows );
    ShapeProperty::m_keyMatrixNameFactory->setColumns( nColumns );
}

