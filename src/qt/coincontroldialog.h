#ifndef COINCONTROLDIALOG_H
#define COINCONTROLDIALOG_H

#include <QAbstractButton>
#include <QAction>
#include <QDialog> 
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QTreeWidgetItem>

namespace Ui {
    class CoinControlDialog;
}
class WalletModel;
class CCoinControl;

class CoinControlDialog : public QDialog
{
    
    Q_OBJECT

public:
    explicit CoinControlDialog(QWidget *parent = 0);
    ~CoinControlDialog();

    void setModel(WalletModel *model);

    // static because also called from sendcoinsdialog
    static void updateLabels(WalletModel*, QDialog*);
    static QString getPriorityLabel(double);

    static QList<qint64> payAmounts;
    static CCoinControl *coinControl;

private:
    Ui::CoinControlDialog *ui;
    WalletModel *model;
    int sortColumn;
    Qt::SortOrder sortOrder;

    QMenu *contextMenu;
    QTreeWidgetItem *contextMenuItem;
    QAction *copyTransactionHashAction;

    QString strPad(QString, int, QString);
    void sortView(int, Qt::SortOrder);
    void updateView();

    enum
    {
        COLUMN_CHECKBOX,