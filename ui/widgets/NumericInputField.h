#ifndef NUMERICINPUTFIELD_H
#define NUMERICINPUTFIELD_H

#include <QWidget>
#include <functional>

class QLineEdit;
class QPushButton;
class QDoubleValidator;

class NumericInputField : public QWidget
{
    Q_OBJECT

public:
    explicit NumericInputField(double minimum,
                               double maximum,
                               int decimals,
                               QWidget* parent = nullptr);

    void setChangedCallback(const std::function<void()>& callback);

    bool isFilled() const;
    bool isAcceptable() const;
    double toDouble(bool* ok = nullptr) const;
    void clearValue();

private:
    void updateValidationStyle();

    QLineEdit* m_edit;
    QPushButton* m_clearButton;
    QDoubleValidator* m_validator;
    std::function<void()> m_changedCallback;
};

#endif // NUMERICINPUTFIELD_H
