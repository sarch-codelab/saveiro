"use strict";

(function () {
    const overlay = document.getElementById("bootOverlay");
    const bootSub = document.getElementById("bootSub");
    const statusPill = document.getElementById("statusPill");
    const statusText = document.getElementById("statusText");
    const metaState = document.getElementById("metaState");
    const uptimeEl = document.getElementById("uptime");
    const terminalPanel = document.getElementById("terminalPanel");

    let emulator = null;
    let booted = false;
    let startedAt = 0;
    let timerId = 0;

    function setStatus(text, mode) {
        statusText.textContent = text;
        metaState.textContent = text;
        statusPill.classList.remove("ready", "error");
        if (mode) {
            statusPill.classList.add(mode);
        }
    }

    function hideOverlay() {
        overlay.classList.add("hidden");
    }

    function showOverlay(message) {
        bootSub.textContent = message || "cargando el bootloader y el kernel…";
        overlay.classList.remove("hidden");
    }

    function markBooted() {
        if (booted) {
            return;
        }
        booted = true;
        setStatus("en línea", "ready");
        startedAt = Date.now();
        hideOverlay();
    }

    function startUptime() {
        if (timerId) {
            clearInterval(timerId);
        }
        uptimeEl.textContent = "00:00";
        timerId = setInterval(function () {
            if (!startedAt) {
                return;
            }
            const total = Math.floor((Date.now() - startedAt) / 1000);
            const mm = String(Math.floor(total / 60)).padStart(2, "0");
            const ss = String(total % 60).padStart(2, "0");
            uptimeEl.textContent = mm + ":" + ss;
        }, 1000);
    }

    function fail(message) {
        setStatus("error", "error");
        bootSub.textContent = message;
        hideOverlay();
        showOverlay(message || "no se pudo iniciar el emulador");
        bootSub.textContent = message;
    }

    if (typeof WebAssembly !== "object") {
        fail("este navegador no soporta WebAssembly");
        return;
    }

    if (typeof V86 === "undefined") {
        fail("no se encontró el emulador v86");
        return;
    }

    try {
        emulator = new V86({
            wasm_path: "assets/v86.wasm",
            bios: { url: "assets/seabios.bin" },
            vga_bios: { url: "assets/vgabios.bin" },
            fda: { url: "assets/saveiro.img" },
            screen_container: document.getElementById("screen_container"),
            memory_size: 32 * 1024 * 1024,
            vga_memory_size: 2 * 1024 * 1024,
            scale: 1,
            autostart: true,
            disable_speaker: true,
            disable_mouse: true,
        });
    } catch (error) {
        fail("error al crear la máquina virtual: " + error.message);
        return;
    }

    window.saveiro = emulator;

    emulator.add_listener("emulator-loaded", function () {
        bootSub.textContent = "ejecutando el sector de arranque…";
    });

    emulator.add_listener("download-error", function () {
        fail("no se pudieron descargar los archivos del sistema");
    });

    /* El kernel saluda por el puerto serie: es la señal más fiable de que
     * la shell está lista. */
    emulator.add_listener("serial0-output-byte", function (byte) {
        if (byte === 36 /* '$' */) {
            markBooted();
        }
    });

    /* Red de seguridad por si el puerto serie no responde. */
    setTimeout(markBooted, 7000);

    startUptime();

    function sendCommand(command) {
        if (!emulator || !booted) {
            return;
        }
        emulator.serial0_send(command + "\n");
        terminalPanel.scrollIntoView({ behavior: "smooth", block: "nearest" });
    }

    document.querySelectorAll(".chip").forEach(function (chip) {
        chip.addEventListener("click", function () {
            sendCommand(chip.getAttribute("data-cmd"));
        });
    });

    document.getElementById("btnRestart").addEventListener("click", function () {
        booted = false;
        startedAt = 0;
        setStatus("reiniciando");
        showOverlay("reiniciando la máquina…");
        try {
            emulator.restart();
        } catch (error) {
            window.location.reload();
            return;
        }
        setTimeout(function () {
            setStatus("arrancando");
        }, 600);
        setTimeout(markBooted, 7000);
    });

    document.getElementById("btnFullscreen").addEventListener("click", function () {
        if (document.fullscreenElement) {
            document.exitFullscreen();
        } else if (terminalPanel.requestFullscreen) {
            terminalPanel.requestFullscreen();
        }
    });

    terminalPanel.addEventListener("click", function () {
        window.focus();
    });
})();
