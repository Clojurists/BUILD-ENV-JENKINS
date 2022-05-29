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
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();
    if (cachedEncryptionStatus != newEncryptionStatus) 
    {
        emit encryptionStatusChanged(newEncryptionStatus);
    }
}


void WalletModel::pollBalanceChanged()
{
    if (nBestHeight != cachedNumBlocks)
    {
        // Balance and number of transactions might have changed
        cachedNumBlocks = nBestHeight;
        checkBalanceChanged();
        // Block index has changed
        emit nBestHeightChanged(nBestHeight);
    }
}


void WalletModel::checkBalanceChanged()
{
    qint64 newBalance = getBalance();
    qint64 newStake = getStake();
    qint64 newUnconfirmedBalance = getUnconfirmedBalance();
    qint64 newImmatureBalance = getImmatureBalance();
    
    if ((cachedBalance != newBalance) || 
        (cachedStake != newStake) || 
        (cachedUnconfirmedBalance != newUnconfirmedBalance) || 
        (cachedImmatureBalance != newImmatureBalance))
    {
        cachedBalance = newBalance;
        cachedStake = newStake;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        emit balanceChanged(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance);
    }
}


void WalletModel::updateTransaction(const QString &hash, int status)
{
    if (transactionTableModel)
    {
        transactionTableModel->updateTransaction(hash, status);
    }
    
    // Balance and number of transactions might have changed
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if (cachedNumTransactions != newNumTransactions)
    {
        cachedNumTransactions = newNumTransactions;
        emit numTransactionsChanged(newNumTransactions);
    }
}


void WalletModel::updateAddressBook(const QString &address, const QString &label, bool isMine, int status)
{
    if (addressTableModel) 
    {
        addressTableModel->updateEntry(address, label, isMine, status);
    }
}


bool WalletModel::validateAddress(const QString &address)
{
    CBitcoinAddress addressParsed(address.toStdString());
    return addressParsed.IsValid();
}


WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients, const CCoinCont