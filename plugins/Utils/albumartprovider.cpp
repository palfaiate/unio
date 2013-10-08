/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Jussi Pakkanen <jussi.pakkanen@canonical.com>
 *          Pawel Stolowski <pawel.stolowski@canonical.com>
*/

#include "albumartprovider.h"
#include <QString>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlQuery>
#include <QBuffer>
#include <cstdio>
#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QByteArray>
#include <unistd.h>
#include <memory>
#include <QImage>
#include <QUrl>

using namespace std;

const std::string AlbumArtProvider::DEFAULT_ALBUM_ART = "/usr/share/unity/icons/album_missing.png";

std::string AlbumArtProvider::get_lastfm_url(const albuminfo &ai) {
    QString artist = QString::fromStdString(ai.artist);
    QString album = QString::fromStdString(ai.album);

    /// @todo: this is the old API which will probably get axed at some point in the future
    ///        The new 2.0 API requires an API key, but supports JSON output, etc, so switching
    ///        to it should be done ASAP.
    QString request = QString("http://ws.audioscrobbler.com/1.0/album/%1/%2/info.xml").arg(artist)
                                                                                      .arg(album)
                                                                                      .toHtmlEscaped();
    QScopedPointer<QNetworkAccessManager> am(new QNetworkAccessManager);
    QNetworkReply *reply = am->get(QNetworkRequest(QUrl(request)));
    QEventLoop loop;
    QObject::connect(am.data(), &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error getting the XML:" << reply->errorString();
        return "";
    }

    QXmlQuery query;
    QBuffer tmp;
    tmp.setData(reply->readAll());
    tmp.open(QIODevice::ReadOnly);
    query.bindVariable("reply", &tmp);
    query.setQuery("doc($reply)/album/coverart/large[1]/text()");
    QString image;
    query.evaluateTo(&image);
    image = image.trimmed();

    return image.toStdString();
}

bool AlbumArtProvider::download_and_store(const std::string &image_url, const std::string &output_file) {
    QString url = QString::fromStdString(image_url);
    QString fileName = QString::fromStdString(output_file);

    QScopedPointer<QNetworkAccessManager> am(new QNetworkAccessManager);
    QNetworkReply *reply = am->get(QNetworkRequest(QUrl(url)));
    QEventLoop loop;
    QObject::connect(am.data(), &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error getting the image:" << reply->errorString();
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "Could not open file for writing:" << file.errorString();
        return false;
    }
    if (file.write(reply->readAll()) == -1){
        qDebug() << "Could not write the image:" << file.error();
        return false;
    }

    return true;
}

void AlbumArtProvider::fix_format(const std::string &fname) {
    // MediaArtSpec requires jpg. Convert to it if necessary.
    FILE *f = fopen(fname.c_str(), "r");
    if(!f)
        return;
    unsigned char buf[2];
    fread(buf, 1, 2, f);
    fclose(f);
    if(buf[0] == 0xff && buf[1] == 0xd8) {
        return;
    }
    QImage im(fname.c_str());
    im.save(fname.c_str(), "JPEG");
}

std::string AlbumArtProvider::get_image(const std::string &artist, const std::string &album) {
    albuminfo info;

    info.artist = artist;
    info.album = album;

    if(info.album.empty() || info.artist.empty()) {
        return DEFAULT_ALBUM_ART;
    }
    if(cache.has_art(info.artist, info.album)) {
        // Image may have expired from cache between these two lines.
        // It might expire before we return. C'est la vie.
        return cache.get_art_file(info.artist, info.album);
    }
    std::string image_url = get_lastfm_url(info);
    if(image_url.empty()) {
        return DEFAULT_ALBUM_ART;
    }
    char tmpname[] = "/tmp/path/to/some/file/somewhere/maybe.jpg";
    tmpnam(tmpname);
    //std::unique_ptr<char, int(*)(const char*)> deleter(tmpname, unlink);
    if(!download_and_store(image_url, tmpname)) {
        return "";
    }
    fix_format(tmpname);
    FILE *f = fopen(tmpname, "r");
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = new char[s];
    fread(buf, 1, s, f);
    fclose(f);

    // Fixme, leaks buf if throws.
    cache.add_art(info.artist, info.album, buf, s);
    delete []buf;
    return cache.get_art_file(info.artist, info.album);
}

QImage AlbumArtProvider::requestImage(const QString &id, QSize *realSize, const QSize &requestedSize) {
    Q_UNUSED(requestedSize)

    const QStringList parts = id.split("/");
    if (parts.size() != 2) {
        qWarning("Invalid albumart uri");
        return QImage(QString::fromStdString(DEFAULT_ALBUM_ART));
    }
    const std::string artist = QUrl::fromPercentEncoding(parts[0].toUtf8()).toStdString();
    const std::string album = QUrl::fromPercentEncoding(parts[1].toUtf8()).toStdString();
    
    std::string tgt_path;
    try {
        tgt_path = get_image(artist, album);
        if(!tgt_path.empty()) {
            QString tgt(tgt_path.c_str());
            QImage image;
            image.load(tgt);
            // FIXME: Rescale to requested size preserving aspect.
            *realSize = image.size();
            return image;
        }
    } catch(std::exception &e) {
        qDebug() << "Album art loader failed: " << e.what();
    } catch(...) {
        qDebug() << "Unknown error when generating image.";
    }

    *realSize = QSize(0, 0);
    return QImage(QString::fromStdString(DEFAULT_ALBUM_ART));
}
