#pragma once
#include <QWidget>

class QTextEdit;

class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget *parent = nullptr);

public slots:
    void appendInfo(const QString &text);
    void appendWarning(const QString &text);
    void appendError(const QString &text);
    void appendRaw(const QString &text);
    void clear();

private:
    void appendColored(const QString &text, const QString &color);
    QTextEdit *m_edit;
};
