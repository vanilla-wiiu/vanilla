#include "syncdialog.h"

#include <QDialogButtonBox>
#include <QPushButton>

#include "syncprogressdialog.h"

QString SyncDialog::g_symbols[g_symbolCount] = {
    QString::fromUtf8("\xE2\x99\xA0"),     // Spade = 0
    QString::fromUtf8("\xE2\x99\xA6"),     // Diamond = 1
    QString::fromUtf8("\xE2\x99\xA5"),     // Heart = 2
    QString::fromUtf8("\xE2\x99\xA3")      // Club = 3
};

SyncDialog::SyncDialog(QWidget *parent) : QDialog(parent)
{
    for (int i = 0; i < g_symbolCount; i++) {
        m_code[i] = -1;
    }

    QVBoxLayout *outerLayout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(tr(
        "<html>"
        "<h3>Touch the symbols in the order they are displayed on the TV screen from left to right.</h3>"
        "<p>If the symbols are not displayed on the TV screen, press the SYNC button on the Wii U console.</p>"
        "</html>"));
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setWordWrap(true);
    outerLayout->addWidget(infoLabel);

    QHBoxLayout *symbolLayout = new QHBoxLayout();
    QFont f = font();
    f.setPointSize(40);
    QFontMetrics fm(f);
    symbolLayout->addStretch();
    for (int i = 0; i < g_symbolCount; i++) {
        QLabel *b = new QLabel(this);
        b->setFont(f);
        b->setAlignment(Qt::AlignCenter);
        b->setFixedSize(fm.height(), fm.height());
        symbolLayout->addWidget(b);
        m_labels[i] = b;
    }
    symbolLayout->addStretch();
    outerLayout->addLayout(symbolLayout);

    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->addStretch();
    QPushButton *b;
    for (int i = 0; i < g_symbolCount; i++) {
        b = createButton(g_symbols[i], i);
    }

    QFrame *vline = new QFrame();
    vline->setFixedSize(3, b->height());
    vline->setFrameShape(QFrame::VLine);
    vline->setFrameShadow(QFrame::Sunken);
    m_buttonLayout->addWidget(vline);

    createButton(QString::fromUtf8("\xE2\xAC\x85"), -1);

    m_buttonLayout->addStretch();
    outerLayout->addLayout(m_buttonLayout);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    outerLayout->addWidget(buttons);

    setWindowTitle(tr("Sync"));
    updateLabels();
}

QPushButton *SyncDialog::createButton(const QString &text, int id)
{
    QFont f = this->font();
    f.setPointSize(16);
    QPushButton *b = new QPushButton(text, this);
    b->setProperty("id", id);
    b->setFixedSize(b->sizeHint().height(), b->sizeHint().height());
    b->setFont(f);
    m_buttonLayout->addWidget(b);
    connect(b, &QPushButton::clicked, this, &SyncDialog::buttonClicked);
    return b;
}

void SyncDialog::buttonClicked()
{
    QPushButton *b = static_cast<QPushButton *>(sender());
    int id = b->property("id").toInt();
    if (id == -1) {
        for (int i = g_symbolCount-1; i >= 0; i--) {
            if (m_code[i] != -1) {
                m_code[i] = -1;
                updateLabels();
                break;
            }
        }
    } else {
        for (int i = 0; i < g_symbolCount; i++) {
            if (m_code[i] == -1) {
                m_code[i] = id;
                updateLabels();
                if (i == g_symbolCount - 1) {
                    launchSync();
                }
                break;
            }
        }
    }
}

void SyncDialog::updateLabels()
{
    static QString empty = QString::fromUtf8("\xE2\x80\xA2");
    for (int i = 0; i < g_symbolCount; i++) {
        m_labels[i]->setText((m_code[i] == -1) ? empty : g_symbols[m_code[i]]);
    }
}

int intPow(int x, unsigned int p)
{
  if (p == 0) return 1;
  if (p == 1) return x;
  
  int tmp = intPow(x, p/2);
  if (p%2 == 0) return tmp * tmp;
  else return x * tmp * tmp;
}

void SyncDialog::launchSync()
{
    SyncProgressDialog *progressDialog = new SyncProgressDialog(this->parentWidget());
    progressDialog->open();

    uint16_t code = 0;
    for (int i = 0; i < g_symbolCount; i++) {
        code += m_code[i] * intPow(10, g_symbolCount - 1 - i);
    }

    progressDialog->start(m_wirelessInterface, code);
    this->close();
}

void SyncDialog::setup(const QString &wirelessInterface)
{
    m_wirelessInterface = wirelessInterface;
}
