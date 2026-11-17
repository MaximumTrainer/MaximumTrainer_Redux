#include "radio.h"



Radio::Radio(QString name, QString genre, bool gotAds, int bitrate, QString language, QString url) {

    this->name = std::move(name);
    this->genre = std::move(genre);
    this->gotAds = gotAds;
    this->bitrate = bitrate;
    this->language = std::move(language);
    this->url = std::move(url);
}
