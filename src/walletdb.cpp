
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletdb.h"
#include "wallet.h"
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;


static uint64 nAccountingEntryNumber = 0;
extern bool fWalletUnlockMintOnly;

//
// CWalletDB
//

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::ReadAccount(const string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const string& strAccount, const CAccount& account)
{
    return Write(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccountingEntry(const uint64 nAccEntryNum, const CAccountingEntry& acentry)
{
    return Write(boost::make_tuple(string("acentry"), acentry.strAccount, nAccEntryNum), acentry);
}

bool CWalletDB::WriteAccountingEntry(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

int64 CWalletDB::GetAccountCreditDebit(const string& strAccount)
{
    list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    int64 nCreditDebit = 0;
    BOOST_FOREACH (const CAccountingEntry& entry, entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const string& strAccount, list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    while (true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << boost::make_tuple(string("acentry"), (fAllAccounts? string("") : strAccount), uint64(0));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
        }

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;
        entries.push_back(acentry);
    }

    pcursor->close();
}


DBErrors
CWalletDB::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);
    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef multimap<int64, TxPair > TxItems;
    TxItems txByTime;

    for (map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txByTime.insert(make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext = 0;
    std::vector<int64> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        CAccountingEntry *const pacentry = (*it).second.second;
        int64& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1)
        {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pacentry)
                // Have to write accounting regardless, since we don't keep it in memory
                if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        }
        else
        {
            int64 nOrderPosOff = 0;
            BOOST_FOREACH(const int64& nOffsetStart, nOrderPosOffsets)
            {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx)
            {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            }
            else
                if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        }
    }

    return DB_LOAD_OK;
}


bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             int& nFileVersion, vector<uint256>& vWalletUpgrade,
             bool& fIsEncrypted,  bool& fAnyUnordered, string& strType, string& strErr)
{
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name")
        {
            string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[CBitcoinAddress(strAddress).Get()];
        }
        else if (strType == "tx")
        {
            uint256 hash;
            ssKey >> hash;
            CWalletTx& wtx = pwallet->mapWallet[hash];
            ssValue >> wtx;
            if (wtx.CheckTransaction() && (wtx.GetHash() == hash))
                wtx.BindWallet(pwallet);
            else
            {
                pwallet->mapWallet.erase(hash);
                return false;
            }

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
            {
                if (!ssValue.empty())
                {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount.c_str(), hash.ToString().c_str());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                }
                else
                {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString().c_str());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                fAnyUnordered = true;

            //// debug print
            //printf("LoadWallet  %s\n", wtx.GetHash().ToString().c_str());
            //printf(" %12"PRI64d"  %s  %s  %s\n",
            //    wtx.vout[0].nValue,
            //    DateTimeStrFormat("%x %H:%M:%S", wtx.GetBlockTime()).c_str(),
            //    wtx.hashBlock.ToString().substr(0,20).c_str(),
            //    wtx.mapValue["message"].c_str());
        }
        else if (strType == "acentry")
        {
            string strAccount;
            ssKey >> strAccount;
            uint64 nNumber;
            ssKey >> nNumber;
            if (nNumber > nAccountingEntryNumber)
                nAccountingEntryNumber = nNumber;

            if (!fAnyUnordered)
            {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    fAnyUnordered = true;
            }
        }
        else if (strType == "key" || strType == "wkey")
        {
            vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            CKey key;
            if (strType == "key")
            {
                CPrivKey pkey;
                ssValue >> pkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(pkey))
                {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (key.GetPubKey() != vchPubKey)
                {
                    strErr = "Error reading wallet database: CPrivKey pubkey inconsistency";
                    return false;
                }
                if (!key.IsValid())
                {
                    strErr = "Error reading wallet database: invalid CPrivKey";
                    return false;
                }
            }
            else
            {
                CWalletKey wkey;
                ssValue >> wkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(wkey.vchPrivKey))
                {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (key.GetPubKey() != vchPubKey)
                {
                    strErr = "Error reading wallet database: CWalletKey pubkey inconsistency";
                    return false;
                }
                if (!key.IsValid())
                {
                    strErr = "Error reading wallet database: invalid CWalletKey";
                    return false;
                }
            }
            if (!pwallet->LoadKey(key))
            {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        }
        else if (strType == "mkey")
        {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;