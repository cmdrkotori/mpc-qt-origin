#include <QAction>
#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>
#include "ipc/http.h"

static const char httpVersion[] = "HTTP/1.1";
static const char errorDocument[] = "<html><head><title>%1</title></head>"
                                    "<body><h1>%1</h1></body></html>";
static const char mimeFormUrlEncoded[] = "application/x-www-form-urlencoded";
static const char mimeHtml[] = "text/html";

static QMap<HttpResponse::HttpStatus,QString> statusToText = {
    { HttpResponse::Http200Ok, "200 OK" },
    { HttpResponse::Http204NoContent, "204 No Content" },
    { HttpResponse::Http301MovedPermanently, "301 Moved Permanently" },
    { HttpResponse::Http302Found, "302 Found" },
    { HttpResponse::Http303SeeOther, "303 See Other" },
    { HttpResponse::Http400BadRequest, "400 Bad Request" },
    { HttpResponse::Http401Unauthorized, "401 Unauthorized" },
    { HttpResponse::Http403Forbidden, "403 Forbidden" },
    { HttpResponse::Http404NotFound, "404 Not Found" },
    { HttpResponse::Http500InternalServerError, "500 Internal Server Error" },
    { HttpResponse::Http501NotImplemented, "501 Not Implemented" }
};

static QMap<QString,QString> extensionToText {
    { "aac", "audio/aac" },
    { "avi", "video/x-msvideo" },
    { "bin", "application/octet-stream" },
    { "bmp", "image/bmp" },
    { "css", "text/css" },
    { "eot", "application/vnd-ms-fontobject" },
    { "gif", "image/gif" },
    { "htm", "text/html" },
    { "html", "text/html" },
    { "ico", "image/vnd.microsoft.icon" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "js", "text/javascript" },
    { "json", "application/json" },
    { "mp3", "audio/mpeg" },
    { "mpeg", "video/mpeg" },
    { "oga", "audio/ogg" },
    { "ogg", "audio/ogg" },
    { "ogv", "video/ogg" },
    { "otf", "font/otf" },
    { "pdf", "application/pdf" },
    { "png", "image/png" },
    { "svg", "image/svg+xml" },
    { "tif", "image/tiff" },
    { "tiff", "image/tiff" },
    { "ttf", "font/ttf" },
    { "txt", "text/plain" },
    { "wav", "audio/wav" },
    { "weba", "audio/webm" },
    { "webm", "video/webm" },
    { "webp", "image/webp" },
    { "woff", "font/woff" },
    { "woff2", "font/woff2" },
    { "xhtml", "application/xhtml+xml" },
    { "xml", "application/xml" }
};

HttpResponse::HttpResponse()
{
    statusCode = HttpResponse::Http200Ok;
    dateResponse = dateModified = QDateTime::currentDateTimeUtc();
    contentType = mimeHtml;
    fallthrough = false;
}

void HttpResponse::fillHeaders()
{
    static const char dateFmt[] = "ddd, dd MMM yyyy HH:mm:ss t";
    headers["Date"] = dateResponse.toString(dateFmt);
    if (!headers.contains("Server"))
        headers["Server"] = QCoreApplication::applicationName();
    headers["Last-Modified"] = dateModified.toString(dateFmt);
    //headers["Accept-Ranges"] = "bytes";
    headers["Content-Length"] = QString::number(content.size());
    headers["Content-Type"] = contentType;
}

void HttpResponse::redirect(QString where) {
    statusCode = HttpResponse::Http302Found;
    headers["Location"] = where;
}

void HttpResponse::serveFile(QString filename)
{
    filename.replace("..", ".");

    QFile f(filename);
    if (!f.exists()) {
        statusCode = HttpResponse::Http404NotFound;
        return;
    }
    f.open(QIODevice::ReadOnly);

    auto extension = QFileInfo(filename).suffix();
    contentType = HttpServer::extensionToContentType(extension);
    content = f.readAll();
}

HttpServer::HttpServer(QObject *owner) : QTcpServer(owner)
{
    connect(this, &HttpServer::newConnection,
            this, &HttpServer::self_newConnection);
}

void HttpServer::clearRoutes()
{
    routeMap.clear();
}

