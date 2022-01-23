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
            return tr("%1/offline").arg(