// hidcomp/hidconfig, HID device interface for emc
// Copyright (C) 2008, Frank Tkalcevic, www.franksworkshop.com

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

#ifndef _HID_H_
#define _HID_H_

#ifdef _WIN32
#pragma warning(push, 1)
#endif
#include <QDomElement>
#include <QList>
#include <QVector>
#ifdef _WIN32
#pragma warning(pop)
#pragma warning(disable:4251)
#endif

#include <vector>

#include "log.h"
#include "cpoints.h"
#include "lcddisplaydata.h"
#include "datatypes.h"

namespace HIDItemType
{
    enum HIDItemType
    {
	Button		= 0,
	Value		= 1,
	Hatswitch	= 2,
	LED		= 3,
	LCD		= 4,
	OutputValue	= 5,
        KeyboardMap     = 6
    };
}


class HIDDeviceCriteria
{
public:
    HIDDeviceCriteria(void);
    ~HIDDeviceCriteria(void);

    bool bPID;
    unsigned short nPID;
    bool bVID;
    unsigned short nVID;
    bool bManufacturer;
    QString sManufacturer;
    bool bProduct;
    QString sProduct;
    bool bSerialNumber;
    QString sSerialNumber;
    bool bSystemId;
    QString sSystemId;
    bool bInterfaceNumber;
    unsigned char nInterfaceNumber;

    void ReadXML( QDomElement pNode );
    QDomElement WriteXML( QDomElement pNode );
};

class HIDItem
{
public:
    HIDItem( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName, HIDItemType::HIDItemType type );
    HIDItem( const HIDItem &other );
    virtual ~HIDItem();

    int nIndex;
    unsigned short nUsagePage;
    unsigned short nUsage;
    bool bEnabled;
    QString sPinName;
    HIDItemType::HIDItemType type;

    virtual void ReadXML( QDomElement /*pNode*/ ) {}
    virtual QDomElement WriteXML( QDomElement pNode );
    static HIDItem *CreateFromXML( QDomElement pNode );
    static HIDItem *CreateItem( HIDItemType::HIDItemType type, int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
};

class HIDInputItem: public HIDItem
{
public:
    HIDInputItem( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName, HIDItemType::HIDItemType type );
    virtual ~HIDInputItem();
};

class HIDOutputItem: public HIDItem
{
public:
    HIDOutputItem( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName, HIDItemType::HIDItemType type );
    HIDOutputItem( const HIDOutputItem &other );
    virtual ~HIDOutputItem();
};

class HIDButton: public HIDInputItem
{
public:
    HIDButton( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    virtual ~HIDButton();
};


namespace ValueType
{
    enum ValueType
    {
	Default
    };
};

namespace ValueOutputType
{
    enum ValueOutputType
    {
	FloatingPoint,
	RawInteger
    };
};

class HIDInputValue: public HIDInputItem
{
public:
    HIDInputValue( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    virtual ~HIDInputValue();
    virtual void ReadXML( QDomElement pNode );
    virtual QDomElement WriteXML( QDomElement pNode );
    int Interpolate( int lP );
    int Scale( int lP );
    double DScale( int lP );

    ValueType::ValueType valueType;
    ValueOutputType::ValueOutputType outputType;
    int nLogicalMinOverride;
    int nLogicalMaxOverride;
    double dOutputMin;
    double dOutputMax;
    bool bUseResponseCurve;
    bool bReverse;
    std::vector<CPoint> responseCurve;
#define POINTS_MAX_VAL		10000		// 100% 
};

class HIDHatswitch: public HIDInputItem
{
public:
    HIDHatswitch( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    virtual ~HIDHatswitch();
};

class HIDLED: public HIDOutputItem
{
public:
    HIDLED( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    virtual ~HIDLED();
};

class HIDOutputValue: public HIDOutputItem
{
public:
    HIDOutputValue( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    virtual ~HIDOutputValue();
};

class LCDEntry
{
public:
    LCDEntry( ELCDDisplayData::ELCDDisplayData eData, int nIndex, int nRow, int nCol, const QString &sFormat, double dScale, const QString &sTestData );
    LCDEntry( const LCDEntry &that );
    ~LCDEntry();
    QDomElement WriteXML( QDomElement pNode );
    void ReadXML( QDomElement pNode );