void HttpServer::route(QString path, std::function<void(HttpRequest&, HttpResponse&)> callback)
{
    routeMap.append({path, callback});
}

QString HttpServer::extensionToContentType(QString extension)
{
    return extensionToText.value(extension, extensionToText["bin"]);
}

QString HttpServer::urlDecode(QString urlText, bool plusToSpace)
{
    typedef unsigned char byte;
    static QByteArray chToHex = [](){
        QByteArray map;
        map.resize(256);
        for (int i = 0; i < 256; i++)
            map[i] = 0;
        map[(byte)'0'] = 0;
        map[(byte)'1'] = 1;
        map[(byte)'2'] = 2;
        map[(byte)'3'] = 3;
        map[(byte)'4'] = 4;
        map[(byte)'5'] = 5;
        map[(byte)'6'] = 6;
        map[(byte)'7'] = 7;
        map[(byte)'8'] = 8;
        map[(byte)'9'] = 9;
        map[(byte)'a'] = 10;
        map[(byte)'A'] = 10;
        map[(byte)'b'] = 11;
        map[(byte)'B'] = 11;
        map[(byte)'c'] = 12;
        map[(byte)'C'] = 12;
        map[(byte)'d'] = 13;
        map[(byte)'D'] = 13;
        map[(byte)'e'] = 14;
        map[(byte)'E'] = 14;
        map[(byte)'f'] = 15;
        map[(byte)'F'] = 15;
        return map;
    }();
    QByteArray ba = urlText.toUtf8();
    QByteArray dest;
    int length = ba.length();
    byte c;
    const byte *data = reinterpret_cast<const byte*>(ba.constData());
    for (int i = 0; i < length; i++) {
        if (data[i] != '%') {
            if (plusToSpace && data[i] == '+')
                dest.append(' ');
            else
                dest.append(data[i]);
            continue;
        }
        c = 0;
        if (i < length-1) {
            i++;
            c = chToHex[data[i]];
        }
        if (i < length-1) {
            i++;
            c *= 16;
            c += chToHex[data[i]];
            if (c)
                dest.append(c);
        }
    }
    return QString::fromUtf8(dest);
}

QString HttpServer::urlEncode(QString plainText)
{
    typedef unsigned char byte;
    static const QByteArray isEncoded = [] {
        QByteArray map;
        map.resize(256);
        for (int i = 0; i < 32; i++)
            map[i] = 1;
        for (int i = 32; i < 128; i++)
            map[i] = 0;
        for (int i = 128; i < 256; i++)
            map[i] = 1;
        map[(byte)' '] = 1;
        map[(byte)'!'] = 1;
        map[(byte)'*'] = 1;
        map[(byte)'\''] = 1;
        map[(byte)'('] = 1;
        map[(byte)')'] = 1;
        map[(byte)';'] = 1;
        map[(byte)':'] = 1;
        map[(byte)'@'] = 1;
        map[(byte)'&'] = 1;
        map[(byte)'='] = 1;
        map[(byte)'+'] = 1;
        map[(byte)'$'] = 1;
        map[(byte)','] = 1;
        map[(byte)'/'] = 1;
        map[(byte)'\\'] = 1;
        map[(byte)'?'] = 1;
        map[(byte)'#'] = 1;
        map[(byte)'['] = 1;
        map[(byte)']'] = 1;
        return map;
    }();
    static const char hexadecimal[] = "0123456789ABCDEF";
    QByteArray ba = plainText.toUtf8();
    QByteArray dest;
    const byte *data = reinterpret_cast<const byte*>(ba.constData());
    int length = ba.length();
    for (int i = 0; i < length; i++) {
        if (isEncoded[data[i]]) {
            dest.append('%');
            dest.append(hexadecimal[data[i]/16]);
            dest.append(hexadecimal[data[i]%16]);
        } else {
            dest.append(data[i]);
        }
    }
    return QString::fromUtf8(dest);
}

