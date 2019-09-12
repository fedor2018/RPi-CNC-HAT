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
#include "controlkeymatrix.h"

ControlKeyMatrix::ControlKeyMatrix(void)
{
}

ControlKeyMatrix::~ControlKeyMatrix(void)
{
}

bool ControlKeyMatrix::Load( const QDomElement &elem, QString *sError )
{
    if ( !XMLUtility::getAttributeUShort( elem, "UsagePage", m_nUsagePage, 0, 0xFFFF, sError ) )
        return false;
    if ( !XMLUtility::getAttributeUShort( elem, "UsageMin", m_nUsageMin, 0, 0xFFFF, sError ) )
        return false;
    if ( !XMLUtility::getAttributeByte( elem, "DebounceMs", m_nDebounceMs, DEBOUNCE_MIN, DEBOUNCE_MAX, sError ) )
        return false;

    QDomNodeList rows = XMLUtility::elementsByTagName( elem, "RowIn" );
    for ( int i = 0; i < rows.count(); i++ )
    {
        QDomElement row = rows.item(i).toElement();
        byte nPort;
        if ( !GetPort( row, "Port", nPort, sError ) )
            return false;
        m_Rows.push_back( nPort );
    }

    QDomNodeList cols = XMLUtility::elementsByTagName( elem, "ColOut" );
    for ( int i = 0; i < cols.count(); i++ )
    {
        QDomElement col = cols.item(i).toElement();
        byte nPort;
        if ( !GetPort( col, "Port", nPort, sError ) )
            return false;
        m_Cols.push_back( nPort );
    }

    QDomNodeList keys = XMLUtility::elementsByTagName( elem, "Key" );
    for ( int i = 0; i < keys.count(); i++ )
    {
        QDomElement key = keys.item(i).toElement();

        byte nRow, nCol;
        QString sName;
        if ( !XMLUtility::getAttributeByte( key, "Row", nRow, 0, (byte)(m_Rows.count()-1), sError ) )
            return false;
        if ( !XMLUtility::getAttributeByte( key, "Col", nCol, 0, (byte)(m_Cols.count()-1), sError ) )
            return false;
        if ( !XMLUtility::getAttributeString( key, "Name", sName, sError ) )
            return false;
        sName = sName.trimmed();
        if ( !sName.isEmpty() )
        {
            if (m_Strings.isEmpty())
                for ( int i = 0; i < m_Rows.count() * m_Cols.count(); i++ )
                    m_Strings.append(QString());

            m_Strings[nCol * m_Rows.count() + nRow] = sName;
        }
    }

    return true;
}


ByteArray ControlKeyMatrix::GetHIDReportDescriptor( StringTable &table, int &nBits ) const
{
    HIDReportDescriptorBuilder Desc;

    Desc.UsagePage(m_nUsagePage);
    Desc.UsageMin(m_nUsageMin);
    Desc.UsageMax((ushort)(m_nUsageMin + m_Rows.count() * m_Cols.count()));
    Desc.LogicalMinimum(0);
    Desc.LogicalMaximum(1);
    Desc.ReportCount((uint)(m_Rows.count() * m_Cols.count()));
    Desc.ReportSize(1);
    nBits += (int)(m_Rows.count() * m_Cols.count());
    if ( !m_Strings.isEmpty() )
    {
        // When we add strings to the key matrix, if we add one string for a key, we must add strings to all keys.
        uint nLastIndex = 0;
        for (int i = 0; i < m_Rows.count() * m_Cols.count(); i++)
        {
            nLastIndex = table.ForceAdd( m_Strings[i] );
            if (i == 0)
                Desc.StringMin(nLastIndex);
        }
        Desc.StringMax(nLastIndex);
    }

    Desc.Input(EDataType::Data, EVarType::Variable, ERelType::Absolute, EWrapType::NoWrap, ELinearType::Linear, EPreferedType::NoPreferred, ENullPositionType::NoNullPosition, EBufferType::BitField);

    return Desc;
}

// returns the micro controller application data
ByteArray ControlKeyMatrix::GetControlConfig( byte nReportId ) const
{
    struct SKeyMatrixControl config;
    memset( &config, 0, sizeof(config) );

    config.hdr.Type = KeyMatrix;
    config.hdr.ReportId = nReportId;
    config.hdr.Length = (byte)(sizeof(config) + m_Rows.count() + m_Cols.count() + sizeof(DebounceData) * m_Rows.count() * m_Cols.count());
    config.Rows = (byte)m_Rows.count();
    config.Cols = (byte)m_Cols.count();
    config.DebounceMs = m_nDebounceMs;

    ByteBuffer buf((byte *)&config, sizeof(config) );

    // add the ports
    foreach( int port, m_Rows )
        buf.AddByte( (byte)port );
    foreach( int port, m_Cols )
        buf.AddByte( (byte)port );

    // debounce buffers for each key
    for (int i = 0; i < m_Rows.count() * m_Cols.count(); i++)
        buf.AddUShort(0);
    assert( sizeof(DebounceData) == sizeof(unsigned short) );

    return buf;
}