    ELCDDisplayData::ELCDDisplayData data() { return m_eData; }
    void setData( ELCDDisplayData::ELCDDisplayData e) { m_eData = e; }
    int index() { return m_nIndex; }
    void setIndex(int n) { m_nIndex = n; }
    int row() { return m_nRow; }
    void setRow(int n) { m_nRow = n; }
    int col() { return m_nCol; }
    void setCol(int n) { m_nCol = n; }
    QString format() { return m_sFormat; }
    void setFormat(QString s) { m_sFormat = s; }
    double scale() { return m_dScale; }
    void setScale(double d) { m_dScale = d; }
    QString testData() { return m_sTestData; }
    void setTestData(QString s) { m_sTestData = s; }

private:
    ELCDDisplayData::ELCDDisplayData m_eData;   // The data that is to be displayed
    int m_nIndex;           // The index of the data item, eg Position[Axis0]
    int m_nRow;
    int m_nCol;
    QString m_sFormat;      // Format String
    double m_dScale;
    QString m_sTestData;
};

class LCDPage
{   
public:
    LCDPage( int nNumber, const QString &sName );
    LCDPage( const LCDPage &that );
    ~LCDPage();
    QDomElement WriteXML( QDomElement pNode );
    void ReadXML( QDomElement pNode );

    int number() { return m_nNumber; }
    void setNumber( int n ) { m_nNumber = n; }
    QString name() { return m_sName; }
    void setName( const QString &s ) { m_sName = s; }
    QList<LCDEntry*> &data() { return m_data; }

private:
    int m_nNumber;
    QString m_sName;
    QList<LCDEntry *> m_data;
};

class LCDFont
{   
public:
    LCDFont( byte nIndex, const QVector<byte> &data );
    LCDFont( const LCDFont &that );
    LCDFont() : m_nIndex(0) {}
    ~LCDFont();
    QDomElement WriteXML( QDomElement pNode );
    void ReadXML( QDomElement pNode );

    byte index() { return m_nIndex; }
    void setIndex( byte n ) { m_nIndex = n; }
    void setData( const QVector<byte> &data );
    const QVector<byte> &data() const { return m_data; }
    static bool GetFontBit( int cols, int r, int c, const byte *data );
    static void SetFontBit( int cols, int r, int c, byte *data, bool bSet );

private:
    byte m_nIndex;
    QVector<byte> m_data;
};

class HIDLCD: public HIDOutputItem
{
public:
    HIDLCD( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    HIDLCD( const HIDLCD &other );
    HIDLCD & operator= ( const HIDLCD & other );
    virtual ~HIDLCD();

    virtual void ReadXML( QDomElement pNode );
    virtual QDomElement WriteXML( QDomElement pNode );

    int samplePeriod() { return m_nSamplePeriod; }
    void setSamplePeriod( int n ) { m_nSamplePeriod = n; }
    int LCDProcPort() { return m_nLCDProcPort; }
    void setLCDProcPort( int n ) { m_nLCDProcPort = n; }
    QList<LCDPage *> &pages() { return m_pages; }
    QList<LCDFont *> &fonts() { return m_fonts; }

private:
    int m_nSamplePeriod;        // ms
    int m_nLCDProcPort;
    QList<LCDPage *> m_pages;
    QList<LCDFont *> m_fonts;
    void clear();
};

class KeyMap
{
public:
    KeyMap( QList<unsigned short> &keys, QList<unsigned short> &modifiers, QString sPinName);
    KeyMap( const KeyMap &other );
    KeyMap();
    ~KeyMap();
    const QList<unsigned short> Keys() const { return m_keys; }
    const QList<unsigned short> Modifiers() const { return m_modifiers; }
    const QString PinName() const { return m_sPinName; }
    QString KeyStrokeName() const;
    void ReadXML( QDomElement pNode );
    QDomElement WriteXML( QDomElement pNode );

private:
    QList<unsigned short> m_keys;
    QList<unsigned short> m_modifiers;
    QString m_sPinName;
};

class HIDKeyboardMap: public HIDInputItem
{
public:
    HIDKeyboardMap( int nIndex, unsigned short nUsagePage, unsigned short nUsage, bool bEnabled, const QString &sPinName );
    HIDKeyboardMap( const HIDKeyboardMap &other );
    HIDKeyboardMap & operator= ( const HIDKeyboardMap & other );
    virtual ~HIDKeyboardMap();

    virtual void ReadXML( QDomElement pNode );
    virtual QDomElement WriteXML( QDomElement pNode );
    QList<KeyMap *> & KeyMappings() { return m_keymappings; }
    void clear();

private:
    QList<KeyMap *> m_keymappings;
};


class HID
{
public:
    HID();
    ~HID();

    HIDDeviceCriteria *criteria;
    std::vector< HIDItem *> items;
    QString configFile;

    static HID *CreateFromXML( const QString &file );
    QDomDocument MakeXML();
};

inline QString FormatPID( unsigned short n ) { return QString("%1").arg(n,4,16,QChar('0')); }
inline QString FormatVID( unsigned short n ) { return QString("%1").arg(n,4,16,QChar('0')); }

#endif
