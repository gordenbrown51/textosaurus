// For license of this file, see <project-root-folder>/LICENSE.md.

#include "miscellaneous/textapplication.h"

#include "gui/dialogs/formmain.h"
#include "gui/messagebox.h"
#include "gui/statusbar.h"
#include "gui/tabwidget.h"
#include "gui/texteditor.h"
#include "gui/toolbar.h"
#include "gui/toolbox.h"
#include "miscellaneous/textfactory.h"

#include "uchardet/uchardet.h"

#include <QFileDialog>
#include <QTextCodec>

TextApplication::TextApplication(QObject* parent) : QObject(parent) {
  connect(qApp, &Application::dataSaveRequested, this, &TextApplication::quit);
}

TextEditor* TextApplication::currentEditor() const {
  return m_tabEditors->textEditorAt(m_tabEditors->currentIndex());
}

QList<TextEditor*> TextApplication::editors() const {
  QList<TextEditor*> editors;

  if (m_tabEditors != nullptr) {
    for (int i = 0; i < m_tabEditors->count(); i++) {
      TextEditor* edit = m_tabEditors->textEditorAt(i);

      if (edit != nullptr) {
        editors.append(edit);
      }
    }
  }

  return editors;
}

bool TextApplication::anyModifiedEditor() const {
  foreach (const TextEditor* editor, editors()) {
    if (editor->isModified()) {
      return true;
    }
  }

  return false;
}

