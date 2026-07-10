#include "GeneratePage.h"
#include "LogPanel.h"

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

GeneratePage::GeneratePage(LogPanel *logPanel, QWidget *parent)
    : QWidget(parent), m_logPanel(logPanel)
{
    m_baseUrlEdit = new QLineEdit(this);
    m_baseUrlEdit->setPlaceholderText(QStringLiteral("http://gitlab.diresoft.net/client_cocos/"));

    m_namePrefixEdit = new QLineEdit(this);
    m_namePrefixEdit->setPlaceholderText(QStringLiteral("s"));
    m_namePrefixEdit->setText(QStringLiteral("s"));

    m_nameSuffixEdit = new QLineEdit(this);
    m_nameSuffixEdit->setPlaceholderText(QStringLiteral(".git"));
    m_nameSuffixEdit->setText(QStringLiteral(".git"));

    auto *prefixGroup = new QGroupBox(QStringLiteral("Repository URL Pattern"), this);
    auto *prefixForm = new QFormLayout(prefixGroup);
    prefixForm->addRow(QStringLiteral("Prefix Address:"), m_baseUrlEdit);
    prefixForm->addRow(QStringLiteral("Name Prefix:"), m_namePrefixEdit);
    prefixForm->addRow(QStringLiteral("Name Suffix:"), m_nameSuffixEdit);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("10020  or  10022,10025,10027"));
    m_addBtn = new QPushButton(QStringLiteral("Add"), this);
    m_nameList = new QListWidget(this);
    m_removeBtn = new QPushButton(QStringLiteral("Remove Selected"), this);
    m_clearBtn = new QPushButton(QStringLiteral("Clear All"), this);

    auto *addRow = new QHBoxLayout;
    addRow->addWidget(m_nameEdit);
    addRow->addWidget(m_addBtn);

    auto *nameButtonsRow = new QHBoxLayout;
    nameButtonsRow->addWidget(m_removeBtn);
    nameButtonsRow->addWidget(m_clearBtn);
    nameButtonsRow->addStretch();

    auto *nameGroup = new QGroupBox(QStringLiteral("Names"), this);
    auto *nameLayout = new QVBoxLayout(nameGroup);
    nameLayout->addLayout(addRow);
    nameLayout->addWidget(m_nameList);
    nameLayout->addLayout(nameButtonsRow);

    m_previewList = new QListWidget(this);
    m_previewList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewList->setSelectionMode(QAbstractItemView::NoSelection);

    auto *previewGroup = new QGroupBox(QStringLiteral("Preview"), this);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->addWidget(m_previewList);

    m_generateBtn = new QPushButton(QStringLiteral("Generate JSON File..."), this);
    m_generateBtn->setMinimumHeight(32);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(prefixGroup);
    mainLayout->addWidget(nameGroup);
    mainLayout->addWidget(previewGroup);
    mainLayout->addWidget(m_generateBtn);

    connect(m_addBtn, &QPushButton::clicked, this, &GeneratePage::onAddName);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &GeneratePage::onAddName);
    connect(m_removeBtn, &QPushButton::clicked, this, &GeneratePage::onRemoveSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &GeneratePage::onClearAll);
    connect(m_generateBtn, &QPushButton::clicked, this, &GeneratePage::onGenerate);
    connect(m_baseUrlEdit, &QLineEdit::textChanged, this, &GeneratePage::updatePreview);
    connect(m_namePrefixEdit, &QLineEdit::textChanged, this, &GeneratePage::updatePreview);
    connect(m_nameSuffixEdit, &QLineEdit::textChanged, this, &GeneratePage::updatePreview);
}

QString GeneratePage::normalizedBaseUrl() const
{
    QString base = m_baseUrlEdit->text().trimmed();
    if (!base.isEmpty() && !base.endsWith('/'))
        base += QLatin1Char('/');
    return base;
}

QString GeneratePage::buildUrl(const QString &name) const
{
    return normalizedBaseUrl() + m_namePrefixEdit->text().trimmed() + name + m_nameSuffixEdit->text().trimmed();
}

QStringList GeneratePage::collectNames() const
{
    QStringList names;
    for (int i = 0; i < m_nameList->count(); ++i)
        names << m_nameList->item(i)->text();
    return names;
}

void GeneratePage::updatePreview()
{
    m_previewList->clear();
    for (const QString &name : collectNames())
        m_previewList->addItem(buildUrl(name));
}

void GeneratePage::onAddName()
{
    const QString text = m_nameEdit->text().trimmed();
    if (text.isEmpty())
        return;

    const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList existing = collectNames();
    int added = 0;
    int skipped = 0;

    for (const QString &token : tokens) {
        const QString name = token.trimmed();
        if (name.isEmpty())
            continue;
        if (existing.contains(name)) {
            ++skipped;
            continue;
        }
        m_nameList->addItem(name);
        existing << name;
        ++added;
    }

    m_nameEdit->clear();
    updatePreview();

    if (added == 0 && skipped > 0) {
        QMessageBox::information(this, QStringLiteral("Add Name"), QStringLiteral("This name is already in the list."));
    } else if (tokens.size() > 1) {
        QString message = QStringLiteral("Added %1 name(s).").arg(added);
        if (skipped > 0)
            message += QStringLiteral(" Skipped %1 duplicate(s).").arg(skipped);
        m_logPanel->appendInfo(message);
    }
}

void GeneratePage::onRemoveSelected()
{
    for (QListWidgetItem *item : m_nameList->selectedItems())
        delete m_nameList->takeItem(m_nameList->row(item));
    updatePreview();
}

void GeneratePage::onClearAll()
{
    m_nameList->clear();
    updatePreview();
}

void GeneratePage::onGenerate()
{
    const QStringList names = collectNames();
    if (names.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Generate JSON"), QStringLiteral("Add at least one name first."));
        return;
    }
    if (normalizedBaseUrl().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Generate JSON"), QStringLiteral("Enter the prefix address first."));
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Repository List"), QStringLiteral("repos.json"), QStringLiteral("JSON Files (*.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");

    QJsonArray array;
    for (const QString &name : names)
        array.append(buildUrl(name));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Generate JSON"), QStringLiteral("Could not write file: ") + path);
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    file.close();

    m_logPanel->appendInfo(QStringLiteral("Generated %1 repository URL(s) to %2").arg(names.size()).arg(path));
    QMessageBox::information(this, QStringLiteral("Generate JSON"),
                              QStringLiteral("Wrote %1 repository URL(s) to:\n%2").arg(names.size()).arg(path));
}
