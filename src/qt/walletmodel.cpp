#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"

#include "ui_interface.h"
#include "wallet.h"
#include "walletdb.h"
#include "base58.h"

#include <QSet>
#include <QTimer>

extern bool fWalletUnlockMintOnly;

WalletModel::WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
    transactionTableModel(0),
    cachedBalance(0), cachedStake(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
    cachedNumTransactions(0),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    addressTableModel = new AddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}


WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}


qint64 WalletModel::getBalance() const
{
    return wallet->GetBalance();
}


qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}


qint64 WalletModel::getStake() const
{
    return wallet->GetStake();
}


qint64 WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}


int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        // the size of mapWallet contains the number of unique transaction IDs
        // (e.g. payments to yourself generate 2 transactions, but both share the same transaction ID)
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}


void WalletModel::updateStatus()
{
    EncryptionStatus