void TextApplication::loadTextEditorFromFile(const QString& file_path, const QString& explicit_encoding) {
  QString encoding;

  if (explicit_encoding.isEmpty()) {
    qDebug("No explicit encoding for file '%s' openin. Try to detect one.", qPrintable(file_path));

    if ((encoding = TextFactory::detectEncoding(file_path)).isEmpty()) {
      // No encoding auto-detected.
      encoding = QSL(DEFAULT_TEXT_FILE_ENCODING);
      qWarning("Auto-detection of encoding failed, using default encoding.");
    }
    else {
      qDebug("Auto-detected encoding is '%s'.", qPrintable(encoding));
    }
  }
  else {
    encoding = explicit_encoding;
  }

  QFile file(file_path);

  if (!file.exists()) {
    QMessageBox::critical(qApp->mainFormWidget(), tr("Cannot open file"),
                          tr("File '%1' does not exist and cannot be opened.").arg(QDir::toNativeSeparators(file_path)));
    return;
  }

  if (file.size() >= MAX_TEXT_FILE_SIZE) {
    QMessageBox::critical(qApp->mainFormWidget(), tr("Cannot open file"),
                          tr("File '%1' too big. %2 can only open files smaller than %3 MB.").arg(QDir::toNativeSeparators(file_path),
                                                                                                  QSL(APP_NAME),
                                                                                                  QString::number(MAX_TEXT_FILE_SIZE /
                                                                                                                  1000000.0)));
    return;
  }

  if (!file.open(QIODevice::OpenModeFlag::ReadOnly)) {
    QMessageBox::critical(qApp->mainFormWidget(), tr("Cannot read file"),
                          tr("File '%1' cannot be opened for reading. Insufficient permissions.").arg(QDir::toNativeSeparators(file_path)));
    return;
  }

  if (file.size() > BIG_TEXT_FILE_SIZE) {
    if (encoding != QSL(DEFAULT_TEXT_FILE_ENCODING) &&
        MessageBox::show(qApp->mainFormWidget(), QMessageBox::Question, tr("Opening big file"),
                         tr("You want to open big text file in encoding which is different from %1. This operation "
                            "might take quite some time.").arg(QSL(DEFAULT_TEXT_FILE_ENCODING)),
                         tr("Do you really want to open the file?"),
                         file.fileName(), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No) {
      return;
    }
    else {
      // File is quite big, we turn some features off to make sure it loads faster.
      QMessageBox::warning(qApp->mainFormWidget(), tr("Loading big file"),
                           tr("File '%1' is big. %2 will switch some features (for example 'Word wrap') off to "
                              "make sure that file loading is not horribly slow.").arg(QDir::toNativeSeparators(file_path),
                                                                                       QSL(APP_NAME)));
      m_actionWordWrap->setChecked(false);
    }
  }

  TextEditor* new_editor = addEmptyTextEditor();

  if (new_editor != nullptr) {
    new_editor->loadFromFile(file, encoding);
    m_settings.setLoadSaveDefaultDirectory(file_path);
    m_tabEditors->setCurrentWidget(new_editor);
  }
}

TextEditor* TextApplication::addEmptyTextEditor() {
  TextEditor* editor = new TextEditor(this, m_tabEditors);

  m_tabEditors->addTab(editor, qApp->icons()->fromTheme(QSL("text-plain")), tr("New text file"), TabBar::TabType::TextEditor);
  connect(editor, &TextEditor::modificationChanged, this, &TextApplication::onEditorTextChanged);

  connect(editor, &TextEditor::loadedFromFile, this, &TextApplication::onEditorLoadedFromFile);
  connect(editor, &TextEditor::savedToFile, this, &TextApplication::onEditorSavedToFile);
  connect(editor, &TextEditor::requestVisibility, this, &TextApplication::onEditorRequestVisibility);

  return editor;
}

void TextApplication::saveCurrentEditor() {
  TextEditor* editor = currentEditor();

  if (editor != nullptr) {
    bool ok;

    editor->save(&ok);
  }
}

void TextApplication::saveCurrentEditorAs() {
  TextEditor* editor = currentEditor();

  if (editor != nullptr) {
    bool ok;

    editor->saveAs(&ok);
  }
}

void TextApplication::saveCurrentEditorAsWithEncoding(QAction* action) {
  TextEditor* editor = currentEditor();

  if (editor != nullptr) {
    bool ok;
    QString encoding = action->data().toString();

    editor->saveAs(&ok, encoding);
  }
}

void TextApplication::saveAllEditors() {
  bool ok;

  foreach (TextEditor* editor, editors()) {
    editor->save(&ok);
  }
}

void TextApplication::closeAllUnmodifiedEditors() {
  foreach (TextEditor* editor, editors()) {
    if (!editor->isModified()) {
      m_tabEditors->closeTab(m_tabEditors->indexOf(editor));
    }
  }
}

void TextApplication::reloadEditorsAfterSettingsChanged(bool reload_visible, bool reload_all) {
  if (reload_all) {
    foreach (TextEditor* editor, editors()) {
      editor->reloadSettings();
    }
  }
  else if (reload_visible) {
    TextEditor* visible = currentEditor();

    if (visible != nullptr) {
      visible->reloadSettings();
    }
  }
}

void TextApplication::onEditorRequestVisibility() {
  TextEditor* editor = qobject_cast<TextEditor*>(sender());

  if (editor != nullptr) {
    m_tabEditors->setCurrentWidget(editor);
  }
}

void TextApplication::onEditorSavedToFile() {
  TextEditor* editor = qobject_cast<TextEditor*>(sender());

  renameEditor(editor);
  updateToolBarFromEditor(editor, true);
  updateStatusBarFromEditor(editor);
}

void TextApplication::onEditorLoadedFromFile() {
  TextEditor* editor = qobject_cast<TextEditor*>(sender());

  renameEditor(editor);
  updateToolBarFromEditor(editor, true);
  updateStatusBarFromEditor(editor);
}

void TextApplication::markEditorModified(TextEditor* editor, bool modified) {
  int index = m_tabEditors->indexOf(editor);

  if (index >= 0) {
    m_tabEditors->tabBar()->setTabIcon(index, modified ?
                                       qApp->icons()->fromTheme(QSL("dialog-warning")) :
                                       qApp->icons()->fromTheme(QSL("text-plain")));

    updateToolBarFromEditor(editor, true);
  }
}

TextApplicationSettings& TextApplication::settings() {
  return m_settings;
}

void TextApplication::loadFilesFromArgs(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    QString file_path = QString::fromUtf8(argv[i]);

    QTimer::singleShot(100, this, [this, file_path] {
      loadTextEditorFromFile(file_path);
    });
  }
}

void TextApplication::undo() {
  TextEditor* editor = currentEditor();

  if (editor != nullptr) {
    editor->undo();
  }
}

