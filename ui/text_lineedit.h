#ifndef TEXTLINEEDIT_H
#define TEXTLINEEDIT_H

#include <QLineEdit>

class TextLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    TextLineEdit(QWidget *parent);
    void setIsOptional(bool optional) { m_isOptional = optional; }
    void setInputText(const QString &text);

private Q_SLOTS:
    void inputTextChanged(const QString &text);
    bool verifyInputText(const QString &text);

Q_SIGNALS:
    void valueChanged();

private:
    bool m_isOptional = false;
    QString m_origValue;
};

#endif // TEXTLINEEDIT_H
