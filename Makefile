# Local dev shortcuts. Needs GNU make and a POSIX sh (Git for Windows' sh.exe will do).
#   make dev      build the browser emulator, serve http://localhost:8790/
#   make editor   serve the Felucca Editor, http://localhost:8791/editor.html
#   make serve    serve the emulator already in build/emu, no rebuild
#   make PORT=9000 dev        .. on another port
#   make EMU_OPT=-O0 dev      faster compile, slower audio (fine for UI work)
PYTHON ?= python
PORT ?= 8790
EDITOR_PORT ?= 8791

export PYTHON
export EMU_OPT

.PHONY: dev serve editor

dev:
	sh dev/run.sh $(PORT)

serve:
	sh dev/run.sh --no-build $(PORT)

# WebMIDI (what the editor talks to the FM-1 with) works on localhost; use Chrome or Edge.
editor:
	@echo ""
	@echo "  Felucca Editor:  http://localhost:$(EDITOR_PORT)/editor.html"
	@echo "  needs an FM-1 on USB and Chrome or Edge; stop with Ctrl-C"
	@echo ""
	$(PYTHON) -m http.server -d web $(EDITOR_PORT)