void TextApplication::redo() {
  TextEditor* editor = currentEditor();

  if (editor != nullptr) {
    editor->redo();
  }
}

void TextApplication::newFile() {
  TextEditor* editor = addEmptyTextEditor();

  m_tabEditors->setCurrentWidget(editor);
}

void TextApplication::onEditorTextChanged(bool modified) {
  TextEditor* editor = qobject_cast<TextEditor*>(sender());

  markEditorModified(editor, modified);
}

void TextApplication::createConnections() {
  connect(&m_settings, &TextApplicationSettings::settingsChanged, this, &TextApplication::reloadEditorsAfterSettingsChanged);

  // Tab widget.
  connect(m_tabEditors, &TabWidget::currentChanged, this, &TextApplication::onEditorTabSwitched);
  connect(m_tabEditors->tabBar(), &TabBar::emptySpaceDoubleClicked, this, &TextApplication::newFile);

  // Actions.
  connect(m_menuEolMode, &QMenu::triggered, &m_settings, &TextApplicationSettings::setEolModeFromAction);
  connect(m_actionTabsCloseAllUnmodified, &QAction::triggered, this, &TextApplication::closeAllUnmodifiedEditors);
  connect(m_actionFileSave, &QAction::triggered, this, &TextApplication::saveCurrentEditor);
  connect(m_actionFileSaveAs, &QAction::triggered, this, &TextApplication::saveCurrentEditorAs);
  connect(m_actionFileSaveAll, &QAction::triggered, this, &TextApplication::saveAllEditors);
  connect(m_actionFileNew, &QAction::triggered, this, &TextApplication::newFile);
  connect(m_actionFileOpen, &QAction::triggered, this, [this]() {
    openTextFile();
  });
  connect(m_actionWordWrap, &QAction::toggled, &m_settings, &TextApplicationSettings::setWordWrapEnabled);
  connect(m_actionEditBack, &QAction::triggered, this, &TextApplication::undo);
  connect(m_actionEditForward, &QAction::triggered, this, &TextApplication::redo);

  // Menus.
  connect(m_menuFileOpenWithEncoding, &QMenu::aboutToShow, this, [this]() {
    if (m_menuFileOpenWithEncoding->isEmpty()) {
      TextFactory::initializeEncodingMenu(m_menuFileOpenWithEncoding);
    }
  });
  connect(m_menuFileOpenWithEncoding, &QMenu::triggered, this, &TextApplication::openTextFile);

  connect(m_menuFileSaveWithEncoding, &QMenu::aboutToShow, this, [this]() {
    if (m_menuFileSaveWithEncoding->isEmpty()) {
      TextFactory::initializeEncodingMenu(m_menuFileSaveWithEncoding);;
    }
  });
  connect(m_menuFileSaveWithEncoding, &QMenu::triggered, this, &TextApplication::saveCurrentEditorAs);
}

void TextApplication::setMainForm(FormMain* main_form, TabWidget* tab_widget,
                                  StatusBar* status_bar, ToolBox* tool_box) {
  m_tabEditors = tab_widget;
  m_statusBar = status_bar;
  m_toolBox = tool_box;

  // Get pointers to editor-related global actions/menus.
  m_actionFileNew = main_form->m_ui.m_actionFileNew;
  m_actionFileOpen = main_form->m_ui.m_actionFileOpen;
  m_actionFileSave = main_form->m_ui.m_actionFileSave;
  m_actionFileSaveAs = main_form->m_ui.m_actionFileSaveAs;
  m_actionFileSaveAll = main_form->m_ui.m_actionFileSaveAll;
  m_actionEolUnix = main_form->m_ui.m_actionEolUnix;
  m_actionEolWindows = main_form->m_ui.m_actionEolWindows;
  m_actionEolMac = main_form->m_ui.m_actionEolMac;
  m_actionWordWrap = main_form->m_ui.m_actionWordWrap;
  m_actionTabsCloseAllUnmodified = main_form->m_ui.m_actionTabsCloseAllUnmodified;
  m_actionEditBack = main_form->m_ui.m_actionEditBack;
  m_actionEditForward = main_form->m_ui.m_actionEditForward;

  m_menuFileSaveWithEncoding = main_form->m_ui.m_menuFileSaveWithEncoding;
  m_menuFileOpenWithEncoding = main_form->m_ui.m_menuFileOpenWithEncoding;
  m_menuEolMode = main_form->m_ui.m_menuEolMode;

  m_actionEolMac->setData(int(QsciScintilla::EolMode::EolMac));
  m_actionEolUnix->setData(int(QsciScintilla::EolMode::EolUnix));
  m_actionEolWindows->setData(int(QsciScintilla::EolMode::EolWindows));

  connect(main_form, &FormMain::closeRequested, this, &TextApplication::quit);

  createConnections();
  loadState();
}