void HttpServer::sendSocket(QTcpSocket *sock, HttpResponse &res)
{
    QString statusText = statusToText.value(res.statusCode);
    if (res.statusCode >= 300 && res.content.isEmpty()) {
        res.content = QString(errorDocument).arg(statusText).toUtf8();
    }

    res.fillHeaders();
    res.statusLine = QString("%1 %2").arg(httpVersion, statusText);

    QByteArray output;
    output.append(res.statusLine.toUtf8());
    output.append("\r\n");
    QMapIterator<QString,QString> i(res.headers);
    while (i.hasNext()) {
        i.next();
        output.append(QString("%1: %2\r\n").arg(i.key(), i.value()).toUtf8());
    }
    output.append("\r\n");
    output.append(res.content);
    sock->write(output);
    sock->close();
}

void HttpServer::self_newConnection()
{
    while (auto sock = nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead,
                this, [=]() {
            socket_readyRead(sock);
        });
    }
}

void HttpServer::socket_readyRead(QTcpSocket *sock)
{
    QByteArray raw = sock->readAll();
    QStringList header = QString::fromUtf8(raw).split("\r\n");
    if (header.size() < 1) {
        HttpResponse res;
        res.statusCode = HttpResponse::Http400BadRequest;
        sendSocket(sock, res);
        return;
    }
    QStringList firstLine = header.value(0).split(' ');

    HttpRequest req;
    req.method = firstLine.value(0);
    if (req.method != "GET" && req.method != "POST") {
        HttpResponse res;
        res.statusCode = HttpResponse::Http501NotImplemented;
        sendSocket(sock, res);
        return;
    }
    req.url = firstLine.value(1);
    header.pop_front();

    auto splitLine = [](QString line, QMap<QString,QString> &vars) {
        QStringList pairs = line.split('&');
        for (auto &p : pairs) {
            auto p2 = p.split('=');
            vars.insert(p2.value(0), urlDecode(p2.value(1), true));
        }
    };

    if (req.url.contains("?")) {
        QStringList segments = req.url.split('?');
        req.url = segments.value(0);
        segments.pop_front();
        QString line = segments.join('&');
        splitLine(line, req.getVars);
    }

    bool seenHeader = false;
    for (int i = 0; i < header.count(); i++) {
        QString line = header[i];
        if (!seenHeader && line.isEmpty()) {
            // seen the last of the header
            seenHeader = true;
        } else if (!seenHeader && line.contains(": ")) {
            // header line
            QStringList pair = line.split(": ");
            req.headers.insert(pair.value(0), pair.value(1));
        } else if (seenHeader && !line.isEmpty() && req.method == "POST"
                   && req.headers.value("Content-Type") == mimeFormUrlEncoded) {
            splitLine(line, req.postVars);
        }
    }

    HttpResponse res;
    bool foundRoute = false;
    for (auto &route : routeMap) {
        if (req.url.contains(route.first)) {
            route.second(req,res);
            if (!res.fallthrough) {
                foundRoute = true;
                break;
            }
            res.fallthrough = false;
        }
    }
    if (!foundRoute)
        res.statusCode = HttpResponse::Http404NotFound;
    sendSocket(sock, res);
}



MpcHcServer::MpcHcServer(QObject *owner)
    : QObject(owner)
{
    setupWmCommands();
    setupHttp();
}

void MpcHcServer::setDefaultPage(QString file)
{
    defaultPage = file;
}

void MpcHcServer::setEnabled(bool yes)
{
    enabled = yes;
    relisten();
}

void MpcHcServer::setLocalHostOnly(bool yes)
{
    localHostOnly = yes;
    relisten();
}

void MpcHcServer::setTcpPort(uint16_t port)
{
    tcpPort = port;
    relisten();
}

void MpcHcServer::setServeFiles(bool yes)
{
    serveFiles = yes;
}

void MpcHcServer::setWebRoot(QString path)
{
    webRoot = path;
}

void MpcHcServer::relisten()
{
    http.close();
    if (enabled)
        http.listen(localHostOnly ? QHostAddress::LocalHost : QHostAddress::Any, tcpPort);
}

