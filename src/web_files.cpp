#include "web_files.h"
#include "sd_storage.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>

namespace WebFiles {

namespace {
    constexpr size_t MAX_TEXT_FILE_SIZE = 16384; // 16 KB, siehe Auftrag
    constexpr size_t MAX_BODY_SIZE = 16384;
    constexpr uint8_t PAGE_SIZE = 100;

    bool hasTextExtension(const String& path) {
        String lower = path; lower.toLowerCase();
        const char* exts[] = {".txt", ".json", ".csv", ".log", ".md", ".cfg", ".ini"};
        for (const char* e : exts) if (lower.endsWith(e)) return true;
        return false;
    }

    bool hasJsonExtension(const String& path) {
        String lower = path; lower.toLowerCase();
        return lower.endsWith(".json");
    }

    String urlDecode(const String& s) {
        String out; out.reserve(s.length());
        for (size_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (c == '%' && i + 2 < s.length()) {
                char hex[3] = { s[i + 1], s[i + 2], 0 };
                out += (char)strtol(hex, nullptr, 16);
                i += 2;
            } else if (c == '+') {
                out += ' ';
            } else {
                out += c;
            }
        }
        return out;
    }

    // Zentrale Pfad-Normalisierung (Auftrag "Pfade werden vorher
    // normalisiert"): URL-Dekodierung, Backslashes -> Slash, ".."/"."-
    // Segmente aufloesen, doppelte Schraegstriche zusammenfassen. Liefert
    // IMMER einen absoluten Pfad ohne abschliessenden Slash (ausser der
    // Wurzel "/"). "/.." ueber die Wurzel hinaus bleibt bei "/" stehen
    // (kein Fehler, einfach nicht weiter nach oben).
    String normalizePath(const String& rawIn) {
        String raw = urlDecode(rawIn);
        raw.replace("\\", "/");
        std::vector<String> segs;
        int i = 0;
        int len = (int)raw.length();
        while (i < len) {
            while (i < len && raw[i] == '/') i++;
            int start = i;
            while (i < len && raw[i] != '/') i++;
            if (i == start) continue;
            String seg = raw.substring(start, i);
            if (seg == ".") continue;
            if (seg == "..") {
                if (!segs.empty()) segs.pop_back();
                continue;
            }
            segs.push_back(seg);
        }
        String out;
        for (auto& s : segs) { out += "/"; out += s; }
        if (out.length() == 0) out = "/";
        return out;
    }

    // /SolarHistory ist NUR fuer AENDERNDE Operationen tabu (Speichern/
    // Loeschen) - zentrale, einzige Pruefung (Auftrag "Daten: Download/
    // Backup"), case-insensitiv (FAT ist nicht case-sensitiv), greift auf
    // den bereits normalisierten Pfad. Lesen/Anzeigen/Herunterladen ist
    // dort (wie ueberall sonst) erlaubt - siehe handleList()/handleRead()/
    // handleDownload(), die isWriteLocked() NICHT aufrufen.
    bool isWriteLocked(const String& normalized) {
        String upper = normalized; upper.toUpperCase();
        String locked = String(Config::SD_HISTORY_DIR); locked.toUpperCase();
        return upper == locked || upper.startsWith(locked + "/");
    }

    // Origin-Grundschutz (Auftrag 5/C.5): ist ein Origin-Header vorhanden,
    // muss er zum eigenen Host passen.
    bool originAllowed(WebServer& server) {
        if (!server.hasHeader("Origin")) return true; // kein Header - kein curl/direkter Zugriff blockiert
        String origin = server.header("Origin");
        String expected = "http://" + WiFi.localIP().toString();
        return origin == expected || origin.startsWith(expected + ":");
    }

    void sendForbidden(WebServer& server, const char* msg) {
        server.send(403, "text/plain", msg);
    }
    void sendBadRequest(WebServer& server, const char* msg) {
        server.send(400, "text/plain", msg);
    }
    void sendNotFound(WebServer& server, const char* msg) {
        server.send(404, "text/plain", msg);
    }

    void handleList(WebServer& server) {
        String rawPath = server.hasArg("path") ? server.arg("path") : "/";
        uint16_t page = server.hasArg("page") ? (uint16_t)server.arg("page").toInt() : 0;
        String path = normalizePath(rawPath);
        // /SolarHistory wird jetzt angezeigt (Auftrag "Daten: Download/
        // Backup") - nur Speichern/Loeschen bleibt dort gesperrt, siehe
        // isWriteLocked(). Die Liste selbst ist ueberall erlaubt.

        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            sendNotFound(server, "not a directory");
            return;
        }

        uint32_t skip = (uint32_t)page * PAGE_SIZE;
        uint32_t idx = 0;
        uint8_t collected = 0;
        bool hasMore = false;

        struct Entry { String name; bool isDir; uint32_t size; bool locked; };
        std::vector<Entry> entries;

