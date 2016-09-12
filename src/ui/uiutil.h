//******************************************************************************
//******************************************************************************

#ifndef UIUTIL_H
#define UIUTIL_H

#include <QWidget>

//******************************************************************************
//******************************************************************************
namespace gui
{

void saveWindowPos(const char * name, const QWidget & window);
void restoreWindowPos(const char * name, QWidget & window);

} // namespace gui

#endif // UIUTIL_H
