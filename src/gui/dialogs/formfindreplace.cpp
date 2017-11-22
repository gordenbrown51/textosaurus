// For license of this file, see <project-root-folder>/LICENSE.md.

#include "gui/dialogs/formfindreplace.h"

#include "gui/texteditor.h"
#include "miscellaneous/textapplication.h"

FormFindReplace::FormFindReplace(TextApplication* app, QWidget* parent) : QDialog(parent), m_application(app) {
  m_ui.setupUi(this);

  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
  setWindowOpacity(0.8);
}

FormFindReplace::~FormFindReplace() {}

void FormFindReplace::display() {
  show();
  activateWindow();
  raise();
}
