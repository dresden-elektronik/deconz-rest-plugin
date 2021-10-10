#include "text_lineedit.h"

TextLineEdit::TextLineEdit(QWidget *parent) :
    QLineEdit(parent)
{
    connect(this, &QLineEdit::textChanged, this, &TextLineEdit::inputTextChanged);
}

void TextLineEdit::setInputText(const QString &text)
{
    m_origValue = text;
    setText(text);
}

void TextLineEdit::inputTextChanged(const QString &text)
{
    if (verifyInputText(text))
    {
    }
    emit valueChanged();
}

bool TextLineEdit::verifyInputText(const QString &text)
{
    bool isValid = true;

    if (!m_isOptional && text.isEmpty())
    {
        isValid = false;
    }

    if (!isValid)
    {
        setStyleSheet(QLatin1String("background-color: yellow"));
    }
    else if (text != m_origValue)
    {
        setStyleSheet(QLatin1String("color:blue"));
    }
    else
    {
        setStyleSheet(QString());
    }

    return isValid;
}

