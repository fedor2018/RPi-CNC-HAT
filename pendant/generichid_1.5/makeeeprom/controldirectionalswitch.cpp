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
#include "controldirectionalswitch.h"

ControlDirectionalSwitch::ControlDirectionalSwitch(void)
{
    m_nPortNW=0xFF;
    m_nPortNE=0xFF;
    m_nPortSW=0xFF;
    m_nPortSW=0xFF;
    m_nPortN=0xFF;
    m_nPortS=0xFF;
    m_nPortW=0xFF;
    m_nPortE=0xFF;
}

ControlDirectionalSwitch::~ControlDirectionalSwitch(void)
{
}

bool ControlDirectionalSwitch::Load( const QDomElement &elem, QString *sError )
{
    if ( !XMLUtility::getAttributeString( elem, "Name", m_sName, sError ) )
        return false;
    if ( !XMLUtility::getAttributeUShort( elem, "UsagePage", m_nUsagePage, 0, 0xFFFF, sError ) )
        return false;
    if ( !XMLUtility::getAttributeUShort( elem, "Usage", m_nUsage, 0, 0xFFFF, sError ) )
        return false;
    if ( !XMLUtility::getAttributeBool( elem, "Pullup", m_bPullup, sError ) )
        return false;
    if ( !XMLUtility::getAttributeByte( elem, "DebounceMs", m_nDebounceMs, DEBOUNCE_MIN, DEBOUNCE_MAX, sError ) )
        return false;

    QString sDir;
    if ( !XMLUtility::getAttributeString( elem, "Directions", sDir, sError ) )
        return false;
    if ( sDir.compare( "2way NS", Qt::CaseInsensitive ) == 0 )
        m_eType = NS;
    else if ( sDir.compare( "2way WE", Qt::CaseInsensitive ) == 0 )
        m_eType = WE;
    else if ( sDir.compare( "4way", Qt::CaseInsensitive ) == 0 )
        m_eType = _4way;
    else if ( sDir.compare( "8way", Qt::CaseInsensitive ) == 0 )
        m_eType = _8way;
    else
    {
        if ( sError != NULL )
            *sError = QString( "Unknown 'Directions' attribute in node <DirectionalSwitch>.  Got '%1'.  Must be one of NS, WE, 4way, or 8way." ).arg(sDir);
        return false;
    }

    switch ( m_eType )
    {
        case NS:
            if ( !GetPort( elem, "PortN", m_nPortN, sError ) )
                return false;
            if ( !GetPort( elem, "PortS", m_nPortS, sError ) )
                return false;
            break;
        case WE:
            if ( !GetPort( elem, "PortW", m_nPortW, sError ) )
                return false;
            if ( !GetPort( elem, "PortE", m_nPortE, sError ) )
                return false;
            break;
        case _8way:
            if ( !GetPort( elem, "PortNW", m_nPortNW, sError ) )
                return false;
            if ( !GetPort( elem, "PortNE", m_nPortNE, sError ) )
                return false;
            if ( !GetPort( elem, "PortSW", m_nPortSW, sError ) )
                return false;
            if ( !GetPort( elem, "PortSE", m_nPortSE, sError ) )
                return false;
        case _4way:
            if ( !GetPort( elem, "PortN", m_nPortN, sError ) )
                return false;
            if ( !GetPort( elem, "PortS", m_nPortS, sError ) )
                return false;
            if ( !GetPort( elem, "PortW", m_nPortW, sError ) )
                return false;
            if ( !GetPort( elem, "PortE", m_nPortE, sError ) )
                return false;
            break;
    }

    return true;
}


