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
      case mBTC: 
           return 3;
      case uBTC: 
           return 0;
      default: 
           return 0;
    }
}


QString BitcoinUnits::format(int unit, qint64 n, bool fPlus)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    if (!valid(unit))
    {
        // Refuse to format invalid unit
        return QString(); 
    }

    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');

    // Right-trim excess zeros after the decimal point
    int nTrim = 0;
    for (int i = remainder_str.size() - 1; i >= 2 && (remainder_str.at(i) == '0'); --i)
    {
        ++nTrim;
    }
    remainder_str.chop(nTrim);
    if (n < 0)
    {
        quotient_str.insert(0, '-');
    }
    else if (fPlus && n > 0)
    {
        quotient_str.insert(0, '+');
    }
    return quotient_str + QString(".