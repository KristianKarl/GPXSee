#include <QFile>
#include <QFileInfo>
#include "config.h"
#include "downloader.h"


#if defined(Q_OS_LINUX)
#define PLATFORM_STR "Linux"
#elif defined(Q_OS_WIN32)
#define PLATFORM_STR "Windows"
#elif defined(Q_OS_MAC)
#define PLATFORM_STR "OS X"
#else
#define PLATFORM_STR "Unknown"
#endif

#define USER_AGENT \
	APP_NAME "/" APP_VERSION " (" PLATFORM_STR "; Qt " QT_VERSION_STR ")"

Downloader::Downloader()
{
	connect(&_manager, SIGNAL(finished(QNetworkReply*)),
			SLOT(downloadFinished(QNetworkReply*)));
}

bool Downloader::doDownload(const Download &dl)
{
	QUrl url(dl.url());

	if (_errorDownloads.contains(url))
		return false;

	QNetworkRequest request(url);
	request.setAttribute(QNetworkRequest::User, QVariant(dl.file()));
	request.setRawHeader("User-Agent", USER_AGENT);
	QNetworkReply *reply = _manager.get(request);

	_currentDownloads.append(reply);

	return true;
}

bool Downloader::saveToDisk(const QString &filename, QIODevice *data)
{
	QFile file(filename);

	if (!file.open(QIODevice::WriteOnly)) {
		fprintf(stderr, "Error writing map tile: %s: %s\n",
		  qPrintable(filename), qPrintable(file.errorString()));
		return false;
	}

	file.write(data->readAll());
	file.close();

	return true;
}

void Downloader::downloadFinished(QNetworkReply *reply)
{
	QUrl url = reply->url();
	if (reply->error()) {
		_errorDownloads.insert(url);
		fprintf(stderr, "Error downloading map tile: %s: %s\n",
		  url.toEncoded().constData(), qPrintable(reply->errorString()));
	} else {
		QUrl redirect = reply->attribute(
		  QNetworkRequest::RedirectionTargetAttribute).toUrl();
		QString filename = reply->request().attribute(QNetworkRequest::User)
		  .toString();

		if (!redirect.isEmpty()) {
			Download dl(redirect, filename);
			doDownload(dl);
		} else
			if (!saveToDisk(filename, reply))
				_errorDownloads.insert(url);
	}

	_currentDownloads.removeAll(reply);
	reply->deleteLater();

	if (_currentDownloads.isEmpty())
		emit finished();
}

bool Downloader::get(const QList<Download> &list)
{
	bool finishEmitted = false;

	for (int i = 0; i < list.count(); i++)
		finishEmitted |= doDownload(list.at(i));

	return finishEmitted;
}
