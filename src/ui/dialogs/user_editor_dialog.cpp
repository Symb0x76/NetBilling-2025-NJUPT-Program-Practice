#include "ui/dialogs/user_editor_dialog.h"

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVariant>
#include <QVBoxLayout>

UserEditorDialog::UserEditorDialog(Mode mode, QWidget* parent)
    : ElaDialog(parent)
    , m_mode(mode)
{
    setWindowTitle(mode == Mode::Create ? QStringLiteral(u8"新增用户") : QStringLiteral(u8"编辑用户"));
    setFixedSize(420, 280);
    setupUi();
}

void UserEditorDialog::setupUi()
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignTop);
    formLayout->setVerticalSpacing(12);

    m_nameEdit = new ElaLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral(u8"请输入用户姓名"));
    formLayout->addRow(QStringLiteral(u8"姓名"), m_nameEdit);

    m_accountEdit = new ElaLineEdit(this);
    m_accountEdit->setPlaceholderText(QStringLiteral(u8"请输入账号（唯一编号）"));
    formLayout->addRow(QStringLiteral(u8"账号"), m_accountEdit);

    m_planCombo = new ElaComboBox(this);
    m_planCombo->addItem(QStringLiteral(u8"标准计费"), QVariant::fromValue(static_cast<int>(Tariff::NoDiscount)));
    m_planCombo->addItem(QStringLiteral(u8"30 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack30h)));
    m_planCombo->addItem(QStringLiteral(u8"60 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack60h)));
    m_planCombo->addItem(QStringLiteral(u8"150 小时套餐"), QVariant::fromValue(static_cast<int>(Tariff::Pack150h)));
    m_planCombo->addItem(QStringLiteral(u8"包月不限时"), QVariant::fromValue(static_cast<int>(Tariff::Unlimited)));
    formLayout->addRow(QStringLiteral(u8"资费方案"), m_planCombo);

    layout->addLayout(formLayout);
    layout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* cancelButton = new ElaPushButton(QStringLiteral(u8"取消"), this);
    auto* okButton = new ElaPushButton(QStringLiteral(u8"确定"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    setCentralWidget(central);

    if (m_mode == Mode::Edit)
    {
        m_accountEdit->setReadOnly(true);
    }

    connect(cancelButton, &ElaPushButton::clicked, this, &UserEditorDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, &UserEditorDialog::accept);
}

void UserEditorDialog::setUser(const User& user)
{
    m_nameEdit->setText(user.name);
    m_accountEdit->setText(user.account);
    const int planIndex = m_planCombo->findData(static_cast<int>(user.plan));
    if (planIndex >= 0)
    {
        m_planCombo->setCurrentIndex(planIndex);
    }
}

User UserEditorDialog::user() const
{
    User result;
    result.name = m_nameEdit->text().trimmed();
    result.account = m_accountEdit->text().trimmed();
    result.plan = static_cast<Tariff>(m_planCombo->currentData().toInt());
    return result;
}

QString UserEditorDialog::validate() const
{
    if (m_nameEdit->text().trimmed().isEmpty())
        return QStringLiteral(u8"姓名不能为空。");
    if (m_accountEdit->text().trimmed().isEmpty())
        return QStringLiteral(u8"账号不能为空。");
    return {};
}

void UserEditorDialog::accept()
{
    const auto error = validate();
    if (!error.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    ElaDialog::accept();
}
