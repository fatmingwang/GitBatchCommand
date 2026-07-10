#pragma once
#include <QWidget>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QPushButton;
class LogPanel;

class GeneratePage : public QWidget {
    Q_OBJECT
public:
    explicit GeneratePage(LogPanel *logPanel, QWidget *parent = nullptr);

private slots:
    void onAddName();
    void onRemoveSelected();
    void onClearAll();
    void onGenerate();
    void updatePreview();

private:
    QString normalizedBaseUrl() const;
    QString buildUrl(const QString &name) const;
    QStringList collectNames() const;

    LogPanel *m_logPanel;

    QLineEdit *m_baseUrlEdit;
    QLineEdit *m_namePrefixEdit;
    QLineEdit *m_nameSuffixEdit;
    QLineEdit *m_nameEdit;
    QListWidget *m_nameList;
    QListWidget *m_previewList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_generateBtn;
};
