#ifndef BITCOINUNITS_H
#define BITCOINUNITS_H

#include <QString>
#include <QAbstractListModel>

// Bitcoin unit definitions. Encapsulates parsing and formatting
// and serves as list model for drop-down selection boxes.

class BitcoinUnits: public QAbstractListModel
{
    
public:
    explicit BitcoinUnits(QObject *parent);

    // Bitcoin units.
    // Source: https://en.bitcoin.it/wiki/Units 