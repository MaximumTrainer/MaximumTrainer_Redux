#ifndef ACHIEVEMENT_H
#define ACHIEVEMENT_H

#include <QtCore>
#include <QNetworkReply>


/// Used to encapsulate Achievement Name, Description and Icon
/// Check for completion is to be coded elswhere
///
class Achievement
{
public:
    ~Achievement();
    Achievement() {}
    Achievement(int idDB, QString name, QString description, QString iconUrl);



    int getId() const;
    QString getName() const;
    QString getDescription() const;
    QString getIconUrl() const;
    bool isCompleted() const;

    void setCompleted(bool completed);





private:

    int idDB;
    QString name;
    QString description;
    // Icon url to load the pixmap later when we need to show it
    QString iconUrl;
    // true if achievement already completed (to save checks)
    bool completed;


};
Q_DECLARE_METATYPE(Achievement)

#endif // ACHIEVEMENT_H
