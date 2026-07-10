#include "LogPanel.h"

#include <QTextEdit>
#include <QVBoxLayout>
#include <QDateTime>
#include <QScrollBar>
#include <QFont>

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
{
    m_edit = new QTextEdit(this);
    m_edit->setReadOnly(true);
    m_edit->setLineWrapMode(QTextEdit::NoWrap);
    m_edit->setStyleSheet(QStringLiteral("QTextEdit { background-color:#1e1e1e; }"));

    QFont font(QStringLiteral("Consolas"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(9);
    m_edit->setFont(font);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_edit);
}

void LogPanel::appendColored(const QString &text, const QString &color)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString escaped = text.toHtmlEscaped();
    m_edit->append(QStringLiteral("<span style=\"color:#808080;\">[%1]</span> <span style=\"color:%2;\">%3</span>")
                       .arg(timestamp, color, escaped));
    QScrollBar *bar = m_edit->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void LogPanel::appendInfo(const QString &text) { appendColored(text, QStringLiteral("#4ec9b0")); }
void LogPanel::appendWarning(const QString &text) { appendColored(text, QStringLiteral("#dcb67a")); }
void LogPanel::appendError(const QString &text) { appendColored(text, QStringLiteral("#f14c4c")); }
void LogPanel::appendRaw(const QString &text) { appendColored(text, QStringLiteral("#d4d4d4")); }
void LogPanel::clear() { m_edit->clear(); }