void MpcHcServer::setupWmCommands()
{
    // I'm probably missing some things that we can do
    wmCommands = QList<WmCommand>({
        { 800, "Open File/URL", [&](){} },
        { 806, "Save Image", [&](){} },
        { 807, "Save Image", [&](){} },
        { 808, "Save Thumbnails", [&](){} },
        { 804, "Close", [&](){} },
        { 814, "Properties", [&](){} },
        { 816, "Exit", [&](){} },
        { 887, "Play", [&](){} },
        { 888, "Pause", [&](){} },
        { 890, "Stop", [&](){} },
        { 891, "Frame-step", [&](){} },
        { 892, "Frame-step back", [&](){} },
        { 895, "Increase Rate", [&](){} },
        { 894, "Decreate Rate", [&](){} },
        { 920, "Next File", [&](){} },
        { 919, "Previous File", [&](){} },
        { 975, "Quick add favorite", [&](){} },
        { 937, "Organize Favorites...", [&](){} },
        { 817, "Toggle Caption&amp;Menu", [&](){} },
        { 818, "Toggle Seek Bar", [&](){} },
        { 819, "Toggle Controls", [&](){} },
        { 820, "Toggle Information", [&](){} },
        { 821, "Toggle Statistics", [&](){} },
        { 822, "Toggle Status", [&](){} },
        { 824, "Toggle Playlist Bar", [&](){} },
        { 827, "View Minimal", [&](){} },
        { 828, "View Compact", [&](){} },
        { 829, "View Normal", [&](){} },
        { 830, "Fullscreen", [&](){} },
        { 813, "Zoom 25%", [&](){} },
        { 832, "Zoom 50%", [&](){} },
        { 833, "Zoom 100%", [&](){} },
        { 834, "Zoom 200%", [&](){} },
        { 968, "Zoom Auto Fit", [&](){} },
        { 4900, "Zoom Auto Fit (Larger Only)", [&](){} },
        { 907, "Volume Up", [&](){} },
        { 908, "Volume Down", [&](){} },
        { 908, "Volume Mute", [&](){} },
        { 954, "Next Subtitle Track", [&](){} },
        { 955, "Next Subtitle Tracks", [&](){} },
        { 956, "On/Off Subtitles", [&](){} },
        { 948, "After Playback: Do nothing", [&](){} },
        { 947, "After Playback: Play next file in the folder", [&](){} },
        { 912, "After Playback: Exit", [&](){} },
        { 912, "After Playback: Stand By", [&](){} },
        { 913, "After Playback: Hibernate", [&](){} },
        { 915, "Shutdown", [&](){} },
        { 916, "Log Off", [&](){} },
        { 917, "Lock", [&](){} }
    });
    for (auto &c : wmCommands) {
        wmCommandsById.insert(c.id, c);
    }
}

void MpcHcServer::setupHttp()
{
    http.route("/", [](HttpRequest &req, HttpResponse &res) {
        (void)req;
        res.headers["Server"] = "MPC-HC WebServer";
        res.fallthrough = true;
    });

    http.route("/favicon.ico", [](HttpRequest &req, HttpResponse &res) {
        (void)req;
        res.serveFile(":/images/icon/mpc-qt.svg");
    });

    http.route("/browser.html", [](HttpRequest &req, HttpResponse &res) {

    });

    http.route("/command.html", [this](HttpRequest &req, HttpResponse &res) {
        res.redirect("/index.html");
        QString commandString = req.getVars.value("wm_command", req.postVars.value("wm_command"));
        if (commandString.isEmpty())
            return;
        int command = commandString.toInt();
        if (command > 0 && wmCommandsById.contains(command))
            wmCommandsById.value(command).func();
    });

    http.route("/info.html", [](HttpRequest &req, HttpResponse &res) {

    });

    http.route("/variables.html", [](HttpRequest &req, HttpResponse &res) {

    });

    http.route("/", [this](HttpRequest &req, HttpResponse &res) {
        QString fileRoot = serveFiles ? webRoot : ":/http";
        QString fallback = serveFiles ? defaultPage : "index.html";
        QString servedFile = fileRoot + req.url;
        res.serveFile(servedFile);
        if (res.statusCode == HttpResponse::Http404NotFound) {
            servedFile = webRoot + "/" + fallback;
            res.serveFile(servedFile);
        };
        if (servedFile == ":/http/index.html") {
            QString contents = QString::fromUtf8(res.content);
            QString actionText;
            for (auto &c : wmCommands) {
                QString optionValue = QString::number(c.id);
                actionText.append(QString("<option value=\"%1\">%2</option>\n").arg(optionValue, c.text));
            }
            res.content = contents.arg(actionText).toUtf8();
        }
    });
}
