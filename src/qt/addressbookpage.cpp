#include "addressbookpage.h"
#include "ui_addressbookpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

AddressBookPage::AddressBookPage(Mode mode, Tabs tab, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddressBookPage),
    model(0),
    optionsModel(0),
    mode(mode),
    tab(tab)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->newAddress->setIcon(QIcon());
    ui->copyAddress->setIcon(QIcon());
    ui->deleteAddress->setIcon(QIcon());
#endif

#ifndef USE_QRCODE
    ui->showQRCode->setVisible(false);
#endif

    switch (mode)
    {
      case ForSending:
           connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));
           ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
           ui->tableView->setFocus();
           break;
      case ForEditing:
           ui->buttonBox->setVisible(false);
           break;
    }
    switch (tab)
    {
      case SendingTab:
           ui->labelExplanation->setVisible(false);
           ui->deleteAddress->setVisible(true);
           ui->signMessage->setVisible(false);
           break;
      case ReceivingTab:
           ui->deleteAddress->setVisible(false);
           ui->signMessage->setVisible(true);
           break;
    }

    // Context menu actions
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *copyAddressAction = new QAction(ui->copyAddress->text(), this);
    QAction *editActi