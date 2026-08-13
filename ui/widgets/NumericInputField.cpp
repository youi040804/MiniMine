#include "NumericInputField.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleValidator>
#include <QSignalBlocker>

NumericInputField::NumericInputField(double minimum,
                                     double maximum,
                                     int decimals,
                                     QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_edit = new QLineEdit(this);
    m_edit->setAlignment(Qt::AlignCenter);
    m_edit->setClearButtonEnabled(false);

    m_validator = new QDoubleValidator(minimum, maximum, decimals, m_edit);
    m_validator->setNotation(QDoubleValidator::StandardNotation);
    m_edit->setValidator(m_validator);

    m_clearButton = new QPushButton(QStringLiteral("×"), this);
    m_clearButton->setFixedSize(24, 24);
    m_clearButton->setFlat(true);
    m_clearButton->setToolTip(QStringLiteral("清空为未填写"));

    layout->addWidget(m_edit, 1);
    layout->addWidget(m_clearButton);

    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        clearValue();
    });
    connect(m_edit, &QLineEdit::textChanged, this, [this]() {
        updateValidationStyle();
        if (m_changedCallback) {
            m_changedCallback();
        }
    });
    connect(m_edit, &QLineEdit::editingFinished, this, [this]() {
        if (!isFilled() || !isAcceptable()) {
            updateValidationStyle();
            return;
        }

        bool ok = false;
        const double value = toDouble(&ok);
        if (ok) {
            QSignalBlocker blocker(m_edit);
            m_edit->setText(QString::number(value, 'f', m_validator->decimals()));
        }
        updateValidationStyle();
    });
}

void NumericInputField::setChangedCallback(const std::function<void()>& callback)
{
    m_changedCallback = callback;
}

bool NumericInputField::isFilled() const
{
    return !m_edit->text().trimmed().isEmpty();
}

bool NumericInputField::isAcceptable() const
{
    if (!isFilled()) {
        return true;
    }

    QString text = m_edit->text().trimmed();
    int pos = 0;
    return m_validator->validate(text, pos) == QValidator::Acceptable;
}

double NumericInputField::toDouble(bool* ok) const
{
    bool localOk = false;
    QString text = m_edit->text().trimmed();
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    const double value = text.toDouble(&localOk);
    const bool acceptable = localOk && isAcceptable();
    if (ok) {
        *ok = acceptable;
    }
    return value;
}

void NumericInputField::clearValue()
{
    m_edit->clear();
    updateValidationStyle();
    if (m_changedCallback) {
        m_changedCallback();
    }
}

void NumericInputField::updateValidationStyle()
{
    if (!isFilled()) {
        m_edit->setStyleSheet(QString());
        return;
    }

    if (isAcceptable()) {
        m_edit->setStyleSheet(QString());
    } else {
        m_edit->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #cc0000; }"));
    }
}
