# Market Tracker C++

Un sistema di monitoraggio real-time per azioni e criptovalute scritto in C++.

## Caratteristiche

- Monitoraggio real-time di azioni e crypto
- Aggiornamento automatico ogni 10 secondi
- Interfaccia console colorata
- Grafici ASCII per trend
- Calcolo indicatori tecnici (SMA, RSI, Volatilità)
- Supporto per multiple API (Alpha Vantage, Yahoo Finance)
- Architettura modulare e estensibile

## Dipendenze

- libcurl (per richieste HTTP)
- nlohmann/json (per parsing JSON)
- CMake 3.16+
- Compilatore C++17

## Installazione

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install libcurl4-openssl-dev nlohmann-json3-dev cmake build-essential
```

### macOS
```bash
brew install curl nlohmann-json cmake
```

### Windows (con vcpkg)
```bash
vcpkg install curl nlohmann-json
```

## Compilazione

```bash
mkdir build
cd build
cmake ..
make
```

## Utilizzo

```bash
./MarketTracker
```

## Configurazione

1. Ottieni una chiave API gratuita da Alpha Vantage
2. Modifica il file `src/api/ApiClient.cpp` inserendo la tua chiave
3. Personalizza i simboli da monitorare in `main.cpp`

## Struttura del Progetto

- `src/api/` - Client API e gestione richieste HTTP
- `src/data/` - Gestione e elaborazione dati di mercato
- `src/ui/` - Interfaccia utente console
- `src/utils/` - Utilità e helper functions

## Estensioni Future

- [ ] Interfaccia grafica (Qt/GTK)
- [ ] Database per storico dati
- [ ] Notifiche via email/Telegram
- [ ] Backtesting strategie
- [ ] Web interface
- [ ] Mobile app

## Licenza

MIT License
*/


gpg -c apikey.txt
passphrase cryptoguru2025!