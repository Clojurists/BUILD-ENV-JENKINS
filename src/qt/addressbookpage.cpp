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
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *showQRCodeAction = new QAction(ui->showQRCode->text(), this);
    QAction *signMessageAction = new QAction(ui->signMessage->text(), this);
    QAction *verifyMessageAction = new QAction(ui->verifyMessage->text(), this);
    deleteAction = new QAction(ui->deleteAddress->text(), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(editAction);
    if (tab == SendingTab)
    {
        contextMenu->addAction(deleteAction);
    }
    contextMenu->addSeparator();
    contextMenu->addAction(showQRCodeAction);
    if (tab == ReceivingTab)
    {
        contextMenu->addAction(signMessageAction);
    }
    else if (tab == SendingTab)
    {
        contextMenu->addAction(verifyMessageAction);
    }

    // Connect signals for context menu actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyAddress_clicked()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction()));
    connect(editAction, SIGNAL(triggered()), this, SLOT(onEditAction()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(on_deleteAddress_clicked()));
    connect(showQRCodeAction, SIGNAL(triggered()), this, SLOT(on_showQRCode_clicked()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(on_signMessage_clicked()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(on_verifyMessage_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    // Pass through accept action from button box
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
}


AddressBookPage::~AddressBookPage()
{
    delete ui;
}


void AddressBookPage::setModel(AddressTableModel *model)
{
    this->model = model;
    if (!model)
    {
        return;
    }
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    switch(tab)
    {
      case ReceivingTab:
           // Receive filter
           proxyModel->setFilterRole(AddressTableModel::TypeRole);
           proxyModel->setFilterFixedString(AddressTableModel::Receive);
           break;
      case SendingTab:
           // Send filter
           proxyModel->setFilterRole(AddressTableModel::TypeRole);
           proxyModel->setFilterFixedString(AddressTableModel::Send);
           break;
    }
    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
    ui->tableView->horizontalHeader()->resizeSection(AddressTableModel::Address, 320);
    ui->tableView->horizontalHeader()->setResizeMode(AddressTableModel::Label, QHeaderView::Stretch);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));

    // Select row for newly created address
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(selectNewAddress(QModelIndex,int,int)));

    selectionChanged();
}


void AddressBookPage::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}


void AddressBookPage::on_copyAddress_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Address);
}


void AddressBookPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Label);
}


void AddressBookPage::onEditAction()
{
    if (!ui->tableView->selectionModel())
    {
        return;
    }
    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();
    if (indexes.isEmpty())
    {
        return;
    }
    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::EditSendingAddress :
            EditAddressDialog::EditReceivingAddress);
    dlg.setModel(model);
    QModelIndex origIndex = proxyModel->mapToSource(indexes.at(0));
    dlg.loadRow(origIndex.row());
    dlg.exec();
}


void AddressBookPage::on_signMessage_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

   