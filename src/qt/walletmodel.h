#ifndef WALLETMODEL_H
#define WALLETMODEL_H

#include <QObject>
#include <vector>
#include <map>

#include "allocators.h"

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CWallet;
class CKeyID;
class CPubKey;
class COutput;
class COutPoint;
class uint256;
class CCoinControl;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient
{
public:
    QString address;
    QString label;
    qint64 amount;
};

//  Interface to Bitcoin wallet from Qt view code.
class WalletModel : public QObject
{
    Q_OBJECT

public:
    explicit WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent = 0);
    ~WalletModel();
    
    // Returned status by sendCoins
    enum StatusCode 
    {
        OK,
        Inv