ByteArray ControlDirectionalSwitch::GetHIDReportDescriptor( StringTable &table, int &nBits ) const
{
    HIDReportDescriptorBuilder Desc;

    Desc.UsagePage(m_nUsagePage);
    Desc.Usage(m_nUsage);

    byte nBitLen = 0;
    switch ( m_eType )
    {
        case NS:
            Desc.LogicalMinimum(1);
            Desc.LogicalMaximum(2);
            Desc.PhysicalMinimum(90);
            Desc.PhysicalMaximum(270);
            nBitLen = 2;
            break;
        case WE:
            Desc.LogicalMinimum(1);
            Desc.LogicalMaximum(2);
            Desc.PhysicalMinimum(0);
            Desc.PhysicalMaximum(180);
            nBitLen = 2;
            break;
        case _8way: // no idea what an 8 way switch is
        case _4way:
            Desc.LogicalMinimum(1);
            Desc.LogicalMaximum(8);
            Desc.PhysicalMinimum(0);
            Desc.PhysicalMaximum(315);
            nBitLen = 4;
            break;
    }

    Desc.ReportSize(nBitLen);
    Desc.ReportCount(1);
    nBits += nBitLen;
    if ( !m_sName.isEmpty() )
        Desc.StringIndex(table[m_sName]);
    Desc.Input(EDataType::Data, EVarType::Variable, ERelType::Absolute, EWrapType::NoWrap, ELinearType::Linear, EPreferedType::NoPreferred, ENullPositionType::NullState, EBufferType::BitField);

    return Desc;
}

// returns the micro controller application data
ByteArray ControlDirectionalSwitch::GetControlConfig( byte nReportId ) const
{
    int nPorts = 0;
    switch ( m_eType )
    {
        case NS:    nPorts = 2; break;
        case WE:    nPorts = 2; break;
        case _8way: nPorts = 8; break;
        case _4way: nPorts = 4; break;
    }

    int nBufSize = sizeof(struct SDirSwitchControl) + sizeof(struct SDirSwitchControl::_SDirSwitchData) * nPorts;
    byte *pBuf = new byte [ nBufSize ];
    memset( pBuf, 0, nBufSize );
    struct SDirSwitchControl &config = *reinterpret_cast<struct SDirSwitchControl *>( pBuf );

    config.hdr.Type = DirectionalSwitch;
    config.hdr.ReportId = nReportId;
    config.hdr.Length = (byte)nBufSize;
    config.DebounceMs = m_nDebounceMs;
    config.Options = (m_bPullup ? (1<<DSW_PULLUP) : 0 );

    switch ( m_eType )
    {
	case NS:
	    config.Type = DS_NS;
	    config.Ports = (byte)nPorts;
	    config.Switches[0].Port = m_nPortN;
	    config.Switches[1].Port = m_nPortS;
	    break;
	case WE:
	    config.Type = DS_WE;
	    config.Ports = (byte)nPorts;
	    config.Switches[0].Port = m_nPortW;
	    config.Switches[1].Port = m_nPortE;
	    break;
	case _4way:
	    config.Type = DS_4way;
	    config.Ports = (byte)nPorts;
	    config.Switches[DIRSW_PORTN].Port = m_nPortN;
	    config.Switches[DIRSW_PORTS].Port = m_nPortS;
	    config.Switches[DIRSW_PORTE].Port = m_nPortE;
	    config.Switches[DIRSW_PORTW].Port = m_nPortW;
	    break;
	case _8way:
	    config.Type = DS_8way;
	    config.Ports = (byte)nPorts;
	    config.Switches[DIRSW_PORTN].Port = m_nPortN;
	    config.Switches[DIRSW_PORTS].Port = m_nPortS;
	    config.Switches[DIRSW_PORTE].Port = m_nPortE;
	    config.Switches[DIRSW_PORTW].Port = m_nPortW;
	    config.Switches[DIRSW_PORTNE].Port = m_nPortNE;
	    config.Switches[DIRSW_PORTNW].Port = m_nPortNW;
	    config.Switches[DIRSW_PORTSE].Port = m_nPortSE;
	    config.Switches[DIRSW_PORTSW].Port = m_nPortSW;
	    break;
    }

    return ByteBuffer((byte *)&config, nBufSize );
}
