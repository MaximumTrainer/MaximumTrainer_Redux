#include "achievement.h"

#include <QLabel>




Achievement::~Achievement() {
}



Achievement::Achievement(int idDB, QString name, QString description, QString iconUrl) {

    this->idDB = idDB;
    this->name = std::move(name);
    this->description = std::move(description);
    this->iconUrl = std::move(iconUrl);
    this->completed = false;

}


int Achievement::getId() const {
    return this->idDB;
}

QString Achievement::getName() const {
    return this->name;
}
QString Achievement::getDescription() const {
    return this->description;
}
QString Achievement::getIconUrl() const {
    return this->iconUrl;
}
bool Achievement::isCompleted() const {
    return this->completed;
}

void Achievement::setCompleted(bool completed) {
    this->completed = completed;
}
