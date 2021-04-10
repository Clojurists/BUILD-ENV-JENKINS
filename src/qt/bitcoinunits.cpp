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
 