        File entry = dir.openNextFile();
        while (entry) {
            String name = entry.name();
            bool isDir = entry.isDirectory();
            uint32_t size = isDir ? 0 : (uint32_t)entry.size();
            entry.close();
            String childNorm = path;
            if (childNorm != "/") childNorm += "/";
            childNorm += name;
            bool locked = isWriteLocked(normalizePath(childNorm));
            if (name.length() > 0) {
                if (idx >= skip && collected < PAGE_SIZE) {
                    entries.push_back({name, isDir, size, locked});
                    collected++;
                } else if (idx >= skip + PAGE_SIZE) {
                    hasMore = true;
                }
                idx++;
            }
            entry = dir.openNextFile();
        }
        dir.close();

        // Ordner zuerst, dann alphabetisch (Auftrag).
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.isDir != b.isDir) return a.isDir;
            return a.name < b.name;
        });

        JsonDocument doc;
        doc["path"] = path;
        doc["hasMore"] = hasMore;
        doc["locked"] = isWriteLocked(path);
        JsonArray arr = doc["entries"].to<JsonArray>();
        for (auto& e : entries) {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = e.name;
            o["dir"] = e.isDir;
            o["size"] = e.size;
            o["locked"] = e.locked;
        }
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // Rekursive, flache Dateiliste eines Ordners (Auftrag "Ordner/Backup
    // herunterladen" - die Browser-seitige ZIP-Funktion braucht die
    // komplette Dateiliste samt relativem Pfad vorab). Ordner selbst
    // werden nicht aufgefuehrt (ZIP-Builder legt sie implizit ueber die
    // Pfade an). Rein lesend, deshalb auch fuer /SolarHistory erlaubt.
    void collectFilesRecursive(const String& dirPath, const String& baseRel, JsonArray& arr, uint16_t& count) {
        constexpr uint16_t MAX_FILES = 500; // Sicherheitsgrenze gegen ausufernde Antworten
        if (count >= MAX_FILES) return;
        File dir = SD.open(dirPath);
        if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
        File entry = dir.openNextFile();
        while (entry && count < MAX_FILES) {
            String name = entry.name();
            bool isDir = entry.isDirectory();
            uint32_t size = isDir ? 0 : (uint32_t)entry.size();
            String childPath = dirPath; if (childPath != "/") childPath += "/"; childPath += name;
            String childRel = baseRel.length() ? (baseRel + "/" + name) : name;
            entry.close();
            if (isDir) {
                collectFilesRecursive(childPath, childRel, arr, count);
            } else if (name.length() > 0) {
                JsonObject o = arr.add<JsonObject>();
                o["path"] = childPath;
                o["rel"] = childRel;
                o["size"] = size;
                count++;
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }

    void handleListTree(WebServer& server) {
        String rawPath = server.hasArg("path") ? server.arg("path") : "/";
        String path = normalizePath(rawPath);
        JsonDocument doc;
        JsonArray arr = doc["files"].to<JsonArray>();
        uint16_t count = 0;
        collectFilesRecursive(path, "", arr, count);
        doc["truncated"] = (count >= 500);
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    void handleRead(WebServer& server) {
        String rawPath = server.hasArg("path") ? server.arg("path") : "";
        String path = normalizePath(rawPath);
        // Lesen ist ueberall erlaubt, auch in /SolarHistory (Auftrag).

        File f = SD.open(path, FILE_READ);
        if (!f || f.isDirectory()) { if (f) f.close(); sendNotFound(server, "not found"); return; }

        size_t fileSize = f.size();
        bool truncated = fileSize > MAX_TEXT_FILE_SIZE;
        size_t readSize = truncated ? MAX_TEXT_FILE_SIZE : fileSize;
        String content;
        content.reserve(readSize);
        uint8_t buf[256];
        size_t remaining = readSize;
        while (remaining > 0) {
            size_t n = f.read(buf, remaining < sizeof(buf) ? remaining : sizeof(buf));
            if (n == 0) break;
            for (size_t i = 0; i < n; i++) content += (char)buf[i];
            remaining -= n;
        }
        f.close();

        bool editable = !truncated && fileSize <= MAX_TEXT_FILE_SIZE;
        JsonDocument doc;
        doc["content"] = content;
        doc["truncated"] = truncated;
        doc["editable"] = editable;
        doc["size"] = (uint32_t)fileSize;
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // Atomar schreiben: erst in eine .tmp-Datei, pruefen, dann die
    // eigentliche Datei ersetzen (Auftrag).
    bool atomicWrite(const String& path, const String& content) {
        String tmp = path + ".webtmp";
        if (SD.exists(tmp)) SD.remove(tmp);
        File wf = SD.open(tmp, FILE_WRITE);
        if (!wf) return false;
        size_t written = wf.print(content);
        wf.close();
        if (written != content.length()) { SD.remove(tmp); return false; }

        File check = SD.open(tmp, FILE_READ);
        bool ok = check && check.size() == content.length();
        if (check) check.close();
        if (!ok) { SD.remove(tmp); return false; }

        if (SD.exists(path)) SD.remove(path);
        if (!SD.rename(tmp, path)) {
            // Fallback ohne rename()-Unterstuetzung: Inhalt nochmal kopieren.
            File rf = SD.open(tmp, FILE_READ);
            File wf2 = SD.open(path, FILE_WRITE);
            bool ok2 = rf && wf2;
            if (ok2) {
                uint8_t buf[256];
                while (rf.available()) {
                    int n = rf.read(buf, sizeof(buf));
                    if (n <= 0) break;
                    wf2.write(buf, (size_t)n);
                }
            }
            if (rf) rf.close();
            if (wf2) wf2.close();
            SD.remove(tmp);
            if (!ok2) return false;
        }
        return true;
    }

    void handleSave(WebServer& server) {
        if (!originAllowed(server)) { sendForbidden(server, "origin"); return; }
        String body = server.arg("plain");
        if (body.length() > MAX_BODY_SIZE) { sendBadRequest(server, "body too large"); return; }

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) { sendBadRequest(server, "invalid request"); return; }
        String rawPath = doc["path"] | "";
        String content = doc["content"] | "";
        if (rawPath.length() == 0) { sendBadRequest(server, "missing path"); return; }
        if (content.length() > MAX_TEXT_FILE_SIZE) { sendBadRequest(server, "content too large"); return; }

        String path = normalizePath(rawPath);
        if (isWriteLocked(path)) { sendForbidden(server, "locked"); return; }
        if (!hasTextExtension(path)) { sendBadRequest(server, "unsupported file type"); return; }

        if (hasJsonExtension(path)) {
            JsonDocument check;
            if (deserializeJson(check, content) != DeserializationError::Ok) {
                sendBadRequest(server, "invalid JSON");
                return;
            }
        }

        if (!atomicWrite(path, content)) { server.send(500, "text/plain", "write failed"); return; }
        server.send(200, "application/json", "{\"ok\":true}");
    }

    void handleDelete(WebServer& server) {
        if (!originAllowed(server)) { sendForbidden(server, "origin"); return; }
        String body = server.arg("plain");
        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) { sendBadRequest(server, "invalid request"); return; }
        String rawPath = doc["path"] | "";
        if (rawPath.length() == 0) { sendBadRequest(server, "missing path"); return; }

        String path = normalizePath(rawPath);
        if (isWriteLocked(path)) { sendForbidden(server, "locked"); return; }

        File f = SD.open(path);
        if (!f) { sendNotFound(server, "not found"); return; }
        bool isDir = f.isDirectory();
        f.close();
        if (isDir) { sendBadRequest(server, "cannot delete a folder"); return; }

        if (!SD.remove(path)) { server.send(500, "text/plain", "delete failed"); return; }
        server.send(200, "application/json", "{\"ok\":true}");
    }

    // Datei herunterladen (Auftrag) - rein lesend, deshalb auch fuer
    // /SolarHistory erlaubt. Streamt in kleinen Stuecken statt die Datei
    // komplett in den RAM zu laden (funktioniert auch fuer grosse/
    // binaere Dateien). Dateiname bleibt ueber Content-Disposition
    // erhalten.
    void handleDownload(WebServer& server) {
        String rawPath = server.hasArg("path") ? server.arg("path") : "";
        String path = normalizePath(rawPath);
        File f = SD.open(path, FILE_READ);
        if (!f || f.isDirectory()) { if (f) f.close(); sendNotFound(server, "not found"); return; }

        int slash = path.lastIndexOf('/');
        String filename = (slash >= 0) ? path.substring(slash + 1) : path;
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        server.setContentLength(f.size());
        server.send(200, "application/octet-stream", "");

        uint8_t buf[2048];
        while (true) {
            size_t n = f.read(buf, sizeof(buf));
            if (n == 0) break;
            server.sendContent_P((const char*)buf, n);
            yield(); // Auftrag: keine lange Schleife ohne yield(), Anzeige/Touch nicht blockieren
        }
        f.close();
    }

    void handleMethodNotAllowed(WebServer& server) {
        server.send(404, "text/plain", "Not found");
    }
}

void registerRoutes(WebServer& server) {
    server.on("/api/files/list", HTTP_GET, [&server]() { handleList(server); });
    server.on("/api/files/listtree", HTTP_GET, [&server]() { handleListTree(server); });
    server.on("/api/files/read", HTTP_GET, [&server]() { handleRead(server); });
    server.on("/api/files/download", HTTP_GET, [&server]() { handleDownload(server); });
    server.on("/api/files/save", HTTP_POST, [&server]() { handleSave(server); });
    server.on("/api/files/delete", HTTP_POST, [&server]() { handleDelete(server); });
}

}
