#ifndef IPCHTTP_H
#define IPCHTTP_H

#include <functional>
#include <QDateTime>
#include <QMap>
#include <QTcpServer>

class QAction;

class HttpRequest {
public:
    QString method;
    QString url;
    QMap<QString,QString> headers;
    QMap<QString,QString> getVars;
    QMap<QString,QString> postVars;
};

class HttpResponse {
public:
    HttpResponse();

    enum HttpStatus {
        Http200Ok = 200,
        Http204NoContent = 204,
        Http301MovedPermanently = 301,
        Http302Found = 302,
        Http303SeeOther = 303,
        Http400BadRequest = 400,
        Http401Unauthorized = 401,
        Http403Forbidden = 403,
        Http404NotFound = 404,
        Http500InternalServerError = 500,
        Http501NotImplemented = 501,
    };

    HttpStatus statusCode;
    QDateTime dateResponse;
    QDateTime dateModified;
    QString contentType;
    QByteArray content;

    QString statusLine;
    QMap<QString,QString> headers;

    bool fallthrough;

    void fillHeaders();
    void redirect(QString where);
    void serveFile(QString filename);
};


class HttpServer : public QTcpServer
{
    Q_OBJECT
public:
    HttpServer(QObject *owner = nullptr);
    void clearRoutes();
    void route(QString path, std::function<void(HttpRequest&, HttpResponse&)> callback);
    static QString extensionToContentType(QString extension);
    static QString urlDecode(QString urlText, bool plusToSpace = false);
    static QString urlEncode(QString plainText);

private:
    void sendSocket(QTcpSocket *sock, HttpResponse &res);

private slots:
    void self_newConnection();
    void socket_readyRead(QTcpSocket *sock);

private:
    QList<QPair<QString, std::function<void(HttpRequest&, HttpResponse&)>>> routeMap;
};


class MpcHcServer : public QObject
{
    Q_OBJECT

    class WmCommand {
    public:
        int id;
        const char *text;
        std::function<void()> func;
    };

public:
    explicit MpcHcServer(QObject *owner = nullptr);

public slots:
    void setDefaultPage(QString file);
    void setEnabled(bool yes);
    void setLocalHostOnly(bool yes);
    void setTcpPort(uint16_t port);
    void setServeFiles(bool yes);
    void setWebRoot(QString path);

private:
    void relisten();
    void setupWmCommands();
    void setupHttp();

    HttpServer http;
    QList<WmCommand> wmCommands;
    QMap<int, WmCommand> wmCommandsById;

    QString defaultPage = "index.html";
    bool enabled = false;
    bool localHostOnly = true;
    uint16_t tcpPort = 13579;
    bool serveFiles = false;
    QString webRoot;
};


#endif // IPCHTTP_H
