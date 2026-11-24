#ifndef SIDEBARICONS_H
#define SIDEBARICONS_H

#include <QIcon>

/// Left-bar icons drawn in code, in the style of the Courses one: flat zone
/// colours with a dark outline, sharp at any scale.
namespace SidebarIcons {

QIcon workout();    // an interval workout
QIcon stats();      // a rising trend
QIcon devices();    // Bluetooth
QIcon studio();     // three riders
QIcon intervals();  // the Intervals.icu logo, without its wide margins
QIcon settings();   // a gear

} // namespace SidebarIcons

#endif // SIDEBARICONS_H
