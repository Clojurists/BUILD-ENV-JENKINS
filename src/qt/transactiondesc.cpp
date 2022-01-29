#include "transactiondesc.h"
#include "guiutil.h"
#include "bitcoinunits.h"
#include "main.h"
#include "wallet.h"
#include "db.h"
#include "ui_interface.h"
#include "base58.h"

QString TransactionDesc::FormatTxStatus(const CWalletTx& wtx)
{
    if (!wtx.IsFinal())
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            return tr("Open for %n more block(s)", "", (wtx.nLockTime - nBestHeight));
        }
        else
        {
            return tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx.nLockTime));
        }
    }
    else
    {
        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < 0)
        {
            return tr("conflicted");
        }
        else if (((GetAdjustedTime() - wtx.nTimeReceived) > (2 * 60)) && (wtx.GetRequestCount() == 0))
        {
            return tr("%1/offline").arg(nDepth);
        }
        else if (nDepth < 3)
        {
            return tr("%1/unconfirmed").arg(nDepth);
        }
        else
        {
            return tr("%1 confirmations").arg(nDepth);
        }
    }
}

QString TransactionDesc::toHTML(CWallet *wallet, CWalletTx &wtx)
{
    QString strHTML;

    {
        LOCK(wallet->cs_wallet);
        strHTML.reserve(4000);
        strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

        int64 nTime = wtx.GetTxTime();
        int64 nCredit = wtx.GetCredit();
        int64 nDebit = wtx.GetDebit();
        int64 nNet = nCredit - nDebit;

        strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(wtx);
        
        int nRequests = wtx.GetRequestCount();
        if (nRequests != -1)
        {
            if (nRequests == 0)
            {
                strHTML += tr(", has not been successfully broadcast yet");
            }
            else if (nRequests > 0)
            {
                strHTML += tr(", broadcast through %n node(s)", "", nRequests);
            }
        }
        strHTML += "