void TextApplication::loadState() {
  m_actionEditBack->setEnabled(false);
  m_actionEditForward->setEnabled(false);
  m_actionWordWrap->setChecked(m_settings.wordWrapEnabled());

  // Setup GUI of actions.
  switch (m_settings.eolMode()) {
    case QsciScintilla::EolMode::EolMac:
      m_actionEolMac->setChecked(true);
      break;

    case QsciScintilla::EolMode::EolWindows:
      m_actionEolWindows->setChecked(true);
      break;

    case QsciScintilla::EolMode::EolUnix:
    default:
      m_actionEolUnix->setChecked(true);
      break;
  }

  m_toolBox->displayOutput(OutputSource::TextApplication, tr("Text component settings loaded."));

  // Make sure that toolbar/statusbar is updated.
  onEditorTabSwitched();
}

void TextApplication::quit(bool* ok) {
  foreach (TextEditor* edit, editors()) {
    bool editor_ok;

    edit->closeEditor(&editor_ok);

    if (!editor_ok) {
      *ok = false;
      return;
    }
  }

  *ok = true;
}

void TextApplication::openTextFile(QAction* action) {
  QString encoding = (action != nullptr && !action->data().isNull()) ? action->data().toString() : QString();
  QString file_path = QFileDialog::getOpenFileName(qApp->mainFormWidget(), tr("Open file"),
                                                   m_settings.loadSaveDefaultDirectory(),
                                                   tr("Text files (*.txt);;All files (*)"));

  if (!file_path.isEmpty()) {
    loadTextEditorFromFile(file_path, encoding);
  }
}

void TextApplication::onEditorTabSwitched(int index) {
  TextEditor* editor = m_tabEditors->textEditorAt(index);

  if (editor != nullptr) {
    editor->reloadSettings();
  }

  updateToolBarFromEditor(editor, false);
  updateStatusBarFromEditor(editor);
}

void TextApplication::updateToolBarFromEditor(TextEditor* editor, bool only_modified) {
  Q_UNUSED(only_modified)

  if (editor == currentEditor()) {
    // Current editor is changed or there is now editor at all.

    if (editor != nullptr) {
      // Current editor is changed.
      m_actionFileSave->setEnabled(editor->isModified());
      m_actionFileSaveAs->setEnabled(true);
      m_menuFileSaveWithEncoding->setEnabled(true);
      m_actionEditBack->setEnabled(editor->isUndoAvailable());
      m_actionEditForward->setEnabled(editor->isRedoAvailable());
    }
    else {
      // No editor selected.
      m_actionFileSave->setEnabled(false);
      m_actionFileSaveAs->setEnabled(false);
      m_menuFileSaveWithEncoding->setEnabled(false);
      m_actionEditBack->setEnabled(false);
      m_actionEditForward->setEnabled(false);
    }
  }

  // Enable this if there is at least one unsaved editor.
  m_actionFileSaveAll->setEnabled(anyModifiedEditor());
}

void TextApplication::updateStatusBarFromEditor(TextEditor* editor) {
  if (editor == currentEditor()) {
    if (editor != nullptr) {
      m_statusBar->setEncoding(editor->encoding());
    }
    else {
      m_statusBar->setEncoding(QString());
    }
  }
}

void TextApplication::renameEditor(TextEditor* editor) {
  int index = m_tabEditors->indexOf(editor);

  if (index >= 0) {
    if (!editor->filePath().isEmpty()) {
      m_tabEditors->tabBar()->setTabText(index, QFileInfo(editor->filePath()).fileName());
    }
  }
}
