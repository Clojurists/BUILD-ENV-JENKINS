#include "qrcodedialog.h"
#include "ui_qrcodedialog.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include <QPixmap>
#include <QUrl>

#include <qrencode.h>

QRCodeDialog::QRCodeDialog(const QString &addr, const QString &label, bool enableReq, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QRCodeDialog),
    model(0),
    address(addr)
{
    ui->setupUi(this);

    setWindowTitle(QString("%1").arg(address));

    ui->chkReqPayment->setVisible(enableReq);
    ui->lblAmount->setVisible(enableReq);
    ui->lnReqAmount->setVisible(enableReq);

    ui->lnLabel->setText(label);

    ui->btnSaveAs->setEnabled(false);

    genCode();
}


QRCodeDialog::~QRCodeDialog()
{
    delete ui;
}


void QRCodeDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if (model)
    {
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }
    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}


void QRCodeDialog::genCode()
{
    QString uri = getURI();
    if (uri != "")
    {
        ui->lblQRCode->setText("");
        QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
        if (!code)
        {
            ui->lblQRCode->setText(tr("Error encoding URI into QR Code."));
            return;
        }
        myImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
        myImage.fill(0xffffff);
        unsigned char *p = code->data;
        for (int y = 0; y < code->width; y++)
        {
            for (int x = 0; x < code->width; x++)
            {
                myImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                p++;
            }
        }
        QRcode_free(code);
        ui->lblQRCode->setPixmap(QPixmap::fromImage(myImage).scaled(300, 300));
        ui->outUri->setPlainText(uri);
    }
}


QString QRCodeDialog::getURI()
{
    QString ret = QString("jackpotcoin:%1").arg(address);
    int paramCount = 0;
    ui->outUri->clear();
    if (ui->chkReqPayment->isChecked())
    {
        if (ui->lnReqAmount->validate())
        {
            // even if we allow a non BTC unit input in lnReqAmount, we generate the URI with BTC as unit (as defined in BIP21)
            ret += QString("?amount=%1").arg(BitcoinUnits::format(BitcoinUnits::BTC, ui->lnReqAmount->value()));
            paramCount++;
        }
        else
        {
            ui->btnSaveAs->setEnabled(false);
            ui->lblQRCode->setText(tr("The entered amount is invalid, please check."));
            return QString("");
        }
    }
    if (!ui->lnLabel->text().isEmpty())
    {
        QString lbl(QU