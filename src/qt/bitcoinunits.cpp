#include "bitcoinunits.h"

#include <QStringList>

BitcoinUnits::BitcoinUnits(QObject *parent):
        QAbstractListModel(parent),
        unitlist(availableUnits())
{
}


QList<BitcoinUnits::Unit> BitcoinUnits::availableUnits()
{
    QList<BitcoinUnits::Unit> unitlist;
    unitlist.append(BTC);
    unitlist.append(mBTC);
    unitlist.append(uBTC);
    return unitlist;
}


bool BitcoinUnits::valid(int unit)
{
    switch (unit)
    {
      case BTC:
      case mBTC:
      case uBTC:
           return true;
      default:
           return false;
    }
}


QString BitcoinUnits::name(int unit)
{
    switch (unit)
    {
      case BTC: 
           return QString("JPC");
      case mBTC: 
           return QString("mJPC");
      case uBTC: 
           return QString::fromUtf8("μJPC");
      default: 
           return QString("???");
    }
}


QString BitcoinUnits::description(int unit)
{
    switch (unit)
    {
      case BTC: 
           return QString("JackpotCoins");
      case mBTC: 
           return QString("Milli-JackpotCoins (1 / 1,000)");
      case uBTC: 
           return QString("Micro-JackpotCoins (1 / 1,000,000)");
      default: 
           return QString("???");
    }
}


qint64 BitcoinUnits::factor(int unit)
{
    switch (unit)
    {
      case BTC:  
           return 1000000;
      case mBTC: 
           return 1000;
      case uBTC: 
           return 1;
      default:   
           return 1000000;
    }
}


int BitcoinUnits::amountDigits(int unit)
{
    switch (unit)
    {
      case BTC: 
           return 8;  // 21,000,000 (# digits, without commas)
      case mBTC: 
           return 11; // 21,000,000,000
      case uBTC: 
           return 14; // 21,000,000,000,000
      default: 
           return 0;
    }
}


int BitcoinUnits::decimals(int unit)
{
    switch(unit)
    {
      case BTC: 
           return 6;