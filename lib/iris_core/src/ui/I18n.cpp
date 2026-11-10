#include "ui/I18n.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace iris {

namespace {

// Whole strings. English text is kept as short as the Spanish, because the grid is 21 columns wide.
constexpr CatalogEntry kCatalog[] = {
    // suggested names (buttons, categories, controls)
    {"Encender", "Power on"}, {"Apagar", "Power off"}, {"Subir vol.", "Vol up"}, {"Bajar vol.", "Vol down"},
    {"Silenciar", "Mute"}, {"Canal +", "Channel +"}, {"Canal -", "Channel -"}, {"Menú", "Menu"},
    {"Aceptar", "Select"}, {"Arriba", "Up"}, {"Abajo", "Down"}, {"Izquierda", "Left"}, {"Derecha", "Right"},
    {"Volver", "Back"}, {"Entrada", "Input"}, {"Inicio", "Home"},
    {"Aire acond.", "A/C"}, {"Luces", "Lights"}, {"Ventilador", "Fan"}, {"Proyector", "Projector"},
    {"Decodificador", "Set-top box"}, {"Varios", "Misc"},
    {"Televisor", "TV set"}, {"Aire", "Air cond."}, {"Tira LED", "LED strip"}, {"Parlante", "Speaker"},
    // capture details
    {"Reconocida", "Recognized"}, {"Parcial", "Partial"}, {"Desconocida", "Unknown"}, {"Inválida", "Invalid"},
    {"Portadora desconocida", "Carrier unknown"}, {"¡Posible ruido!", "Possible noise!"},
    {"¡Captura truncada!", "Capture truncated!"},
    // messages
    {"Guardado desactivado", "Saving disabled"}, {"Memoria insuficiente", "Not enough memory"},
    {"¡No se pudo guardar!", "Could not save!"}, {"Enviando...", "Sending..."}, {"Ocupado, espere", "Busy, wait"},
    {"Botón no encontrado", "Button not found"}, {"Sin señal guardada", "No saved signal"},
    {"Bat n/d", "Bat n/a"}, {"n/d", "n/a"},
    // diagnostics
    {"FALLA", "FAIL"}, {"activo", "active"}, {"detenido", "stopped"}, {"reposo", "idle"}, {"enviando", "sending"},
    {"cancelando", "cancelling"}, {"SÍ", "YES"}, {"Diagnóstico", "Diagnostics"},
    // learn / capture
    {"Captura RAW", "RAW capture"}, {"Esperando señal...", "Waiting for signal..."},
    {"Apunte el control al", "Point the remote at"}, {"receptor y pulse.", "the receiver, press."},
    {"Izq:cancelar", "Left:cancel"}, {"¡Truncada!", "Truncated!"}, {"OK:elegir Izq:salir", "OK:select Left:exit"},
    {"Captura inválida:", "Invalid capture:"}, {"vacía o muy corta.", "empty or too short."},
    {"OK:reintentar Izq:salir", "OK:retry Left:exit"}, {"No llegó ninguna", "No signal arrived"},
    {"señal a tiempo.", "in time."}, {"Falló el receptor IR", "IR receiver failed"},
    {"Preparando...", "Preparing..."}, {"Izq:salir", "Left:exit"},
    {"Guardar", "Save"}, {"Probar envío", "Test send"}, {"Reintentar", "Retry"}, {"Reproducir", "Play"},
    {"Guardar en botón", "Save to button"}, {"Señal guardada", "Signal saved"}, {"Guardar en: control", "Save to: remote"},
    {"Sin captura", "No capture"},
    // manual signal
    {"Señal manual", "Manual signal"}, {"Protocolo", "Protocol"}, {"Dirección", "Address"}, {"Comando", "Command"},
    {"Sí", "Yes"}, {"Vista previa: inválida", "Preview: invalid"},
    // button / remote / category menus
    {"Botón", "Button"}, {"Enviar", "Send"}, {"Aprender señal", "Learn signal"}, {"Crear manual", "Create manual"},
    {"Renombrar", "Rename"}, {"Renombrar botón", "Rename button"}, {"Renombrado", "Renamed"},
    {"Borrar señal", "Delete signal"}, {"¿Borrar señal?", "Delete signal?"}, {"Señal borrada", "Signal deleted"},
    {"Eliminar botón", "Delete button"}, {"¿Eliminar botón?", "Delete button?"}, {"Botón eliminado", "Button deleted"},
    {"Control", "Remote"}, {"Renombrar control", "Rename remote"}, {"Cambiar categoría", "Change category"},
    {"Mover a...", "Move to..."}, {"Movido", "Moved"}, {"Duplicar", "Duplicate"}, {"Nombre de la copia", "Copy name"},
    {"Control duplicado", "Remote duplicated"}, {"Eliminar control", "Delete remote"},
    {"¿Eliminar control?", "Delete remote?"}, {"Control eliminado", "Remote deleted"}, {"vacío", "empty"},
    {"+ Nuevo botón", "+ New button"}, {"Nuevo botón", "New button"}, {"Botón creado", "Button created"},
    {"OK:enviar Der:menú", "OK:send Right:menu"},
    {"Categoría", "Category"}, {"Renombrar categoría", "Rename category"}, {"Renombrada", "Renamed"},
    {"Eliminar categoría", "Delete category"}, {"¿Eliminar categoría?", "Delete category?"},
    {"(debe estar vacía)", "(must be empty)"}, {"Categoría eliminada", "Category deleted"},
    {"+ Nuevo control", "+ New remote"}, {"Nuevo control", "New remote"}, {"Control creado", "Remote created"},
    {"OK:abrir Der:menú", "OK:open Right:menu"}, {"Controles", "Remotes"}, {"+ Nueva categoría", "+ New category"},
    {"Nueva categoría", "New category"}, {"Categoría creada", "Category created"},
    {"Botón destino", "Target button"}, {"Cree un control antes", "Create a remote first"},
    {"Aprender: control", "Learn: remote"}, {"Captura", "Capture"}, {"OK:reproducir RAW", "OK:replay RAW"},
    {"¿Reproducir RAW?", "Replay RAW?"}, {"Se emitirá la captura", "The capture will be"},
    {"con portadora asumida", "sent with an assumed"}, {"de 38 kHz.", "38 kHz carrier."},
    // logs, exploration
    {"(sin registro)", "(no log)"}, {"Registro de eventos", "Event log"}, {"(sin eventos)", "(no events)"},
    {"Historial de arranques", "Boot history"}, {"(sin datos)", "(no data)"}, {"Detenido", "Stopped"},
    {"Inactiva", "Idle"}, {"Pendiente de confirmar", "Awaiting confirm"}, {"Lista", "Ready"}, {"En curso", "Running"},
    {"Detenida", "Stopped"}, {"Completa", "Complete"}, {"FALLÓ", "FAILED"}, {"Exploración", "Exploration"},
    {"El emisor no sabe si", "The emitter can't tell"}, {"el equipo respondió.", "if the device replied."},
    {"Izq/OK:detener", "Left/OK:stop"}, {"Dir desde", "Addr from"}, {"Dir hasta", "Addr to"}, {"Cmd desde", "Cmd from"},
    {"Cmd hasta", "Cmd to"}, {"Máx. envíos", "Max sends"}, {"Pausa (ms)", "Pause (ms)"}, {"Plan inválido", "Invalid plan"},
    {"Revisar e iniciar", "Review and start"}, {"Exploración larga", "Long exploration"},
    {"Solo con equipos propios.", "Only on your own gear."}, {"El emisor no sabe si el", "The emitter can't tell if"},
    {"equipo respondió.", "the device replied."}, {"¿Iniciar?", "Start?"}, {"Ver registro previo", "View previous log"},
    {"Registro exploración", "Exploration log"},
    // tools
    {"Herramientas IR", "IR tools"}, {"Info del receptor", "Receiver info"}, {"Receptor IR", "IR receiver"},
    {"Receptor 1: TSOP38238", "Receiver 1: TSOP38238"}, {"Entrega señal ya", "Outputs a signal"},
    {"demodulada: no se", "already demodulated:"}, {"mide la portadora.", "carrier not measured."},
    {"Receptor 2: apagado", "Receiver 2: off"}, {"Reproducir RAW", "Replay RAW"}, {"Sin capturas aún", "No captures yet"},
    {"Últimas capturas", "Latest captures"}, {"Diagnóstico HW", "HW diagnostics"},
    {"Historial arranques", "Boot history"}, {"pulso", "press"}, {"largo", "long"}, {"repite", "repeat"},
    {"Prueba de botones", "Button test"}, {"Izq largo: salir", "Hold Left: exit"},
    // settings
    {"Configuración", "Settings"}, {"Invertir video", "Invert video"}, {"Espera captura", "Capture wait"},
    {"SD en Controles", "SD in Remotes"}, {"Info del equipo", "Device info"}, {"Almacenamiento", "Storage"}, {"Idioma", "Language"},
    {"¡FLASH NO MONTADA!", "FLASH NOT MOUNTED!"}, {"OK:borrar/formatear", "OK:erase/format"},
    {"¿FORMATEAR FLASH?", "FORMAT FLASH?"}, {"La flash no se pudo leer.", "Flash could not be read."},
    {"Formatear borra todo", "Formatting erases all"}, {"lo guardado en ella.", "data stored in it."},
    {"Flash formateada", "Flash formatted"}, {"¿BORRAR TODO?", "ERASE ALL?"},
    {"Se eliminan todos los", "All remotes and"}, {"controles y señales.", "signals are deleted."},
    {"No se puede deshacer.", "This cannot be undone."}, {"Todo borrado", "All erased"},
    // import
    {"Control importado", "Remote imported"}, {"Guardar en...", "Save in..."}, {"Importar", "Import"},
    {"Sin tarjeta SD", "No SD card"}, {"Archivo demasiado grande", "File too large"},
    {"Versión no soportada", "Unsupported version"}, {"No es un archivo IRDB", "Not an IRDB file"},
    {"No se pudo leer", "Could not read"}, {"OK:importar Izq:atrás", "OK:import Left:back"},
    {"Nada para importar", "Nothing to import"}, {"Importar IRDB", "Import IRDB"},
    {"Sin carpeta /irdb", "No /irdb folder"}, {"(lista incompleta)", "(list incomplete)"},
    {"(carpeta vacía)", "(empty folder)"}, {"» Importar carpeta", "» Import folder"},
    {"» Usar esta carpeta", "» Use this folder"}, {"Crear control", "Create remote"},
    // widgets
    {"OK:Sí  Izq:No", "OK:Yes  Left:No"}, {"Esp.", "Spc"}, {"Borrar", "Del"}, {"Listo", "Done"},
    {"Izq largo:cancelar", "Hold Left:cancel"}, {"Nombre muy largo", "Name too long"},
    {"Escribir otro...", "Type another..."}, {"OK:elegir Izq:atrás", "OK:select Left:back"},
    {"Izq/Der:dígito OK:ok", "Left/Right:digit OK:ok"}, {"Izq:atrás", "Left:back"}, {"OK:cambiar Izq:atrás", "OK:change Left:back"},
    // modes: loop, crazy, Be Gone, listening, SD controls
    {"Guardados", "Saved"}, {"Tarjeta SD", "SD card"}, {"Modo loop", "Loop mode"}, {"Modo loco", "Crazy mode"},
    {"Escuchar", "Listen"}, {"Escuchando", "Listening"}, {"Escuchar de nuevo", "Listen again"}, {"Control SD", "SD remote"},
    {"Elija botones", "Pick buttons"}, {"Marque al menos uno", "Mark at least one"},
    {"con OK en la lista.", "with OK in the list."}, {"Iniciar", "Start"}, {"Pausa", "Pause"},
    {"OK:marcar Izq:atrás", "OK:mark Left:back"}, {"Pausa media", "Average pause"},
    {"Botones al azar,", "Random buttons,"}, {"a veces repetidos.", "sometimes repeated."}, {"Acción", "Action"},
    {"Elegir categoría", "Choose category"}, {"Be Gone: categoría", "Be Gone: category"},
    {"Be Gone: carpeta", "Be Gone: folder"}, {"¿Iniciar Be Gone?", "Start Be Gone?"},
    {"Envía muchos códigos.", "Sends many codes."}, {"Loop: control", "Loop: remote"}, {"Loco: control", "Crazy: remote"},
    {"Terminado", "Finished"}, {"OK:repetir Izq:salir", "OK:repeat Left:exit"},
    {"Pulse botones de un", "Press buttons of a"}, {"control hacia el", "remote toward the"}, {"receptor.", "receiver."},
    {"OK:detener Izq:salir", "OK:stop Left:exit"}, {"OK:ver Der:menú", "OK:view Right:menu"},
    {"Memoria llena", "Memory full"}, {"Detalle", "Details"}, {"Eliminar", "Delete"}, {"Sin lugar", "No room"},
    {"Importar carpeta", "Import folder"}, {"Exportar a SD", "Export to SD"}, {"Exportar todo a SD", "Export all to SD"},
    {"Sin nada que exportar", "Nothing to export"}, {"No se pudo escribir", "Could not write"},
    // errors shown as messages
    {"Dato inválido", "Invalid data"}, {"Valor fuera de rango", "Value out of range"},
    {"Límite alcanzado", "Limit reached"}, {"No encontrado", "Not found"}, {"Ya existe ese nombre", "Name already exists"},
    {"En uso: vacíelo antes", "In use: empty it first"}, {"Datos dañados", "Corrupted data"},
    {"Error al guardar", "Save error"}, {"No disponible ahora", "Not available now"},
};
constexpr size_t kCatalogCount = sizeof kCatalog / sizeof kCatalog[0];

// Text that starts a string ("Botón Power"): the English replaces the start, the rest is looked up as a whole string.
constexpr CatalogEntry kPrefixes[] = {
    {"Botón ", "Button "},       {"Aprender: ", "Learn: "},   {"Destino: ", "Target: "},   {"Dir ", "Addr "},
    {"Portadora ", "Carrier "},  {"Pantalla: ", "Display: "}, {"Receptor: ", "Receiver: "}, {"Emisor: ", "Emitter: "},
    {"Batería: ", "Battery: "},  {"Nivel est.: ", "Est. level: "}, {"Reinicio: ", "Reset: "}, {"Almacén: ", "Storage: "},
    {"Sin guardar: ", "Unsaved: "}, {"Tiempo: ", "Time: "},  {"Estado: ", "State: "},    {"Enviados: ", "Sent: "},
    {"Último: ", "Last: "},      {"Capturas en RAM: ", "Captures in RAM: "}, {"Ult: ", "Last: "},
    {"Categorías: ", "Categories: "}, {"Controles: ", "Remotes: "}, {"Botones: ", "Buttons: "},
    {"Con señal: ", "With signal: "}, {"Sin soporte: ", "Unsupported: "}, {"Inválidos: ", "Invalid: "},
    {"Sin lugar: ", "No room: "}, {"Rango ", "Range "},         {"y sus ", "and its "},
    {"Vista previa: ", "Preview: "},  {"Archivos: ", "Files: "},    {"Importados: ", "Imported: "},
    {"Omitidos: ", "Skipped: "},     {"Capturas: ", "Captures: "}, {"Loco: ", "Crazy: "},
    {"Exportados: ", "Exported: "}, {"Apagar: ", "Power off: "},     {"Encender: ", "Power on: "},
};

// Text that ends a string ("3 botones"): the head is looked up as a whole string.
constexpr CatalogEntry kSuffixes[] = {
    {" botones", " buttons"},  {" bot.", " btn."},   {" pulsos", " pulses"},
    {" (ruido)", " (noise)"},  {" códigos a probar", " codes to try"},
};

struct SortedIndex {
  std::array<uint16_t, kCatalogCount> order{};
  SortedIndex() {
    for (size_t i = 0; i < kCatalogCount; ++i) order[i] = static_cast<uint16_t>(i);
    std::sort(order.begin(), order.end(), [](uint16_t a, uint16_t b) { return std::strcmp(kCatalog[a].es, kCatalog[b].es) < 0; });
  }
};

const char* exact(std::string_view s) {
  static const SortedIndex index;
  size_t lo = 0, hi = kCatalogCount;
  while (lo < hi) {
    const size_t mid = (lo + hi) / 2;
    const char* key = kCatalog[index.order[mid]].es;
    const int cmp = std::string_view(key).compare(s);
    if (cmp == 0) return kCatalog[index.order[mid]].en;
    if (cmp < 0) lo = mid + 1;
    else hi = mid;
  }
  return nullptr;
}

std::string wholeOrSame(std::string_view s) {
  const char* hit = exact(s);
  return hit ? std::string(hit) : std::string(s);
}

bool startsWith(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool endsWith(std::string_view s, std::string_view p) {
  return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

}  // namespace

const char* langName(Lang l) { return l == Lang::Es ? "Español" : "English"; }
Lang nextLang(Lang l) { return l == Lang::En ? Lang::Es : Lang::En; }

std::string translate(Lang lang, std::string_view spanish) {
  if (lang == Lang::Es || spanish.empty()) return std::string(spanish);
  if (const char* hit = exact(spanish)) return hit;
  for (const CatalogEntry& p : kPrefixes) {
    if (startsWith(spanish, p.es)) return std::string(p.en) + wholeOrSame(spanish.substr(std::strlen(p.es)));
  }
  for (const CatalogEntry& s : kSuffixes) {
    if (endsWith(spanish, s.es)) return wholeOrSame(spanish.substr(0, spanish.size() - std::strlen(s.es))) + s.en;
  }
  return std::string(spanish);
}

size_t catalogSize() { return kCatalogCount; }
const CatalogEntry& catalogEntry(size_t index) { return kCatalog[index]; }

}  // namespace iris
