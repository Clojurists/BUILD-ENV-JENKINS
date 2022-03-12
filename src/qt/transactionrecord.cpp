#include "transactionrecord.h"
#include "wallet.h"
#include "base58.h"

// Return positive answer if transaction should be shown in list.
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    if (wtx.IsCoinBase() || wtx.IsCoinStake())
    {
        //
        // Ensures we show generated coins / mined transactions at depth 1
        //
        // return (wtx.IsInMainChain());
    }
    return true;
}

// Decompose CWallet transaction to model transaction records.
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.GetTxTime();
    int64_