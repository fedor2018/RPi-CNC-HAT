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
#include "configurationconfig.h"

ConfigurationConfig::ConfigurationConfig(void)
{
    m_nDescriptorsLength = 0;
}

ConfigurationConfig::~ConfigurationConfig(void)
{
}


// A timer config string is Mode,Prescaler,Top
// where Mode is 0 - phase correct, 1 - fast pwm
static bool UnpackTimer( const QDomElement &elem, const QString &sAttrName, struct TimerConfig &timer, bool b16Bit, QString *sError )
{
    QString s;
    if ( !XMLUtility::getAttributeString( elem, sAttrName, s, sError ) )
        return false;
    
    QStringList parts = s.split(",");
    if ( parts.count() != 3 )
    {
        if ( sError != NULL )
            *sError = QString("Invalid timer format, '%1', for attribute '%2' on node <%3>.  Must be Mode,Prescaler,Top").arg(s).arg(sAttrName).arg(elem.parentNode().nodeName());
        return false;
    }
    
    bool bOk;
    s = parts[0].trimmed();
    int n = s.toInt(&bOk);
    if ( !bOk || n < 0 || n > 1 )
    {
        if ( sError != NULL )
            *sError = QString("Invalid mode, '%1', for attribute '%2' on node <%3>.  Must be 0 or 1").arg(s).arg(sAttrName).arg(elem.parentNode().nodeName());
        return false;
    }
    byte nMode = (byte)n;
    
    int nMax;
    QStringList sPrescales;
    if ( b16Bit )
    {
        nMax = 0xFFFF;
        sPrescales << "1" << "8" << "64" << "256" << "1024";
    }
    else
    {
        nMax = 0xFF;
        sPrescales << "1" << "8" << "32" << "64" << "128" << "256" << "1024";
    }
    
    s = parts[1].trimmed();
    if ( !sPrescales.contains(s) )
    {
        if ( sError != NULL )
            *sError = QString("Invalid prescale, '%1', for attribute '%2' on node <%3>.  Must be 0 or 1").arg(s).arg(sAttrName).arg(elem.parentNode().nodeName());
        return false;
    }
    uint16_t nPrescale = (uint16_t)s.toInt();
    
    s = parts[2].trimmed();
    n = s.toInt(&bOk);
    if ( !bOk || n < 1 || n > nMax )
    {
        if ( sError != NULL )
            *sError = QString("Invalid Count Top, '%1', for attribute '%2' on node <%3>.  Must be from 1 to %4").arg(s).arg(sAttrName).arg(elem.parentNode().nodeName()).arg(nMax);
        return false;
    }
    uint16_t nTop = (uint16_t)n;
    
    timer.Mode = nMode;
    timer.Prescaler = nPrescale;
    timer.CounterTop = nTop;
    
    return true;
}


bool ConfigurationConfig::Load( const QDomElement &elem, QString *sError )
{
    m_nDescriptorsLength = 0;
    
    if ( !XMLUtility::getAttributeBool( elem, "BusPowered", m_bBusPowered, sError ) )
        return false;
    if ( !XMLUtility::getAttributeUShort( elem, "PowerConsumption", m_nPowerConsumption, 0, 500, sError ) )
        return false;
    if ( !XMLUtility::getAttributeByte( elem, "Interval", m_nInterval, 1, 255, sError ) )
        return false;
    if ( !XMLUtility::getAttributeBool( elem, "is5Volts", m_b5Volts, sError ) )
        return false;
    QString s;
    if ( !XMLUtility::getAttributeString( elem, "UseStatusLEDs", s, sError ) )
        return false;
    if ( s.compare("LED1",Qt::CaseInsensitive) == 0 )
        m_eUseStatusLEDs = LED1;
    else if ( s.compare("LED2",Qt::CaseInsensitive) == 0 )
        m_eUseStatusLEDs = LED2;
    else if ( s.compare("Both",Qt::CaseInsensitive) == 0 )
        m_eUseStatusLEDs = Both;
    else if ( s.compare("None",Qt::CaseInsensitive) == 0 )
        m_eUseStatusLEDs = None;
    else
    {
        if ( sError != NULL )
            *sError = QString("Unknown value found for attribute '%1' in node <%2>.  Found '%3' but expected one of LED1,LED2,Both, or None.").arg("UseStatusLEDs",elem.nodeName(),s);
        return false;
    }
    if ( !XMLUtility::getAttributeUInt( elem, "SerialDebug", m_nSerialDebug, 0, 100, sError ) )
        return false;
    if ( !XMLUtility::getAttributeBool( elem, "HIDDebug", m_bHIDDebug, sError ) )
        return false;
    if ( !XMLUtility::getAttributeBool( elem, "EnableJTAG", m_bEnableJTAG, sError ) )
        return false;
    if ( !XMLUtility::getAttributeUShort( elem, "UsagePage", m_nUsagePage, 0, 0xFFFF, sError ) )
        return false;
    if ( !XMLUtility::getAttributeUShort( elem, "Usage", m_nUsage, 0, 0xFFFF, sError ) )
        return false;
    
    if ( !XMLUtility::getAttributeString( elem, "PowerPort", s, sError ) )
        return false;
    if  ( s.isEmpty() )
        m_nPowerPort = 0xFF;
    else if ( !MakePort( s, m_nPowerPort, sError ) )
        return false;
    
    assert( countof(m_Timers) == 3 );
    if ( !UnpackTimer( elem, "Timer1", m_Timers[0], true, sError ) )
        return false;
    if ( !UnpackTimer( elem, "Timer2", m_Timers[1], false, sError ) )
        return false;
    if ( !UnpackTimer( elem, "Timer3", m_Timers[2], true, sError ) )
        return false;
    
    return true;
}


ByteArray ConfigurationConfig::GetReportDescriptor(StringTable & /*table*/) const
{
    ByteBuffer ConfigDescriptor;
    
    ConfigDescriptor.AddByte( 0 );	    // Length of this descriptor
    ConfigDescriptor.AddByte(2);	    // CONFIGURATION Type
    ConfigDescriptor.AddShort(m_nDescriptorsLength);      // Overall length of all config descriptors
    ConfigDescriptor.AddByte(1);	    // Number of interfaces;
    ConfigDescriptor.AddByte(1);	    // Configuration number
    ConfigDescriptor.AddByte(0);	    // Configuration String index
    
    byte nAttributes = 0x80;
    if ( !m_bBusPowered )
        nAttributes |= 1 << 6;
    ConfigDescriptor.AddByte(nAttributes);      // Attributes
    ConfigDescriptor.AddByte((byte)(m_nPowerConsumption / 2));      // Power Consumption
    ConfigDescriptor[0] = (byte)ConfigDescriptor.count();
    
    assert(DESCRIPTOR_SIZE == ConfigDescriptor.count());
    
    return ConfigDescriptor;
}
