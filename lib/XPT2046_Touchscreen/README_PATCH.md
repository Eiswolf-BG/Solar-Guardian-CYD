# Lokal gepatchter Fork

Dies ist KEIN von PlatformIO automatisch heruntergeladenes Paket, sondern
eine lokale, im Projekt vendorte Kopie von
[PaulStoffregen/XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
v1.4.0.

**Einzige Änderung:** `Z_THRESHOLD` in `XPT2046_Touchscreen.cpp` von 300
auf 180 gesenkt (Auftrag "Wischgeste empfindlicher", siehe CLAUDE.md).
Der Original-Wert blockiert nicht nur `touched()`, sondern bereits die
X/Y-Rohmessung selbst - es gibt dafür keine öffentliche Setter-API in der
Bibliothek, deshalb dieser direkte Patch statt eines Workarounds im
Projektcode.

Liegt bewusst in `lib/` (nicht in `lib_deps` in `platformio.ini`), damit
PlatformIO diese lokale Version verwendet statt das Original-Paket neu
herunterzuladen und den Patch dabei zu verlieren.
