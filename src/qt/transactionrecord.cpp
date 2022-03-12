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
    int64_t nCredit = wtx.GetCredit(true);
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash(), hashPrev = 0;
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    if ((nNet > 0) || (wtx.IsCoinBase() || wtx.IsCoinStake()))
    {
        BOOST_FOREACH (const CTxOut& txout, wtx.vout)
        {
            if (wallet->IsMine(txout))
            {
                TransactionRecord sub(hash, nTime);
                CTxDestination address;
                sub.idx = parts.size(); // sequence number
                sub.credit = txout.nValue;
                if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address))
                {
                    // Received by Bitcoin Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction