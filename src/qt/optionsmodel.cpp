
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "init.h"
#include "walletdb.h"
#include "guiutil.h"

#include <QSettings>

OptionsModel::OptionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
    Init();
}

bool static ApplyProxySettings()
{
    QSettings settings;
    CService addrProxy(settings.value("addrProxy", "127.0.0.1:9050").toString().toStdString());
    int nSocksVersion(settings.value("nSocksVersion", 5).toInt());
    if (!settings.value("fUseProxy", false).toBool()) 
    {
        addrProxy = CService();
        nSocksVersion = 0;
        return false;
    }
    if (nSocksVersion && !addrProxy.IsValid())
    {
        return false;
    }
    if (!IsLimited(NET_IPV4))
    {
        SetProxy(NET_IPV4, addrProxy, nSocksVersion);
    }
    if (nSocksVersion > 4) 
    {
#ifdef USE_IPV6
        if (!IsLimited(NET_IPV6)) 
        {
            SetProxy(NET_IPV6, addrProxy, nSocksVersion);
        }
#endif
        SetNameProxy(addrProxy, nSocksVersion);
    }
    return true;
}


void OptionsModel::Init()
{
    QSettings settings;

    // These are Qt-only settings:
    nDisplayUnit = settings.value("nDisplayUnit", BitcoinUnits::BTC).toInt();
    fDisplayAddresses = settings.value("fDisplayAddresses", false).toBool();
    fMinimizeToTray = settings.value("fMinimizeToTray", false).toBool();
    fMinimizeOnClose = settings.value("fMinimizeOnClose", false).toBool();
	fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();
    nTransactionFee = settings.value("nTransactionFee").toLongLong();
    language = settings.value("language", "").toString();
    nReserveBalance = settings.value("nReserveBalance").toLongLong();
    fHideNotification = settings.value("fHideNotification", false).toBool();
    fHideInvalid = settings.value("fHideInvalid", true).toBool();

    // These are shared with core Bitcoin; we want
    // command-line options to override the GUI settings:
    if (settings.contains("fUseUPnP"))
    {
        SoftSetBoolArg("-upnp", settings.value("fUseUPnP").toBool());
    }
    if (settings.contains("addrProxy") && settings.value("fUseProxy").toBool())
    {
        SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString());
    }
    if (settings.contains("nSocksVersion") && settings.value("fUseProxy").toBool())
    {
        SoftSetArg("-socks", settings.value("nSocksVersion").toString().toStdString());
    }
    if (settings.contains("detachDB"))
    {
        SoftSetBoolArg("-detachdb", settings.value("detachDB").toBool());
    }
    if (!language.isEmpty())
    {
        SoftSetArg("-lang", language.toStdString());
    }
}


bool OptionsModel::Upgrade()
{
    QSettings settings;

    if (settings.contains("bImportFinished"))
    {
        // Already upgraded
        return false;
    }

    settings.setValue("bImportFinished", true);

    // Move settings from old wallet.dat (if any):
    CWalletDB walletdb(strWalletFileName);

    QList<QString> intOptions;
    intOptions << "nDisplayUnit" << "nTransactionFee" << "nReserveBalance";
    foreach (QString key, intOptions)
    {
        int value = 0;
        if (walletdb.ReadSetting(key.toStdString(), value))
        {
            settings.setValue(key, value);
            walletdb.EraseSetting(key.toStdString());
        }
    }
    QList<QString> boolOptions;
    boolOptions << "fDisplayAddresses" << "fMinimizeToTray" << "fMinimizeOnClose" << "fUseProxy" << "fUseUPnP" << "fHideNotification" << "fHideInvalid";
    foreach(QString key, boolOptions)
    {
        bool value = false;
        if (walletdb.ReadSetting(key.toStdString(), value))
        {
            settings.setValue(key, value);
            walletdb.EraseSetting(key.toStdString());
        }
    }
    try
    {
        CAddress addrProxyAddress;
        if (walletdb.ReadSetting("addrProxy", addrProxyAddress))
        {
            settings.setValue("addrProxy", addrProxyAddress.ToStringIPPort().c_str());
            walletdb.EraseSetting("addrProxy");
        }
    }
    catch (std::ios_base::failure &e)
    {
        // 0.6.0rc1 saved this as a CService, which causes failure when parsing as a CAddress
        CService addrProxy;
        if (walletdb.ReadSetting("addrProxy", addrProxy))
        {
            settings.setValue("addrProxy", addrProxy.ToStringIPPort().c_str());
            walletdb.EraseSetting("addrProxy");
        }
    }
    ApplyProxySettings();
    Init();

    return true;
}


int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}


QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if (role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
          case StartAtStartup:
               return QVariant(GUIUtil::GetStartOnSystemStartup());
          case MinimizeToTray:
               return QVariant(fMinimizeToTray);
          case MapPortUPnP:
#ifdef USE_UPNP
               return settings.value("fUseUPnP", GetBoolArg("-upnp", true));
#else
               return QVariant(false);
#endif
          case MinimizeOnClose:
               return QVariant(fMinimizeOnClose);
          case ProxyUse:
               return settings.value("fUseProxy", false);
          case ProxyIP: 
          {
               proxyType proxy;
               if (GetProxy(NET_IPV4, proxy))
               {
                   return QVariant(QString::fromStdString(proxy.first.ToStringIP()));
               }
               else
               {
                   return QVariant(QString::fromStdString("127.0.0.1"));
               }
          }
          case ProxyPort: 
          {
               proxyType proxy;
               if (GetProxy(NET_IPV4, proxy))
               {
                   return QVariant(proxy.first.GetPort());
               }
               else
               {
                  return QVariant(9050);
               }
          }
          case ProxySocksVersion:
               return settings.value("nSocksVersion", 5);
          case Fee:
               return QVariant((qint64) nTransactionFee);
          case ReserveBalance:
               return QVariant((qint64) nReserveBalance);
          case DisplayUnit:
               return QVariant(nDisplayUnit);
          case DisplayAddresses:
               return QVariant(fDisplayAddresses);
          case DetachDatabases:
               return QVariant(bitdb.GetDetach());
          case Language:
               return settings.value("language", "");
		  case CoinControlFeatures:
               return QVariant(fCoinControlFeatures);
          case HideNotification:
               return QVariant(fHideNotification);
          case HideInvalid:
               return QVariant(fHideInvalid);
          default:
               return QVariant();
        }
    }
    return QVariant();
}


bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true;
    if(role == Qt::EditRole)
    {
        QSettings settings;
        switch(index.row())
        {
          case StartAtStartup:
               successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
               break;
          case MinimizeToTray:
               fMinimizeToTray = value.toBool();
               settings.setValue("fMinimizeToTray", fMinimizeToTray);
               break;
          case MapPortUPnP:
               fUseUPnP = value.toBool();
               settings.setValue("fUseUPnP", fUseUPnP);
               MapPort();
               break;
          case MinimizeOnClose:
               fMinimizeOnClose = value.toBool();
               settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
               break;
          case ProxyUse:
               settings.setValue("fUseProxy", value.toBool());
               ApplyProxySettings();
               break;
          case ProxyIP: 
          {
               proxyType proxy;
               proxy.first = CService("127.0.0.1", 9050);
               GetProxy(NET_IPV4, proxy);
               CNetAddr addr(value.toString().toStdString());
               proxy.first.SetIP(addr);
               settings.setValue("addrProxy", proxy.first.ToStringIPPort().c_str());
               successful = ApplyProxySettings();
          }
               break;
          case ProxyPort: 
          {
               proxyType proxy;
               proxy.first = CService("127.0.0.1", 9050);
               GetProxy(NET_IPV4, proxy);
               proxy.first.SetPort(value.toInt());
               settings.setValue("addrProxy", proxy.first.ToStringIPPort().c_str());
               successful = ApplyProxySettings();
          }
               break;
          case ProxySocksVersion: 
          {
               proxyType proxy;
               proxy.second = 5;
               GetProxy(NET_IPV4, proxy);
               proxy.second = value.toInt();
               settings.setValue("nSocksVersion", proxy.second);
               successful = ApplyProxySettings();
          }
               break;
          case Fee:
               nTransactionFee = value.toLongLong();
               settings.setValue("nTransactionFee", (qint64) nTransactionFee);
			   emit transactionFeeChanged(nTransactionFee);
               break;
          case ReserveBalance:
               nReserveBalance = value.toLongLong();
               settings.setValue("nReserveBalance", (qint64) nReserveBalance);
               emit reserveBalanceChanged(nReserveBalance);
               break;
          case DisplayUnit:
               nDisplayUnit = value.toInt();
               settings.setValue("nDisplayUnit", nDisplayUnit);
               emit displayUnitChanged(nDisplayUnit);
               break;
          case DisplayAddresses:
               fDisplayAddresses = value.toBool();
               settings.setValue("fDisplayAddresses", fDisplayAddresses);
               break;
          case DetachDatabases: 
          {
               bool fDetachDB = value.toBool();
               bitdb.SetDetach(fDetachDB);
               settings.setValue("detachDB", fDetachDB);
          }
               break;
          case Language:
               settings.setValue("language", value);
               break;
          case CoinControlFeatures: 
          {
               fCoinControlFeatures = value.toBool();
               settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
               emit coinControlFeaturesChanged(fCoinControlFeatures);
          }
               break;
          case HideNotification:
          {
               fHideNotification = value.toBool();
               settings.setValue("fHideNotification", fHideNotification);
          }
               break;
          case HideInvalid:
          {
               fHideInvalid = value.toBool();
               settings.setValue("fHideInvalid", fHideInvalid);
          }
               break;
          default:
               break;
        }
    }
    emit dataChanged(index, index);

    return successful;
}


qint64 OptionsModel::getTransactionFee()
{
    return nTransactionFee;
}


qint64 OptionsModel::getReserveBalance()
{
    return nReserveBalance;
}


bool OptionsModel::getCoinControlFeatures()
{
     return fCoinControlFeatures;
}


bool OptionsModel::getMinimizeToTray()
{
    return fMinimizeToTray;
}


bool OptionsModel::getMinimizeOnClose()
{
    return fMinimizeOnClose;
}


int OptionsModel::getDisplayUnit()
{
    return nDisplayUnit;
}


bool OptionsModel::getDisplayAddresses()
{
    return fDisplayAddresses;
}


bool OptionsModel::getHideNotification()
{
    return fHideNotification;
}


bool OptionsModel::getHideInvalid()
{
    return fHideInvalid;
}