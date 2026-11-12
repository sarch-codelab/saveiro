"use strict";

/* Arranca SaveiroOS dentro de v86 y deja que el escritorio ocupe la pantalla.
 * No hay panel de comandos: todo se usa con el ratón y el teclado dentro de la
 * propia máquina. */
(function () {
    const overlay = document.getElementById("bootOverlay");
    const bootSub = document.getElementById("bootSub");
    const statusText = document.getElementById("statusText");
    const statusDot = document.getElementById("statusDot");
    const screen = document.getElementById("screen");

    let emulator = null;
    let booted = false;

    function setStatus(text, mode) {
        statusText.textContent = text;
        statusDot.classList.remove("ready", "error");
        if (mode) {
            statusDot.classList.add(mode);
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
        hideOverlay();
    }

    function fail(message) {
        setStatus("error", "error");
        showOverlay(message || "no se pudo iniciar la máquina");
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
        });
    } catch (error) {
        fail("error al crear la máquina virtual: " + error.message);
        return;
    }

    window.saveiro = emulator;

    /* el escritorio necesita el adaptador de ratón de v86 */
    emulator.mouse_set_status(true);

    emulator.add_listener("emulator-loaded", function () {
        bootSub.textContent = "ejecutando el sector de arranque…";
    });

    emulator.add_listener("download-error", function () {
        fail("no se pudieron descargar los archivos del sistema");
    });

    /* la shell saluda por el puerto serie: señal de que el escritorio ya corre */
    let seen = "";
    emulator.add_listener("serial0-output-byte", function (byte) {
        seen = (seen + String.fromCharCode(byte)).slice(-64);
        if (seen.indexOf("saveiro:/$") !== -1) {
            markBooted();
        }
    });

    /* red de seguridad */
    setTimeout(markBooted, 9000);

    document.getElementById("btnRestart").addEventListener("click", function () {
        booted = false;
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
        setTimeout(markBooted, 9000);
    });

    document.getElementById("btnFullscreen").addEventListener("click", function () {
        if (document.fullscreenElement) {
            document.exitFullscreen();
        } else if (screen.requestFullscreen) {
            screen.requestFullscreen();
        }
    });

    /* que el teclado llegue siempre a la máquina */
    screen.addEventListener("click", function () {
        window.focus();
    });
})();
