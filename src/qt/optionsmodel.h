#ifndef OPTIONSMODEL_H
#define OPTIONSMODEL_H

#include <QAbstractListModel>

// Interface from Qt to configuration data structure for Bitcoin client.
// To Qt, the options are presented as a list with the different options
// laid out vertically.
// This can be changed to a tree once the settings become sufficiently
// complex.

class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OptionsModel(QObject *parent = 0);

    enum OptionID {
        StartAtStartup,      // bool
        MinimizeToTray,      // bool
        MapPortUPnP,         // bool
        MinimizeOnClose,     // bool
        ProxyUse,            // bool
        ProxyIP,             // QString
        ProxyPort,           // int
        ProxySocksVersion,   // int
        Fee,                 // qint64
        DisplayUnit,         // BitcoinUnits::Unit
        DisplayAddresses,    // bool
        DetachDatabases,     // bool
        Language,            // QString
		CoinControlFeatures, // bool
        ReserveBalance,      // qint64
        HideNotification,    // Bool
        HideInvalid,         // Bool
        OptionIDRowCount,    // Used to define the number of options, should be end of enum.
    };

    void Init();

    // Migrate settings from wallet.dat after app initialization
    // returns true if settings upgraded
    bool Upgrade(); 

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

    // Explicit getters
    qint64 getTransactionFee();
    qint64 getReserveBalance();
    bool getMinimizeToTray();
    bool getMinimizeOnClose();
    int getDisplayUnit();
    bool getDisplayAddresses();
    bool getHideNotification();
    bool getHideInvalid();
    QString getLanguage() { return language; }
	bool getCoinControlFeatures();

private:
    int nDisplayUnit;
    bool fDisplayAddresses;
    bool fMinimizeToTray;
    bool fMinimizeOnClose;
    bool fHideNotification;
    bool fHideInvalid;
	bool fCoinControlFeatures;
    QString language;

signals:
    void displayUnitChanged(int unit);
	void transactionFeeChanged(qint64);
    void coinControlFeaturesChanged(bool);
    void reserveBalanceChanged(qint64);
    
};

#endif // OPTIONSMODEL_H
